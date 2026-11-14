// ncnn_model — a network in ncnn's files (`.param` text graph, `.bin` weights) as a graph of
// layers over named blobs, for the layer types and parameters `rife_net` runs; anything else is
// refused by name.
#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace opensu::frame_gen::ncnn {

/// A layer's parameters by id: a scalar is one value, an array (ids from -23300 down) its values.
class Params {
  public:
    void set(int id, std::vector<double> values);
    [[nodiscard]] bool has(int id) const;
    [[nodiscard]] int integer(int id, int otherwise) const;
    [[nodiscard]] float real(int id, float otherwise) const;
    /// The array of id `id` (its `-23300 - id` entry); empty when absent.
    [[nodiscard]] std::vector<double> array(int id) const;
    [[nodiscard]] const std::map<int, std::vector<double>>& all() const noexcept {
        return values_;
    }

  private:
    std::map<int, std::vector<double>> values_;
};

struct Layer {
    std::string type;
    std::string name;
    std::vector<int> bottoms;
    std::vector<int> tops;
    Params params;
    /// Convolution and Deconvolution: [out][in][kh][kw]; MemoryData: its constant.
    std::vector<float> weight;
    std::vector<float> bias;
};

struct Model {
    std::vector<Layer> layers;
    std::vector<std::string> blobs;

    /// The index of the blob named `name`; throws when there is none.
    [[nodiscard]] int blob(std::string_view name) const;
};

/// Reads `param` and `bin`; throws `std::runtime_error` naming the file and what it cannot read,
/// a layer type or parameter outside what `rife_net` runs included.
[[nodiscard]] Model load(const std::filesystem::path& param, const std::filesystem::path& bin);

/// The float a binary16 `bits` holds.
[[nodiscard]] float halfToFloat(std::uint16_t bits) noexcept;

} // namespace opensu::frame_gen::ncnn
