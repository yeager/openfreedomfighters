# Indexed intro picture renderer

`SdlIntroRenderer` consumes expanded picture batches and explicit resolved GPU
state. It uploads the retained intro images once, keyed by their scene catalog
indices. `prepare` copies geometry and draw inputs, allocates pipelines/samplers,
and records one indexed-geometry upload pass. `SdlIntroFrame::draw` binds each
draw's projection, stage uniforms, texture, sampler, raster/depth/blend state,
viewport, scissor and vertex/index buffers in supplied order.

Expanded XYZ already contains object/view transformation. The vertex shader
receives the explicit column-major projection and an identity model matrix.
Packed vertex color is unpacked without repeating the expansion's channel
reduction. No texture replacement, scene admission, camera choice or visibility
decision occurs here.

The caller supplies a compatible single-sample render pass and its clear/load/
store policy. Only linear RGBA8/BGRA8 color targets are admitted. Fog, alpha
testing, later stages, comparison samplers and sampler extension properties are
rejected until supported. Every inherited pipeline field is an explicit input;
zero-initializing a test request is not evidence of original device defaults.

Prepared frames retain texture/shader ownership even if the renderer is released.
The device must outlive both. The caller must complete submission or cancel its
command before destroying the frame; waiting for GPU idle does not resolve an
unsubmitted command. Preparation validates inputs before recording. On failure,
the caller cancels its command instead of continuing a partial frame.

Normal startup now retains this renderer alongside `IntroRuntime`, but does not
yet issue intro draws: original automatic activation and complete inherited state
remain unresolved. Pipelines and geometry are currently allocated per prepared
frame; caching is deferred until the complete startup path works.

## Evidence

The Vulkan GPU test checks a nonidentity perspective projection against an
independent 64-pixel oracle, texture selection, overlapping draw order, scissor
and retained ownership. The second draw uses different geometry and indices:
unused offscreen vertices precede its narrow visible quad. Reusing the first
draw's vertex or index buffer offset now produces different pixels.

A private integration probe loads the owned intro archive into `IntroRuntime`,
uses its legal-picture hierarchy, decoded geometry/textures, bounds, alignment,
submission cache and live color state, then renders 23 picture groups. Readback
shows 286,720 drawn pixels; a subsequent owner-alpha mutation changes those alpha
channels while retaining RGB. Original images and captures stay private.

That probe explicitly supplies Center completion, picture admission, dimensions,
the undefined-Y replacement policy and raster/sampler/depth state. It proves a
source-backed GPU path, not the original first frame's layout, timing or automatic
playback. CI run 34042124079 at commit `f373d3b` also executed the 64-pixel
renderer oracle and all 14 stage-shader cases on Windows/Direct3D 12 and
macOS/Metal. Both backend runs passed without skipping these tests. The later
run 34043926700 at commit `942b2c4` also passed the distinct-offset fixture and
all 14 shader cases on both backends, without skipping them.
