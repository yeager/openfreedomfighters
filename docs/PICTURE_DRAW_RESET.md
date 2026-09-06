# Ordinary picture draw reset

`PictureDrawReset` implements the ordered reset required before each ordinary
draw-loop invocation, including exhausted and barrier-resumed rounds. Its
`PictureDrawContext` is shared with diagnostics, view transitions and material
requests. It is tracked state, not a complete description of actual GPU state.

The effective texture-stage count is an explicit input: the original obtains
`MaxTextureBlendStages` from adapter capabilities, with a configuration override
to one. It is not the simultaneous-texture count. Native bounds policy accepts
zero through eight without clamping. The actual intro count is not assumed.

## Boundaries

1. Reset tracked blend/depth/alpha/light/cull/fog values, then supported texture
   stages, both stream pointers/strides, indices, vertex format and pixel shader.
2. Submit all backend reset commands unconditionally in their reviewed order.
3. Only after submission returns, set material cache to `(0xffffffff, 0, 0)` and
   zero projection, world and view cache blocks, in that order.
4. Store effective feature bits and capture them for the diagnostic. Call its
   service getter, then the returned diagnostic callback.
5. If captured features were zero, apply stage-zero texture-factor RGB/alpha
   selection through difference checks and request wireframe fill. Then disable
   lighting only if its current tracked value is nonzero.

Backend submission sees reset tracked state but the old material/matrix caches.
The diagnostic sees completed cache clearing and may mutate the shared context.
Fallback selection uses the captured pre-callback feature bits; its stage setters
and lighting correction inspect current post-callback tracking.

## Backend command order

The scalar prefix is source ONE, destination ZERO, blend false, depth-write true,
depth LESS_EQUAL, alpha comparison ALWAYS, alpha test false, fog true, fog color
zero, fog start bits `0x3f333333`, fog end bits `0x3f800000`, lighting false and
clockwise cull.

Each supported stage then receives null texture, RGB DISABLE/TEXTURE/CURRENT,
and alpha DISABLE/TEXTURE/CURRENT, finishing one stage before the next. The tail
is null stream zero with stride zero, null indices with the **retained** base
vertex, vertex-format word `0x142`, and null pixel shader.

Commands use semantic operations and typed payloads, not original executable
code. A backend must translate them to its own shader/pipeline/buffer behavior.
Failed submission is not treated as success: exceptions propagate with the
executed prefix, and the caller must abort the frame.

## What is not reset

Index base vertex, both feature masks, material mode and material suppression
survive. Fog suppression and depth-write suppression clear separately. Stages
beyond the explicit count retain their tracked values.

Stream-one tracking clears without a stream-one GPU unbind. Matrix caches become
zero bytes without GPU transform commands. No requests establish depth-test
enable, blend operation, fog vertex/table mode, sampler filtering/addressing,
anisotropy, texture factor, coordinate index/transform enable, clipping,
viewport, actual matrices or solid fill on nonzero-feature paths.

The context's fog is passed directly to `PictureViewTransition`, and its
`fog.colors` to the material planner. Reset does not touch a view's clear guard.
Normal intro admission, the outer drawing coordinator and the real GPU executor
are still required; this reset does not itself produce an intro frame.

## Verification

All 59 local CTest executables pass. The reset test also passes with GCC and
ASan/UBSan. Independent fixtures cover command order and payloads, stage counts,
old-cache visibility during submission, diagnostic mutations, retained inputs,
failure prefixes and repeated/exhausted ordered-loop invocations.

The private owned-data probe connects reset, ordered intro records and the real
camera's view transition through one shared context. Its stage count and backend
conditions are explicit test inputs, not measured first-frame capabilities.
