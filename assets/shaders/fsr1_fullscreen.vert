#version 450

// Project-owned no-input full-screen triangle for the FSR 1 graphics passes.
// The fragment shaders use gl_FragCoord and therefore need no varying input.

void main() {
    vec2 position = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(position * 2.0 - 1.0, 0.0, 1.0);
}
