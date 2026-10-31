#version 450
#extension GL_GOOGLE_include_directive : require

// Project-owned SDL GPU fragment wrapper around AMD FidelityFX FSR 1 RCAS.
// Upstream implementation: GPUOpen-Effects/FidelityFX-FSR, pinned by
// tools/build_fsr1_shaders.py. AMD's MIT license is in FSR1_UPSTREAM_NOTICE.txt.

layout(location = 0) out vec4 result_color;
layout(set = 2, binding = 0) uniform sampler2D source_texture;
layout(std140, set = 3, binding = 0) uniform RcasConstants {
    uvec4 con;
} rcas;

#define A_GPU 1
#define A_GLSL 1
#include "ffx_a.h"

#define FSR_RCAS_F 1
AF4 FsrRcasLoadF(ASU2 pixel) {
    ivec2 extent = textureSize(source_texture, 0);
    ivec2 bounded_pixel = clamp(ivec2(pixel), ivec2(0), extent - ivec2(1));
    return texelFetch(source_texture, bounded_pixel, 0);
}
void FsrRcasInputF(inout AF1 r, inout AF1 g, inout AF1 b) {}

#include "ffx_fsr1.h"

void main() {
    AU2 output_pixel = AU2(gl_FragCoord.xy);
    AF3 color;
    FsrRcasF(color.r, color.g, color.b, output_pixel, rcas.con);
    result_color = AF4(color, 1.0);
}
