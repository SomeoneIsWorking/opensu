#include "rife_reference.hpp"

#include <algorithm>
#include <climits>
#include <cmath>
#include <optional>
#include <stdexcept>

namespace opensu::frame_gen::test {
namespace {

using ncnn::Layer;

[[noreturn]] void fail(const Layer& layer, const std::string& what) {
    throw std::runtime_error{layer.name + " (" + layer.type + "): " + what};
}

Blob convolution(const Layer& layer, const Blob& in) {
    const ncnn::Params& p = layer.params;
    const int outc = p.integer(0, 0);
    const int k = p.integer(1, 1);
    const int stride = p.integer(3, 1);
    const int pad = p.integer(4, 0);
    const bool leaky = p.integer(9, 0) == 2;
    const float slope = leaky ? static_cast<float>(p.array(10).at(0)) : 0.0F;
    if (layer.weight.size() != static_cast<std::size_t>(outc) * in.c * k * k) {
        fail(layer, "weights do not match the input's channels");
    }
    const int outw = (in.w + 2 * pad - k) / stride + 1;
    const int outh = (in.h + 2 * pad - k) / stride + 1;
    Blob out = blob(outw, outh, outc);
    for (int o = 0; o < outc; ++o) {
        for (int y = 0; y < outh; ++y) {
            for (int x = 0; x < outw; ++x) {
                float sum = layer.bias.empty() ? 0.0F : layer.bias[o];
                for (int i = 0; i < in.c; ++i) {
                    const float* kernel =
                        &layer.weight[((static_cast<std::size_t>(o) * in.c) + i) * k * k];
                    for (int ky = 0; ky < k; ++ky) {
                        const int iy = y * stride + ky - pad;
                        if (iy < 0 || iy >= in.h) {
                            continue;
                        }
                        for (int kx = 0; kx < k; ++kx) {
                            const int ix = x * stride + kx - pad;
                            if (ix >= 0 && ix < in.w) {
                                sum += in.at(ix, iy, i) * kernel[ky * k + kx];
                            }
                        }
                    }
                }
                out.at(x, y, o) = leaky && sum < 0 ? sum * slope : sum;
            }
        }
    }
    return out;
}

Blob deconvolution(const Layer& layer, const Blob& in) {
    const ncnn::Params& p = layer.params;
    const int outc = p.integer(0, 0);
    const int k = p.integer(1, 1);
    const int stride = p.integer(3, 1);
    const int pad = p.integer(4, 0);
    if (layer.weight.size() != static_cast<std::size_t>(outc) * in.c * k * k) {
        fail(layer, "weights do not match the input's channels");
    }
    const int outw = (in.w - 1) * stride + k - 2 * pad;
    const int outh = (in.h - 1) * stride + k - 2 * pad;
    Blob out = blob(outw, outh, outc);
    for (int o = 0; o < outc; ++o) {
        for (int y = 0; y < outh; ++y) {
            for (int x = 0; x < outw; ++x) {
                float sum = layer.bias.empty() ? 0.0F : layer.bias[o];
                for (int ky = 0; ky < k; ++ky) {
                    const int by = y + pad - ky;
                    if (by < 0 || by % stride != 0 || by / stride >= in.h) {
                        continue;
                    }
                    for (int kx = 0; kx < k; ++kx) {
                        const int bx = x + pad - kx;
                        if (bx < 0 || bx % stride != 0 || bx / stride >= in.w) {
                            continue;
                        }
                        for (int i = 0; i < in.c; ++i) {
                            sum +=
                                in.at(bx / stride, by / stride, i) *
                                layer.weight[(((static_cast<std::size_t>(o) * in.c) + i) * k + ky) *
                                                 k +
                                             kx];
                        }
                    }
                }
                out.at(x, y, o) = sum;
            }
        }
    }
    return out;
}

/// ncnn's bilinear source index and weight of the next one for `d` of `outSize` from `size`.
std::pair<int, float> linear(int d, int size, double scale) {
    float f = static_cast<float>((d + 0.5) * scale - 0.5);
    int s = static_cast<int>(std::floor(f));
    f -= static_cast<float>(s);
    if (s < 0) {
        s = 0;
        f = 0;
    }
    if (s >= size - 1) {
        s = size - 2;
        f = 1;
    }
    return {s, f};
}

Blob interp(const Layer& layer, const Blob& in) {
    const float hScale = layer.params.real(1, 1);
    const float wScale = layer.params.real(2, 1);
    const int outw = static_cast<int>(static_cast<float>(in.w) * wScale);
    const int outh = static_cast<int>(static_cast<float>(in.h) * hScale);
    if (outw == in.w && outh == in.h) {
        return in;
    }
    if (in.w < 2 || in.h < 2) {
        fail(layer, "an input under 2 pixels is not resized");
    }
    const double ws = outw == in.w ? 1.0 : 1.0 / wScale;
    const double hs = outh == in.h ? 1.0 : 1.0 / hScale;
    Blob out = blob(outw, outh, in.c);
    for (int ch = 0; ch < in.c; ++ch) {
        for (int y = 0; y < outh; ++y) {
            const auto [sy, fy] = linear(y, in.h, hs);
            for (int x = 0; x < outw; ++x) {
                const auto [sx, fx] = linear(x, in.w, ws);
                const float top = in.at(sx, sy, ch) * (1 - fx) + in.at(sx + 1, sy, ch) * fx;
                const float bottom =
                    in.at(sx, sy + 1, ch) * (1 - fx) + in.at(sx + 1, sy + 1, ch) * fx;
                out.at(x, y, ch) = top * (1 - fy) + bottom * fy;
            }
        }
    }
    return out;
}

Blob warp(const Blob& image, const Blob& flow) {
    Blob out = blob(image.w, image.h, image.c);
    for (int ch = 0; ch < image.c; ++ch) {
        for (int y = 0; y < image.h; ++y) {
            for (int x = 0; x < image.w; ++x) {
                const float sx = static_cast<float>(x) + flow.at(x, y, 0);
                const float sy = static_cast<float>(y) + flow.at(x, y, 1);
                const int x0 = std::clamp(static_cast<int>(std::floor(sx)), 0, image.w - 1);
                const int y0 = std::clamp(static_cast<int>(std::floor(sy)), 0, image.h - 1);
                const int x1 = std::clamp(static_cast<int>(std::floor(sx)) + 1, 0, image.w - 1);
                const int y1 = std::clamp(static_cast<int>(std::floor(sy)) + 1, 0, image.h - 1);
                const float alpha = sx - static_cast<float>(x0);
                const float beta = sy - static_cast<float>(y0);
                const float top = image.at(x0, y0, ch) * (1 - alpha) + image.at(x1, y0, ch) * alpha;
                const float bottom =
                    image.at(x0, y1, ch) * (1 - alpha) + image.at(x1, y1, ch) * alpha;
                out.at(x, y, ch) = top * (1 - beta) + bottom * beta;
            }
        }
    }
    return out;
}

float binary(int op, float a, float b) {
    switch (op) {
    case 0:
        return a + b;
    case 1:
        return a - b;
    case 2:
        return a * b;
    case 3:
        return a / b;
    case 7:
        return b - a;
    default:
        return b / a;
    }
}

Blob binaryOp(const Layer& layer, const Blob& a, const std::optional<Blob>& b) {
    const int op = layer.params.integer(0, 0);
    if (!b) {
        const float scalar = layer.params.real(2, 0);
        Blob out = a;
        for (float& v : out.data) {
            v = binary(op, v, scalar);
        }
        return out;
    }
    const auto dim = [&](int x, int y) {
        if (x != y && x != 1 && y != 1) {
            fail(layer, "shapes do not broadcast");
        }
        return std::max(x, y);
    };
    Blob out = blob(dim(a.w, b->w), dim(a.h, b->h), dim(a.c, b->c));
    for (int ch = 0; ch < out.c; ++ch) {
        for (int y = 0; y < out.h; ++y) {
            for (int x = 0; x < out.w; ++x) {
                out.at(x, y, ch) =
                    binary(op, a.at(a.w == 1 ? 0 : x, a.h == 1 ? 0 : y, a.c == 1 ? 0 : ch),
                           b->at(b->w == 1 ? 0 : x, b->h == 1 ? 0 : y, b->c == 1 ? 0 : ch));
            }
        }
    }
    return out;
}

Blob crop(const Layer& layer, const Blob& in) {
    int start = static_cast<int>(layer.params.array(9).at(0));
    const double rawEnd = layer.params.array(10).at(0);
    int end = rawEnd >= INT_MAX ? in.c : static_cast<int>(rawEnd);
    start = start < 0 ? start + in.c : start;
    end = end < 0 ? end + in.c : std::min(end, in.c);
    if (start < 0 || end <= start) {
        fail(layer, "an empty channel range");
    }
    Blob out = blob(in.w, in.h, end - start);
    const std::size_t plane = static_cast<std::size_t>(in.w) * in.h;
    std::copy_n(in.data.begin() + static_cast<std::ptrdiff_t>(plane * start), plane * (end - start),
                out.data.begin());
    return out;
}

Blob concat(const Layer& layer, const std::vector<const Blob*>& ins) {
    int channels = 0;
    for (const Blob* in : ins) {
        if (in->w != ins.front()->w || in->h != ins.front()->h) {
            fail(layer, "inputs of different sizes");
        }
        channels += in->c;
    }
    Blob out = blob(ins.front()->w, ins.front()->h, channels);
    auto at = out.data.begin();
    for (const Blob* in : ins) {
        at = std::copy(in->data.begin(), in->data.end(), at);
    }
    return out;
}

Blob eltwise(const Layer& layer, const std::vector<const Blob*>& ins) {
    const std::vector<double> coeffs = layer.params.array(1);
    Blob out = blob(ins.front()->w, ins.front()->h, ins.front()->c);
    for (std::size_t n = 0; n < ins.size(); ++n) {
        if (ins[n]->data.size() != out.data.size()) {
            fail(layer, "inputs of different shapes");
        }
        const auto coeff = coeffs.empty() ? 1.0F : static_cast<float>(coeffs.at(n));
        for (std::size_t i = 0; i < out.data.size(); ++i) {
            out.data[i] += ins[n]->data[i] * coeff;
        }
    }
    return out;
}

Blob pixelShuffle(const Layer& layer, const Blob& in) {
    const int r = layer.params.integer(0, 1);
    Blob out = blob(in.w * r, in.h * r, in.c / (r * r));
    for (int p = 0; p < out.c; ++p) {
        for (int sh = 0; sh < r; ++sh) {
            for (int sw = 0; sw < r; ++sw) {
                const int q = p * r * r + sh * r + sw;
                for (int y = 0; y < in.h; ++y) {
                    for (int x = 0; x < in.w; ++x) {
                        out.at(x * r + sw, y * r + sh, p) = in.at(x, y, q);
                    }
                }
            }
        }
    }
    return out;
}

} // namespace

Blob blob(int w, int h, int c, float fill) {
    return Blob{.w = w,
                .h = h,
                .c = c,
                .data = std::vector<float>(static_cast<std::size_t>(w) * h * c, fill)};
}

std::vector<Blob> run(const ncnn::Model& model, const std::map<std::string, Blob>& inputs) {
    std::vector<std::optional<Blob>> blobs(model.blobs.size());
    for (const auto& [name, value] : inputs) {
        blobs[static_cast<std::size_t>(model.blob(name))] = value;
    }
    for (const Layer& layer : model.layers) {
        std::vector<const Blob*> ins;
        for (const int b : layer.bottoms) {
            const std::optional<Blob>& in = blobs[static_cast<std::size_t>(b)];
            if (!in.has_value()) {
                fail(layer, "input " + model.blobs[static_cast<std::size_t>(b)] + " was not made");
            }
            ins.push_back(&*in);
        }
        const auto top = [&](Blob value) {
            blobs[static_cast<std::size_t>(layer.tops.at(0))] = std::move(value);
        };
        const std::string& type = layer.type;
        if (type == "Input") {
            if (!blobs[static_cast<std::size_t>(layer.tops.at(0))]) {
                fail(layer, "no value given");
            }
        } else if (type == "Split") {
            for (const int t : layer.tops) {
                blobs[static_cast<std::size_t>(t)] = *ins.at(0);
            }
        } else if (type == "MemoryData") {
            const ncnn::Params& p = layer.params;
            Blob value =
                blob(p.integer(0, 1), std::max(p.integer(1, 1), 1), std::max(p.integer(2, 1), 1));
            value.data = layer.weight;
            top(std::move(value));
        } else if (type == "Convolution") {
            top(convolution(layer, *ins.at(0)));
        } else if (type == "Deconvolution") {
            top(deconvolution(layer, *ins.at(0)));
        } else if (type == "Interp") {
            top(interp(layer, *ins.at(0)));
        } else if (type == "rife.Warp") {
            top(warp(*ins.at(0), *ins.at(1)));
        } else if (type == "BinaryOp") {
            top(binaryOp(layer, *ins.at(0),
                         ins.size() > 1 ? std::optional<Blob>{*ins[1]} : std::nullopt));
        } else if (type == "ReLU") {
            Blob out = *ins.at(0);
            const float slope = layer.params.real(0, 0);
            for (float& v : out.data) {
                v = v < 0 ? v * slope : v;
            }
            top(std::move(out));
        } else if (type == "Sigmoid") {
            Blob out = *ins.at(0);
            for (float& v : out.data) {
                v = 1.0F / (1.0F + std::exp(-v));
            }
            top(std::move(out));
        } else if (type == "Crop") {
            top(crop(layer, *ins.at(0)));
        } else if (type == "Concat") {
            top(concat(layer, ins));
        } else if (type == "Eltwise") {
            top(eltwise(layer, ins));
        } else if (type == "PixelShuffle") {
            top(pixelShuffle(layer, *ins.at(0)));
        } else {
            fail(layer, "not run");
        }
    }
    std::vector<Blob> out;
    out.reserve(blobs.size());
    for (std::optional<Blob>& b : blobs) {
        out.push_back(b ? std::move(*b) : Blob{});
    }
    return out;
}

} // namespace opensu::frame_gen::test
