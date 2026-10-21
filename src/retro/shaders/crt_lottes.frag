#version 450

// PUBLIC DOMAIN CRT STYLED SCAN-LINE SHADER by Timothy Lottes, ported from libretro slang-shaders
// crt/shaders/crt-lottes.slang (890a1e95); its parameters come from frame_filter.hpp.

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 color;
layout(binding = 0) uniform sampler2D frame;
layout(push_constant) uniform Push {
    vec2 texels;
    float hardScan;
    float hardPix;
    float warpX;
    float warpY;
    float maskDark;
    float maskLight;
    float scaleInLinearGamma;
    float shadowMask;
    float brightBoost;
    float hardBloomScan;
    float hardBloomPix;
    float bloomAmount;
    float shape;
} params;

float ToLinear1(float c) {
    if (params.scaleInLinearGamma == 0.0) {
        return c;
    }
    return (c <= 0.04045) ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4);
}

vec3 ToLinear(vec3 c) {
    if (params.scaleInLinearGamma == 0.0) {
        return c;
    }
    return vec3(ToLinear1(c.r), ToLinear1(c.g), ToLinear1(c.b));
}

float ToSrgb1(float c) {
    if (params.scaleInLinearGamma == 0.0) {
        return c;
    }
    return (c < 0.0031308 ? c * 12.92 : 1.055 * pow(c, 0.41666) - 0.055);
}

vec3 ToSrgb(vec3 c) {
    if (params.scaleInLinearGamma == 0.0) {
        return c;
    }
    return vec3(ToSrgb1(c.r), ToSrgb1(c.g), ToSrgb1(c.b));
}

// Nearest emulated sample; black off the frame, as RetroArch's border sampler gives.
vec3 Fetch(vec2 pos, vec2 off) {
    vec2 texel = floor(pos * params.texels + off);
    if (any(lessThan(texel, vec2(0.0))) || any(greaterThanEqual(texel, params.texels))) {
        return vec3(0.0);
    }
    return ToLinear(params.brightBoost * texture(frame, (texel + 0.5) / params.texels).rgb);
}

// Distance in emulated pixels to nearest texel.
vec2 Dist(vec2 pos) {
    pos = pos * params.texels;
    return -((pos - floor(pos)) - vec2(0.5));
}

float Gaus(float pos, float scale) {
    return exp2(scale * pow(abs(pos), params.shape));
}

vec3 Horz3(vec2 pos, float off) {
    vec3 b = Fetch(pos, vec2(-1.0, off));
    vec3 c = Fetch(pos, vec2(0.0, off));
    vec3 d = Fetch(pos, vec2(1.0, off));
    float dst = Dist(pos).x;
    float scale = params.hardPix;
    float wb = Gaus(dst - 1.0, scale);
    float wc = Gaus(dst + 0.0, scale);
    float wd = Gaus(dst + 1.0, scale);
    return (b * wb + c * wc + d * wd) / (wb + wc + wd);
}

vec3 Horz5(vec2 pos, float off) {
    vec3 a = Fetch(pos, vec2(-2.0, off));
    vec3 b = Fetch(pos, vec2(-1.0, off));
    vec3 c = Fetch(pos, vec2(0.0, off));
    vec3 d = Fetch(pos, vec2(1.0, off));
    vec3 e = Fetch(pos, vec2(2.0, off));
    float dst = Dist(pos).x;
    float scale = params.hardPix;
    float wa = Gaus(dst - 2.0, scale);
    float wb = Gaus(dst - 1.0, scale);
    float wc = Gaus(dst + 0.0, scale);
    float wd = Gaus(dst + 1.0, scale);
    float we = Gaus(dst + 2.0, scale);
    return (a * wa + b * wb + c * wc + d * wd + e * we) / (wa + wb + wc + wd + we);
}

vec3 Horz7(vec2 pos, float off) {
    vec3 a = Fetch(pos, vec2(-3.0, off));
    vec3 b = Fetch(pos, vec2(-2.0, off));
    vec3 c = Fetch(pos, vec2(-1.0, off));
    vec3 d = Fetch(pos, vec2(0.0, off));
    vec3 e = Fetch(pos, vec2(1.0, off));
    vec3 f = Fetch(pos, vec2(2.0, off));
    vec3 g = Fetch(pos, vec2(3.0, off));
    float dst = Dist(pos).x;
    float scale = params.hardBloomPix;
    float wa = Gaus(dst - 3.0, scale);
    float wb = Gaus(dst - 2.0, scale);
    float wc = Gaus(dst - 1.0, scale);
    float wd = Gaus(dst + 0.0, scale);
    float we = Gaus(dst + 1.0, scale);
    float wf = Gaus(dst + 2.0, scale);
    float wg = Gaus(dst + 3.0, scale);
    return (a * wa + b * wb + c * wc + d * wd + e * we + f * wf + g * wg) /
           (wa + wb + wc + wd + we + wf + wg);
}

