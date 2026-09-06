#version 450

layout(location = 0) in vec4 diffuse;
layout(location = 1) in vec2 tex_coord;
layout(location = 0) out vec4 result_color;
layout(set = 2, binding = 0) uniform sampler2D picture_texture;
layout(std140, set = 3, binding = 0) uniform Stage {
    uvec4 rgb;
    uvec4 alpha;
    vec4 texture_factor;
} stage;

// Stage-zero CURRENT is DIFFUSE. Codes are a private portable shader ABI,
// not serialized game values or Direct3D enum values.
vec4 argument(uint source, vec4 texel) {
    if (source == 0u) return texel;
    if (source == 3u) return stage.texture_factor;
    return diffuse;
}

vec4 operation(uvec4 request, vec4 texel) {
    vec4 first = argument(request.y, texel);
    if (request.x == 0u) return first;
    vec4 second = argument(request.z, texel);
    if (request.x == 1u) return clamp(2.0 * first * second, 0.0, 1.0);
    return clamp(first + second, 0.0, 1.0);
}

void main() {
    if (stage.rgb.x == 3u) {
        result_color = diffuse;
        return;
    }
    vec4 texel = texture(picture_texture, tex_coord);
    result_color = vec4(operation(stage.rgb, texel).rgb,
                        operation(stage.alpha, texel).a);
}
