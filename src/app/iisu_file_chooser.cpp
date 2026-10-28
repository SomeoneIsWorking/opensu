#include "iisu_file_chooser.hpp"

#include <optional>
#include <string>
#include <system_error>
#include <utility>

#include "artwork/iisu_pack.hpp"

namespace opensu::app {
namespace {

/// The files the chooser lists: an APK or a zip.
bool packLike(const std::filesystem::path& file) {
    const std::string extension = file.extension().string();
    return extension == ".apk" || extension == ".zip";
}

/// Why `file` cannot serve `need`, or empty.
std::string refusal(const std::filesystem::path& file, IisuFileChooser::Need need) {
    std::string error;
    const std::shared_ptr<const artwork::IisuPack> pack = artwork::IisuPack::open(file, error);
    if (pack == nullptr) {
        return error;
    }
    if (need == IisuFileChooser::Need::Sounds && !pack->hasSounds()) {
        return file.filename().string() +
               " is iiSU's starter pack, which holds no sounds; pick iiSU's release APK";
    }
    return {};
}

} // namespace

void IisuFileChooser::choose(Need need, const std::filesystem::path& known,
                             std::function<void(const std::filesystem::path&)> chosen) {
    std::error_code ec;
    const std::filesystem::path downloads = home_ / "Downloads";
    std::filesystem::path start = std::filesystem::is_directory(downloads, ec) ? downloads : home_;
    if (!known.empty()) {
        start = known.parent_path();
    }
    paths_.begin(PathEditor::Request{
        .title = need == Need::Sounds ? "iiSU's release" : "iiSU's release or starter pack",
        .start = start,
        .clearLabel = {},
        .refusal =
            [need](const std::filesystem::path& file) {
                return refusal(file, need);
            },
        .files = &packLike,
        .done =
            [chosen = std::move(chosen)](const std::optional<std::filesystem::path>& file) {
                if (file) {
                    chosen(*file);
                }
            }});
}

} // namespace opensu::app
