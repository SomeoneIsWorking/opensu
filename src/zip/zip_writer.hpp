// zip_writer — a stored (uncompressed) zip written to a stream, for files built on the device.
// zip64 is refused: one zip holds under 4 GB and 65535 entries.
#pragma once

#include <cstdint>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

namespace opensu::zip {

class Writer {
  public:
    explicit Writer(std::ostream& out) : out_{&out} {
    }

    /// Appends one file; false, with `error`, when the zip would outgrow its format.
    [[nodiscard]] bool add(std::string_view name, std::string_view bytes, std::string& error);
    /// Writes the central directory; false, with `error`, when the stream failed.
    [[nodiscard]] bool finish(std::string& error);

  private:
    struct Written {
        std::string name;
        std::uint32_t crc32{0};
        std::uint32_t size{0};
        std::uint32_t offset{0};
    };

    std::ostream* out_;
    std::vector<Written> written_;
    std::uint64_t position_{0};
};

} // namespace opensu::zip
