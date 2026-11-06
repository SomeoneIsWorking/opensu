#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <charconv>
#include <string_view>
#include <system_error>

#include "activity_call.hpp"
#include "lucent/log.h"
#include "rom_tree.hpp"

namespace opensu::library::roms {

namespace {

namespace fs = std::filesystem;

/// A document as the descriptor its provider opened; a provider's pipe cannot be read at an offset.
class DocumentFile final : public RomFile {
  public:
    explicit DocumentFile(int descriptor) : descriptor_{descriptor} {
    }
    ~DocumentFile() override {
        ::close(descriptor_);
    }
    DocumentFile(const DocumentFile&) = delete;
    DocumentFile& operator=(const DocumentFile&) = delete;
    DocumentFile(DocumentFile&&) = delete;
    DocumentFile& operator=(DocumentFile&&) = delete;

    [[nodiscard]] std::uint64_t size() const override {
        struct stat status{};
        return ::fstat(descriptor_, &status) == 0 ? static_cast<std::uint64_t>(status.st_size) : 0;
    }
    [[nodiscard]] std::optional<std::string> read(std::uint64_t offset, std::size_t size) override {
        std::string bytes(size, '\0');
        std::size_t got = 0;
        while (got < size) {
            const ssize_t read = ::pread(descriptor_, bytes.data() + got, size - got,
                                         static_cast<off_t>(offset + got));
            if (read < 0 && errno == EINTR) {
                continue;
            }
            if (read <= 0) {
                return std::nullopt;
            }
            got += static_cast<std::size_t>(read);
        }
        return bytes;
    }

  private:
    int descriptor_;
};

/// The filesystem, and the folders picked through the system picker by their document URIs.
class DocumentTree final : public RomTree {
  public:
    [[nodiscard]] std::optional<std::vector<RomNode>>
    children(const fs::path& folder) const override {
        if (!folder.string().starts_with("content://")) {
            return files_.children(folder);
        }
        const android::ActivityCall call;
        if (!call.ready()) {
            return std::nullopt;
        }
        auto* rows = static_cast<jobjectArray>(call.env()->CallObjectMethod(
            call.activity(),
            call.method("listDocuments", "(Ljava/lang/String;)[Ljava/lang/String;"),
            call.env()->NewStringUTF(folder.c_str())));
        if (!call.takeException().empty() || rows == nullptr) {
            return std::nullopt;
        }
        std::vector<RomNode> nodes;
        for (const std::string& row : call.strings(rows)) {
            if (std::optional<RomNode> node = parse(row)) {
                nodes.push_back(std::move(*node));
            }
        }
        std::ranges::sort(nodes, {}, &RomNode::name);
        return nodes;
    }

    [[nodiscard]] std::unique_ptr<RomFile> open(const fs::path& file) const override {
        if (!file.string().starts_with("content://")) {
            return files_.open(file);
        }
        const android::ActivityCall call;
        if (!call.ready()) {
            return nullptr;
        }
        const jint descriptor = call.env()->CallIntMethod(
            call.activity(), call.method("openDocument", "(Ljava/lang/String;)I"),
            call.env()->NewStringUTF(file.c_str()));
        if (!call.takeException().empty() || descriptor < 0) {
            return nullptr;
        }
        return std::make_unique<DocumentFile>(descriptor);
    }

    [[nodiscard]] std::string label(const fs::path& folder) const override {
        if (!folder.string().starts_with("content://")) {
            return files_.label(folder);
        }
        const android::ActivityCall call;
        if (!call.ready()) {
            return folder.string();
        }
        auto* name = static_cast<jstring>(call.env()->CallObjectMethod(
            call.activity(), call.method("documentLabel", "(Ljava/lang/String;)Ljava/lang/String;"),
            call.env()->NewStringUTF(folder.c_str())));
        if (!call.takeException().empty() || name == nullptr) {
            return folder.string();
        }
        return call.string(name);
    }

    [[nodiscard]] std::string writeRefusal(const fs::path& folder) const override {
        if (!folder.string().starts_with("content://")) {
            return files_.writeRefusal(folder);
        }
        const android::ActivityCall call;
        if (!call.ready()) {
            return "the app cannot be asked about " + folder.string();
        }
        const jboolean writable = call.env()->CallBooleanMethod(
            call.activity(), call.method("documentWritable", "(Ljava/lang/String;)Z"),
            call.env()->NewStringUTF(folder.c_str()));
        if (!call.takeException().empty() || writable == JNI_FALSE) {
            return "openSU may only read " + label(folder) + "; pick it again to let it write";
        }
        return {};
    }

    void release(const fs::path& folder) const override {
        if (!folder.string().starts_with("content://")) {
            return;
        }
        const android::ActivityCall call;
        if (!call.ready()) {
            lucent::warn("roms", "the app cannot be asked to release {}", folder.string());
            return;
        }
        call.env()->CallVoidMethod(call.activity(),
                                   call.method("releaseDocument", "(Ljava/lang/String;)V"),
                                   call.env()->NewStringUTF(folder.c_str()));
        if (const std::string thrown = call.takeException(); !thrown.empty()) {
            lucent::warn("roms", "the grant on {} stays: {}", folder.string(), thrown);
        }
    }

    [[nodiscard]] bool place(const fs::path& folder, std::string_view relative,
                             const fs::path& staged, std::string& error) const override {
        if (!folder.string().starts_with("content://")) {
            return files_.place(folder, relative, staged, error);
        }
        const android::ActivityCall call;
        if (!call.ready()) {
            error = "the app cannot be asked to write " + folder.string();
            return false;
        }
        auto* refusal = static_cast<jstring>(call.env()->CallObjectMethod(
            call.activity(),
            call.method(
                "placeDocument",
                "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;"),
            call.env()->NewStringUTF(folder.c_str()),
            call.env()->NewStringUTF(std::string{relative}.c_str()),
            call.env()->NewStringUTF(staged.c_str())));
        if (const std::string thrown = call.takeException(); !thrown.empty()) {
            error = thrown;
            return false;
        }
        if (refusal != nullptr) {
            error = call.string(refusal);
            return false;
        }
        std::error_code ec;
        fs::remove(staged, ec);
        return true;
    }

  private:
    /// "d|f TAB size TAB modified TAB name TAB uri", as DocumentTree.java lists a folder.
    static std::optional<RomNode> parse(std::string_view row) {
        const std::size_t first = row.find('\t');
        const std::size_t second = row.find('\t', first + 1);
        const std::size_t third = row.find('\t', second + 1);
        const std::size_t fourth = row.find('\t', third + 1);
        if (first != 1 || second == std::string_view::npos || third == std::string_view::npos ||
            fourth == std::string_view::npos) {
            return std::nullopt;
        }
        RomNode node;
        node.folder = row.front() == 'd';
        node.local = false;
        const std::string_view size = row.substr(first + 1, second - first - 1);
        std::from_chars(size.data(), size.data() + size.size(), node.size);
        const std::string_view modified = row.substr(second + 1, third - second - 1);
        std::from_chars(modified.data(), modified.data() + modified.size(), node.modified);
        node.name = std::string{row.substr(third + 1, fourth - third - 1)};
        node.location = fs::path{std::string{row.substr(fourth + 1)}};
        return node;
    }

    FileTree files_;
};

} // namespace

std::unique_ptr<RomTree> openRomTree() {
    return std::make_unique<DocumentTree>();
}

} // namespace opensu::library::roms
