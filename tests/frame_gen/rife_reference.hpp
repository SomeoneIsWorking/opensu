// rife_reference — the RIFE graph run plainly on the CPU, one loop per layer as ncnn's reference
// layers define it: the oracle `rife_net` is checked against.
#pragma once

#include <map>
#include <string>
#include <vector>

#include "frame_gen/ncnn_model.hpp"

namespace opensu::frame_gen::test {

/// A blob: `c` planes of `h` rows of `w` floats.
struct Blob {
    int w{0};
    int h{0};
    int c{0};
    std::vector<float> data;

    [[nodiscard]] float at(int x, int y, int ch) const {
        return data[(static_cast<std::size_t>(ch) * h + y) * w + x];
    }
    float& at(int x, int y, int ch) {
        return data[(static_cast<std::size_t>(ch) * h + y) * w + x];
    }
};

[[nodiscard]] Blob blob(int w, int h, int c, float fill = 0);

/// Every blob of `model` from `inputs` by name; throws on a shape a layer cannot take.
[[nodiscard]] std::vector<Blob> run(const ncnn::Model& model,
                                    const std::map<std::string, Blob>& inputs);

} // namespace opensu::frame_gen::test
