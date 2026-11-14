#include "frame_gen/ncnn_model.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <initializer_list>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <utility>

#include <lucent/text.h>

#include "fileio/little_endian.hpp"
#include "fileio/read_whole.hpp"

namespace opensu::frame_gen::ncnn {
namespace {

/// The first line of every ncnn `.param` file.
inline constexpr int paramMagic = 7767517;
/// A parameter id at or below this is the array of id `arrayBase - id`.
inline constexpr int arrayBase = -23300;
/// The tag before half-precision weights in a `.bin`; a zero tag is plain floats.
inline constexpr std::uint32_t halfTag = 0x01306B47;

/// The parameter ids each layer type is run with; any other id is refused.
struct LayerType {
    std::string_view name;
    std::initializer_list<int> params;
};

inline constexpr std::array<LayerType, 14> supportedTypes{{
    {"Input", {}},
    {"Split", {}},
    {"MemoryData", {0, 1, 2}},
    {"Crop", {9, 10, 11}},
    {"BinaryOp", {0, 1, 2}},
    {"ReLU", {0}},
    {"Sigmoid", {}},
    {"Concat", {0}},
    {"Eltwise", {0, 1}},
    {"PixelShuffle", {0}},
    {"Interp", {0, 1, 2}},
    {"Convolution", {0, 1, 3, 4, 5, 6, 9, 10}},
    {"Deconvolution", {0, 1, 3, 4, 5, 6}},
    {"rife.Warp", {0}},
}};

double number(std::string_view text, const std::string& where) {
    double value = 0;
    const auto [end, error] =
        lucent::text::from_chars_real(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size()) {
        throw std::runtime_error{where + ": not a number: " + std::string{text}};
    }
    return value;
}

std::vector<std::string> words(const std::string& line) {
    std::vector<std::string> out;
    std::istringstream in{line};
    for (std::string word; in >> word;) {
        out.push_back(std::move(word));
    }
    return out;
}

void check(const Layer& layer, const std::string& where) {
    const auto* type = std::ranges::find(supportedTypes, layer.type, &LayerType::name);
    if (type == supportedTypes.end()) {
        throw std::runtime_error{where + ": layer type " + layer.type + " is not run"};
    }
    for (const auto& [id, values] : layer.params.all()) {
        if (std::ranges::find(type->params, id) == type->params.end()) {
            throw std::runtime_error{where + ": " + layer.type + " parameter " +
                                     std::to_string(id) + " is not run"};
        }
    }
    const auto require = [&](bool ok, const char* what) {
        if (!ok) {
            throw std::runtime_error{where + ": " + layer.type + " " + what + " is not run"};
        }
    };
    const Params& p = layer.params;
    if (layer.type == "BinaryOp") {
        const int op = p.integer(0, 0);
        require(op == 0 || op == 1 || op == 2 || op == 3 || op == 7 || op == 8, "operation");
    } else if (layer.type == "Concat") {
        require(p.integer(0, 0) == 0, "axis other than channels");
    } else if (layer.type == "Eltwise") {
        require(p.integer(0, 0) == 1, "operation other than sum");
    } else if (layer.type == "Interp") {
        require(p.integer(0, 0) == 2, "resize other than bilinear");
    } else if (layer.type == "Crop") {
        for (const double axis : p.array(11)) {
            require(axis == 0, "axis other than channels");
        }
    } else if (layer.type == "Convolution") {
        const int activation = p.integer(9, 0);
        require(activation == 0 || activation == 2, "activation");
    }
}

class Weights {
  public:
    Weights(std::string bytes, std::string where)
        : bytes_{std::move(bytes)}, where_{std::move(where)} {
    }

    /// `count` values stored with a tag (ncnn's type 0): half or plain floats.
    std::vector<float> tagged(std::size_t count) {
        const auto tag = static_cast<std::uint32_t>(fileio::littleEndian(take(4), 0, 4));
        if (tag == halfTag) {
            const std::string_view raw = take((count * 2 + 3) / 4 * 4);
            std::vector<float> out(count);
            for (std::size_t i = 0; i < count; ++i) {
                out[i] =
                    halfToFloat(static_cast<std::uint16_t>(fileio::littleEndian(raw, i * 2, 2)));
            }
            return out;
        }
        if (tag == 0) {
            return floats(count);
        }
        throw std::runtime_error{where_ + ": weights stored as tag " + std::to_string(tag) +
                                 " are not read"};
    }

    /// `count` plain floats (ncnn's type 1).
    std::vector<float> floats(std::size_t count) {
        const std::string_view raw = take(count * 4);
        std::vector<float> out(count);
        for (std::size_t i = 0; i < count; ++i) {
            out[i] = std::bit_cast<float>(
                static_cast<std::uint32_t>(fileio::littleEndian(raw, i * 4, 4)));
        }
        return out;
    }

    void finish() const {
        if (at_ != bytes_.size()) {
            throw std::runtime_error{where_ + ": " + std::to_string(bytes_.size() - at_) +
                                     " bytes left after the last layer"};
        }
    }

  private:
    std::string_view take(std::size_t count) {
        if (bytes_.size() - at_ < count) {
            throw std::runtime_error{where_ + ": ends before the weights do"};
        }
        const std::string_view out{bytes_.data() + at_, count};
        at_ += count;
        return out;
    }

