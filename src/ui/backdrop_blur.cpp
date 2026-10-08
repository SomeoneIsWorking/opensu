#include "backdrop_blur.hpp"

#include <algorithm>
#include <cmath>
#include <string>

#include "lucent/log.h"

namespace iideck::ui {
namespace {

// A Gaussian of standard deviation `sigma` pixels along `direction`, on raylib's default vertex
// stage. The taps are at whole pixels out to three sigma, at most 96.
constexpr const char* gaussianCore = R"(
#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;
uniform float sigma;
uniform vec2 texel;
out vec4 finalColor;

vec4 gaussian() {
    int radius = int(min(ceil(3.0 * sigma), 96.0));
    vec4 sum = texture(texture0, fragTexCoord);
    float weight = 1.0;
    for (int i = 1; i <= radius; ++i) {
        float w = exp(-0.5 * float(i * i) / (sigma * sigma));
        sum += w * (texture(texture0, fragTexCoord + texel * float(i)) +
                    texture(texture0, fragTexCoord - texel * float(i)));
        weight += 2.0 * w;
    }
    return sum / weight;
}
)";

constexpr const char* horizontalMain = R"(
void main() {
    finalColor = gaussian();
}
)";

// The vertical pass also clips to the capsule: a signed distance to a stadium whose ends are
// half-circles, with a pixel of coverage at its edge. gl_FragCoord is bottom-up in a window and in
// a render texture alike, so the frame's height turns it into the y the layout uses.
constexpr const char* verticalMain = R"(
uniform vec4 capsule;
uniform float frameHeight;
uniform float opacity;

void main() {
    vec4 blurred = gaussian();
    vec2 point = vec2(gl_FragCoord.x, frameHeight - gl_FragCoord.y);
    vec2 extent = capsule.zw;
    float radius = extent.y;
    vec2 q = abs(point - capsule.xy) - (extent - vec2(radius));
    float outside = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
    float coverage = clamp(0.5 - outside, 0.0, 1.0);
    finalColor = vec4(blurred.rgb, coverage * opacity);
}
)";

} // namespace

BackdropBlur::~BackdropBlur() {
    if (loaded_) {
        UnloadShader(horizontal_.shader);
        UnloadShader(vertical_.shader);
        UnloadRenderTexture(strip_);
    }
}

bool BackdropBlur::ready() {
    if (loaded_) {
        return true;
    }
    if (failed_) {
        return false;
    }
    const std::string core = gaussianCore;
    horizontal_.shader = LoadShaderFromMemory(nullptr, (core + horizontalMain).c_str());
    vertical_.shader = LoadShaderFromMemory(nullptr, (core + verticalMain).c_str());
    if (!IsShaderValid(horizontal_.shader) || !IsShaderValid(vertical_.shader)) {
        lucent::error("ui", "the backdrop blur's shaders did not compile; glass stays unblurred");
        failed_ = true;
        return false;
    }
    for (Pass* pass : {&horizontal_, &vertical_}) {
        pass->sigma = GetShaderLocation(pass->shader, "sigma");
        pass->texel = GetShaderLocation(pass->shader, "texel");
    }
    vertical_.capsule = GetShaderLocation(vertical_.shader, "capsule");
    vertical_.frameHeight = GetShaderLocation(vertical_.shader, "frameHeight");
    vertical_.opacity = GetShaderLocation(vertical_.shader, "opacity");
    loaded_ = true;
    return true;
}

void BackdropBlur::resize(int width, int height) {
    if (strip_.id != 0 && strip_.texture.width == width && strip_.texture.height == height) {
        return;
    }
    if (strip_.id != 0) {
        UnloadRenderTexture(strip_);
    }
    strip_ = LoadRenderTexture(width, height);
}

void BackdropBlur::prepare(const Texture& scene, const Rect& body, float sigma) {
    prepared_ = false;
    if (sigma <= 0.0f || body.width <= 0.0f || body.height <= 0.0f || !ready()) {
        return;
    }
    // The strip is the capsule widened by nothing and heightened by three sigma each side, which
    // the vertical pass reads past the capsule's own rows.
    const float margin = std::ceil(3.0f * sigma);
    const auto width = static_cast<int>(std::ceil(body.width));
    const auto height = static_cast<int>(std::ceil(body.height + 2.0f * margin));
    resize(width, height);
    stripTop_ = std::floor(body.y - margin);
    sigma_ = sigma;

    const float sceneHeight = static_cast<float>(scene.height);
    const float texel[2] = {1.0f / static_cast<float>(scene.width), 0.0f};
    SetShaderValue(horizontal_.shader, horizontal_.sigma, &sigma, SHADER_UNIFORM_FLOAT);
    SetShaderValue(horizontal_.shader, horizontal_.texel, texel, SHADER_UNIFORM_VEC2);
    BeginTextureMode(strip_);
    ClearBackground(BLANK);
    BeginShaderMode(horizontal_.shader);
    DrawTexturePro(scene,
                   Rectangle{std::floor(body.x),
                             sceneHeight - (stripTop_ + static_cast<float>(height)),
                             static_cast<float>(width), -static_cast<float>(height)},
                   Rectangle{0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)},
                   Vector2{0.0f, 0.0f}, 0.0f, WHITE);
    EndShaderMode();
    EndTextureMode();
    prepared_ = true;
}

void BackdropBlur::draw(const Rect& body, const Placement& placement) const {
    if (!prepared_ || placement.opacity <= 0.0f) {
        return;
    }
    const float texel[2] = {0.0f, 1.0f / static_cast<float>(strip_.texture.height)};
    const float capsule[4] = {body.centreX(), body.centreY(), body.width * 0.5f,
                              body.height * 0.5f};
    const Shader shader = vertical_.shader;
    SetShaderValue(shader, vertical_.sigma, &sigma_, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, vertical_.texel, texel, SHADER_UNIFORM_VEC2);
    SetShaderValue(shader, vertical_.capsule, capsule, SHADER_UNIFORM_VEC4);
    SetShaderValue(shader, vertical_.frameHeight, &placement.frameHeight, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, vertical_.opacity, &placement.opacity, SHADER_UNIFORM_FLOAT);
    BeginShaderMode(shader);
    const auto width = static_cast<float>(strip_.texture.width);
    const auto height = static_cast<float>(strip_.texture.height);
    DrawTexturePro(strip_.texture, Rectangle{0.0f, 0.0f, width, -height},
                   Rectangle{std::floor(body.x), stripTop_, width, height}, Vector2{0.0f, 0.0f},
                   0.0f, WHITE);
    EndShaderMode();
}

} // namespace iideck::ui
