#version 450
#extension GL_GOOGLE_include_directive : require

// Project-owned SDL GPU fragment wrapper around AMD FidelityFX FSR 1 EASU.
// Upstream implementation: GPUOpen-Effects/FidelityFX-FSR, pinned by
// tools/build_fsr1_shaders.py. AMD's MIT license is in FSR1_UPSTREAM_NOTICE.txt.

layout(location = 0) out vec4 result_color;
layout(set = 2, binding = 0) uniform sampler2D source_texture;
layout(std140, set = 3, binding = 0) uniform EasUConstants {
    uvec4 con0;
    uvec4 con1;
    uvec4 con2;
    uvec4 con3;
} easu;

#define A_GPU 1
#define A_GLSL 1
#include "ffx_a.h"

#define FSR_EASU_F 1
AF4 FsrEasuRF(AF2 p) { return textureGather(source_texture, p, 0); }
AF4 FsrEasuGF(AF2 p) { return textureGather(source_texture, p, 1); }
AF4 FsrEasuBF(AF2 p) { return textureGather(source_texture, p, 2); }

#include "ffx_fsr1.h"

void main() {
    AU2 output_pixel = AU2(gl_FragCoord.xy);
    AF3 color;
    FsrEasuF(color, output_pixel, easu.con0, easu.con1, easu.con2, easu.con3);
    result_color = AF4(color, 1.0);
}
