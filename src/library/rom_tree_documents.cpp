#include <algorithm>
#include <charconv>
#include <string_view>
#include <system_error>

#include "activity_call.hpp"
#include "lucent/log.h"
#include "rom_tree.hpp"

namespace opensu::library::roms {

namespace {

namespace fs = std::filesystem;

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
    /// "d|f TAB size TAB name TAB uri", as DocumentTree.java lists a folder.
    static std::optional<RomNode> parse(std::string_view row) {
        const std::size_t first = row.find('\t');
        const std::size_t second = row.find('\t', first + 1);
        const std::size_t third = row.find('\t', second + 1);
        if (first != 1 || second == std::string_view::npos || third == std::string_view::npos) {
            return std::nullopt;
        }
        RomNode node;
        node.folder = row.front() == 'd';
        node.local = false;
        const std::string_view size = row.substr(first + 1, second - first - 1);
        std::from_chars(size.data(), size.data() + size.size(), node.size);
        node.name = std::string{row.substr(second + 1, third - second - 1)};
        node.location = fs::path{std::string{row.substr(third + 1)}};
        return node;
    }

    FileTree files_;
};

} // namespace

std::unique_ptr<RomTree> openRomTree() {
    return std::make_unique<DocumentTree>();
}

} // namespace opensu::library::roms