float Scan(vec2 pos, float off) {
    float dst = Dist(pos).y;
    return Gaus(dst + off, params.hardScan);
}

float BloomScan(vec2 pos, float off) {
    float dst = Dist(pos).y;
    return Gaus(dst + off, params.hardBloomScan);
}

// The nearest three lines.
vec3 Tri(vec2 pos) {
    vec3 a = Horz3(pos, -1.0);
    vec3 b = Horz5(pos, 0.0);
    vec3 c = Horz3(pos, 1.0);
    float wa = Scan(pos, -1.0);
    float wb = Scan(pos, 0.0);
    float wc = Scan(pos, 1.0);
    return a * wa + b * wb + c * wc;
}

vec3 Bloom(vec2 pos) {
    vec3 a = Horz5(pos, -2.0);
    vec3 b = Horz7(pos, -1.0);
    vec3 c = Horz7(pos, 0.0);
    vec3 d = Horz7(pos, 1.0);
    vec3 e = Horz5(pos, 2.0);
    float wa = BloomScan(pos, -2.0);
    float wb = BloomScan(pos, -1.0);
    float wc = BloomScan(pos, 0.0);
    float wd = BloomScan(pos, 1.0);
    float we = BloomScan(pos, 2.0);
    return a * wa + b * wb + c * wc + d * wd + e * we;
}

vec2 Warp(vec2 pos) {
    pos = pos * 2.0 - 1.0;
    pos *= vec2(1.0 + (pos.y * pos.y) * params.warpX, 1.0 + (pos.x * pos.x) * params.warpY);
    return pos * 0.5 + 0.5;
}

vec3 Mask(vec2 pos) {
    vec3 mask = vec3(params.maskDark, params.maskDark, params.maskDark);
    if (params.shadowMask == 1.0) {
        // Very compressed TV style shadow mask.
        float line = params.maskLight;
        float odd = 0.0;
        if (fract(pos.x * 0.166666666) < 0.5) {
            odd = 1.0;
        }
        if (fract((pos.y + odd) * 0.5) < 0.5) {
            line = params.maskDark;
        }
        pos.x = fract(pos.x * 0.333333333);
        if (pos.x < 0.333) {
            mask.r = params.maskLight;
        } else if (pos.x < 0.666) {
            mask.g = params.maskLight;
        } else {
            mask.b = params.maskLight;
        }
        mask *= line;
    } else if (params.shadowMask == 2.0) {
        // Aperture grille.
        pos.x = fract(pos.x * 0.333333333);
        if (pos.x < 0.333) {
            mask.r = params.maskLight;
        } else if (pos.x < 0.666) {
            mask.g = params.maskLight;
        } else {
            mask.b = params.maskLight;
        }
    } else if (params.shadowMask == 3.0) {
        // Stretched VGA style shadow mask.
        pos.x += pos.y * 3.0;
        pos.x = fract(pos.x * 0.166666666);
        if (pos.x < 0.333) {
            mask.r = params.maskLight;
        } else if (pos.x < 0.666) {
            mask.g = params.maskLight;
        } else {
            mask.b = params.maskLight;
        }
    } else if (params.shadowMask == 4.0) {
        // VGA style shadow mask.
        pos.xy = floor(pos.xy * vec2(1.0, 0.5));
        pos.x += pos.y * 3.0;
        pos.x = fract(pos.x * 0.166666666);
        if (pos.x < 0.333) {
            mask.r = params.maskLight;
        } else if (pos.x < 0.666) {
            mask.g = params.maskLight;
        } else {
            mask.b = params.maskLight;
        }
    }
    return mask;
}

void main() {
    vec2 pos = Warp(uv);
    vec3 outColor = Tri(pos);
    outColor += Bloom(pos) * params.bloomAmount;
    if (params.shadowMask > 0.0) {
        // The mask runs on target pixels, as the original's on the pass's output pixels.
        outColor *= Mask(gl_FragCoord.xy * 1.000001);
    }
    color = vec4(ToSrgb(outColor), 1.0);
}
