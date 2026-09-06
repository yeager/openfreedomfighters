# Viewport-bounded GPU clear

`SdlPictureClear` consumes the clear and viewport requests produced by
`PictureViewTransition`. It encodes a dedicated triangle-list pass with
attachment LOAD/STORE, viewport and scissor, leaving pixels outside that rectangle
unchanged. It does not submit the command buffer or admit an intro view.

The clear pipeline disables blending and culling, writes color only when
requested, and uses ALWAYS depth comparison for requested depth writes. Requested
stencil clearing uses REPLACE with the supplied stencil value. Color-only and
depth-only requests keep the other attachment contents. Per-view once-per-frame
guarding stays in `PictureViewTransition`, not in the GPU helper.

The existing pinned SDL_ttf shaders draw an identity-projected quad against a
white texel. This is sufficient for clearing; it is not a substitute for the
picture material/texture-stage shader. Transient upload and GPU buffers cycle
between requests so multiple clears can coexist in one command buffer.

## Current supported domain

- Live, consistent device and attachment metadata; no active pass on entry.
- Single-sample mip-zero/layer-zero RGBA8 or BGRA8 linear UNORM color targets.
- Nonnegative integer origins and positive extents through `2^24`, depth range `[0,1]`.
- A real depth attachment for depth/stencil requests; a stencil-capable format
  when stencil clearing is requested.

sRGB/HDR color encoding, multisampling and other target layouts need separate
support. Invalid inputs reject before command encoding. Later failures preserve
the encoded prefix; the caller must abandon the failed frame. The caller keeps
the device and attachments alive and supplies accurate formats and dimensions.
Subsequent picture passes must bind their own pipeline and resources; logical
renderer tracking does not imply that GPU bindings survived this dedicated pass.

## Test boundary

Synthetic offscreen patterns are used to measure the actual GPU result: seed
pixels, apply bounded and overlapping clears, download and compare every pixel.
These are verification inputs, not substitute game assets or a fabricated intro.
Tests return skip status when no GPU device is available, rather than claiming
GPU correctness from a CPU-only run. Unsupported depth capabilities are reported
separately from the color checks.

All 61 local CTest executables pass without skips. Color tests exercise actual
Vulkan offscreen readback for both RGBA8 and BGRA8 targets.

A separate witness rasterizer verifies depth and stencil through GPU equality
tests, then downloads the resulting white/black color mask. It loads both planes
without writing them and uses independent resources and pipelines. Positive and
negative controls check the witness itself; these are not raw depth downloads.
The 52 probes cover D24S8 and D32S8, inside/outside preservation, independent and
combined clears, overlaps and repeated non-mutating probes. Tested depth values
are 0 and 1; stencil values are 0, 0x35 and 255. The same test joins
`PictureViewTransition` to the GPU helper and checks repeated-frame suppression,
a suppressed clear consuming the frame guard, and clearing on the next frame.
These local results establish Vulkan behavior, not untested backend correctness.
Targeted GCC and ASan/UBSan runs also pass with SDL's offscreen Vulkan backend.
The initial desktop-backend sanitizer run stalled in the external symbolizer and
was terminated; it is not counted as a successful sanitizer run.

Normal startup still needs its admitted view/draw coordinator and picture GPU
executor. Its existing diagnostic full-target clear has not been replaced by an
invented intro view.