    std::string bytes_;
    std::string where_;
    std::size_t at_{0};
};

void loadWeights(Layer& layer, Weights& weights) {
    const Params& p = layer.params;
    if (layer.type == "Convolution" || layer.type == "Deconvolution") {
        layer.weight = weights.tagged(static_cast<std::size_t>(p.integer(6, 0)));
        if (p.integer(5, 0) != 0) {
            layer.bias = weights.floats(static_cast<std::size_t>(p.integer(0, 0)));
        }
    } else if (layer.type == "MemoryData") {
        const auto size = static_cast<std::size_t>(p.integer(0, 0)) *
                          static_cast<std::size_t>(std::max(p.integer(1, 1), 1)) *
                          static_cast<std::size_t>(std::max(p.integer(2, 1), 1));
        layer.weight = weights.floats(size);
    }
}

} // namespace

void Params::set(int id, std::vector<double> values) {
    values_[id] = std::move(values);
}

bool Params::has(int id) const {
    return values_.contains(id);
}

int Params::integer(int id, int otherwise) const {
    const auto found = values_.find(id);
    return found == values_.end() ? otherwise : static_cast<int>(found->second.front());
}

float Params::real(int id, float otherwise) const {
    const auto found = values_.find(id);
    return found == values_.end() ? otherwise : static_cast<float>(found->second.front());
}

std::vector<double> Params::array(int id) const {
    const auto found = values_.find(id);
    return found == values_.end() ? std::vector<double>{} : found->second;
}

int Model::blob(std::string_view name) const {
    for (std::size_t i = 0; i < blobs.size(); ++i) {
        if (blobs[i] == name) {
            return static_cast<int>(i);
        }
    }
    throw std::runtime_error{"the model has no blob " + std::string{name}};
}

Model load(const std::filesystem::path& param, const std::filesystem::path& bin) {
    std::string error;
    const std::optional<std::string> text = fileio::readWhole(param, error);
    if (!text) {
        throw std::runtime_error{param.string() + ": " + error};
    }
    std::optional<std::string> bytes = fileio::readWhole(bin, error);
    if (!bytes) {
        throw std::runtime_error{bin.string() + ": " + error};
    }
    Weights weights{std::move(*bytes), bin.string()};

    std::istringstream lines{*text};
    std::string line;
    std::getline(lines, line);
    if (words(line) != std::vector<std::string>{std::to_string(paramMagic)}) {
        throw std::runtime_error{param.string() + ": not an ncnn param file"};
    }
    std::getline(lines, line);
    const std::vector<std::string> counts = words(line);
    if (counts.size() != 2) {
        throw std::runtime_error{param.string() + ": no layer and blob counts"};
    }

    Model model;
    std::unordered_map<std::string, int> blobIndex;
    const auto blobOf = [&](const std::string& name) {
        const auto [at, added] = blobIndex.try_emplace(name, static_cast<int>(model.blobs.size()));
        if (added) {
            model.blobs.push_back(name);
        }
        return at->second;
    };
    const auto layerCount = static_cast<std::size_t>(number(counts[0], param.string()));
    for (std::size_t i = 0; i < layerCount; ++i) {
        if (!std::getline(lines, line)) {
            throw std::runtime_error{param.string() + ": ends before its layers do"};
        }
        const std::vector<std::string> w = words(line);
        const std::string where = param.string() + " line " + std::to_string(i + 3);
        if (w.size() < 4) {
            throw std::runtime_error{where + ": too short"};
        }
        Layer layer;
        layer.type = w[0];
        layer.name = w[1];
        const auto bottoms = static_cast<std::size_t>(number(w[2], where));
        const auto tops = static_cast<std::size_t>(number(w[3], where));
        if (w.size() < 4 + bottoms + tops) {
            throw std::runtime_error{where + ": fewer blobs than counted"};
        }
        for (std::size_t b = 0; b < bottoms; ++b) {
            layer.bottoms.push_back(blobOf(w[4 + b]));
        }
        for (std::size_t t = 0; t < tops; ++t) {
            layer.tops.push_back(blobOf(w[4 + bottoms + t]));
        }
        for (std::size_t k = 4 + bottoms + tops; k < w.size(); ++k) {
            const std::size_t equals = w[k].find('=');
            if (equals == std::string::npos) {
                throw std::runtime_error{where + ": parameter without a value: " + w[k]};
            }
            const int id =
                static_cast<int>(number(std::string_view{w[k]}.substr(0, equals), where));
            std::string_view value = std::string_view{w[k]}.substr(equals + 1);
            std::vector<double> values;
            if (id <= arrayBase) {
                const std::size_t comma = value.find(',');
                const auto count = static_cast<std::size_t>(number(value.substr(0, comma), where));
                value =
                    comma == std::string_view::npos ? std::string_view{} : value.substr(comma + 1);
                while (!value.empty()) {
                    const std::size_t next = value.find(',');
                    values.push_back(number(value.substr(0, next), where));
                    value = next == std::string_view::npos ? std::string_view{}
                                                           : value.substr(next + 1);
                }
                if (values.size() != count) {
                    throw std::runtime_error{where + ": array " + std::to_string(id) +
                                             " has a wrong count"};
                }
                layer.params.set(arrayBase - id, std::move(values));
            } else {
                layer.params.set(id, {number(value, where)});
            }
        }
        check(layer, where);
        loadWeights(layer, weights);
        model.layers.push_back(std::move(layer));
    }
    weights.finish();
    return model;
}

float halfToFloat(std::uint16_t bits) noexcept {
    const std::uint32_t sign = (bits >> 15U) & 1U;
    const std::uint32_t exponent = (bits >> 10U) & 0x1FU;
    const std::uint32_t mantissa = bits & 0x3FFU;
    float magnitude = 0;
    if (exponent == 0) {
        magnitude = std::ldexp(static_cast<float>(mantissa), -24);
    } else if (exponent == 0x1F) {
        magnitude = mantissa == 0 ? INFINITY : NAN;
    } else {
        magnitude =
            std::ldexp(static_cast<float>(mantissa | 0x400U), static_cast<int>(exponent) - 25);
    }
    return sign != 0 ? -magnitude : magnitude;
}

} // namespace opensu::frame_gen::ncnn
