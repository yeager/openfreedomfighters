# Modern graphics specification

OpenFreedomFighters keeps one authoritative gameplay simulation, two runtime
profiles, and an optional third presentation layer. `Original` is the compatibility
reference. `Modern` improves rendering while using the retail installation's assets. `Modern+` is a
future optional layer for separately distributed, legally clean replacement
assets. A graphics setting must not change mission logic, collision, AI, damage,
or save-state results.

## Profile contract

| Capability | Original | Modern | Modern+ |
|---|---|---|---|
| Retail geometry and textures | Required | Required | Required as fallback |
| Widescreen and ultrawide | Corrected projection | Native | Native |
| Presentation frame rate | Reference-compatible option | Unlocked where safe | Unlocked where safe |
| Texture filtering | Reference path | Anisotropic and stable mip selection | Same, with replacement maps |
| Anti-aliasing | Reference-compatible | Planned: temporal or high-quality spatial AA | Same, plus optional DLSS 4.5 on supported RTX hardware; future FSR support remains separately capability-gated |
| Lighting and shadows | Reproduced original model | Higher-resolution dynamic path | Optional authored relighting |
| Color output | SDR reference transform | SDR/HDR tone mapping | SDR/HDR tone mapping |
| Effects | Reference particles and blending | Improved particles, water, glass, smoke, and explosions | Optional authored effects |
| Asset detail | Retail | Retail, optionally reconstructed at runtime | Curated HD textures and materials |

Every feature must be independently configurable. Selecting `Original` must not
silently enable a Modern-only effect, while disabling a Modern feature must have
a deterministic fallback rather than removing visible content.

## Modern baseline

The baseline target includes native-resolution rendering, correct aspect ratios,
resolution-independent UI, adjustable field of view, stable mipmapping, up to
16x anisotropic filtering, anti-aliasing, improved shadow filtering and distance,
ambient contact shading, scalable particles, and an explicit SDR/HDR color
pipeline. Camera motion blur, depth of field, film grain, chromatic effects, and
camera shake are optional and default to conservative values.

Texture enhancement must preserve alpha, palette semantics, animation sequences,
and material boundaries. Automated upscaling is never accepted blindly: UI,
faces, signage, foliage, normal maps, and masks require category-specific review.
The retail image remains the fallback whenever an enhanced result is missing or
invalid.

## Modern+ asset contract

Modern+ may add clean replacement textures, normal/roughness maps, enhanced
particles, and authored lighting metadata. Replacement packs are separate from
the engine repository and may not contain transformed retail assets unless their
distribution is independently authorized. Assets are addressed by stable engine
identifiers, versioned, hash-checked, and individually optional. A missing or
outdated pack falls back to Modern without breaking a scene or save.

Geometry replacement is intentionally later than texture and material work. It
must preserve collision proxies, attachment points, animation bindings, culling
bounds, and gameplay silhouettes. Modern+ must remain interoperable with the same
simulation and multiplayer/replay state if those systems are implemented.

## Performance and quality levels

The minimum Modern target is a stable 60 fps at the Steam Deck's native display
resolution. Desktop presets scale shadow resolution and distance, ambient
shading, volumetrics, reflection quality, particle density, and internal render
resolution. Dynamic resolution and temporal upscaling are permitted, but the UI
is composed at output resolution. Shader compilation and derived assets use a
versioned local cache with safe invalidation.

Quality validation uses deterministic camera paths, image comparisons for the
Original reference, GPU captures for pass and resource correctness, and frametime
budgets rather than average FPS alone. Windows, Linux, macOS, and Steam Deck must
render the same material semantics even when their native graphics backends differ.

## Temporal upscaling, DLSS, FSR, and XeSS

The current user-selectable SDL Modern path is spatial-only. It can render
scene content at a fixed user-selected internal scale and linearly scale that
result to the output; UI is composed at output resolution. It does not expose
temporal anti-aliasing, temporal reconstruction, dynamic resolution, or a
portable temporal fallback/resolver. A separate Modern diagnostic-scene path
does execute a bounded portable temporal resolve pass to validate GPU resource
and submission lifetime; it is not a gameplay renderer or an F10 capability.

The renderer-facing temporal contract owns color, depth, motion vectors,
exposure, jitter, reactive-mask, HUD-less inputs and ping-pong history. Its
portable `TemporalResolveBaseline` coordinator rejects missing producer
resources, unwritten motion vectors, invalid extents, overlapping submissions
and discontinuities. A backend may mark that coordinator as submitted only
after its actual resolve pass has been accepted for GPU submission. It will
compose UI afterward at output resolution. The coordinator is not a filter,
does not allocate API resources, and is not an active runtime fallback until a
backend records that pass.

The runtime derives its available upscalers from completed renderer bindings,
not configuration defaults, product names, or detected libraries. A binding must
identify and version its runtime, have a compatible native device, accept the
complete temporal input set, and own a real frame-submission callback. Ambiguous
or incomplete bindings fail closed. The current SDL GPU renderer has no such
binding, so its F10 choices resolve to native rendering rather than claiming a
portable temporal or vendor backend is active.

The existing F10 render-scale control is already portable: scene content renders
to a 50--200% internal SDL GPU target and is linearly scaled to the output,
while UI remains at output resolution. It is a fixed user-selected scale, not
dynamic resolution, temporal reconstruction, or a claim of original-engine
behavior.

Modern+ targets the documented DLSS 4.5 release as an optional NVIDIA RTX backend.
The integration must use NVIDIA's official SDK and redistributable binaries, expose
the supported quality presets, and report the loaded runtime version exactly.
When implemented, capability checks will choose between DLSS 4.5, a portable
temporal path, or spatial/native rendering at runtime. Original mode will not
enable DLSS by default. macOS and non-RTX devices must not lose a quality or
resolution option merely because DLSS is unavailable.

Intel XeSS-SR is the corresponding optional Intel super-resolution backend. Its
F10 selection is retained as intent and currently resolves to the available
spatial/native path until the active renderer can supply the native D3D12 or
Vulkan command objects and temporal inputs required by Intel's SDK. It is not
currently loaded or labeled as active. The [XeSS plan](XESS.md) records the
supported API and packaging boundary.

AMD GPUs use the same currently available spatial/native path as every other
supported adapter. The F10 menu can retain an FSR request, but resolves it to
that path until a verified AMD adapter is loaded. FSR is never a substitute label
for that path. A future optional FSR adapter may
use AMD's official SDK only after its supported APIs, platforms, redistribution
terms, motion-vector and exposure requirements, and image-quality behavior are
verified. Its absence must never remove resolution or quality controls. Temporal
controls must remain unavailable until a real temporal resolver exists. The
[FSR plan](FSR.md) owns that integration boundary.

A future DLSS 5 backend is not a current deliverable. It may replace or supplement
4.5 only after NVIDIA publishes official documentation, an SDK, platform support,
and redistribution terms that fit the project. No speculative API or mislabeled
older implementation belongs in the engine.

DLSS frame generation remains a later, independently selectable feature. It requires
validated depth and motion vectors, a HUD-less color buffer, correct UI separation,
latency controls, and robust frame pacing before it can ship. Generated frames and
upscaling are presentation-only and may not influence input sampling, physics,
mission logic, replay state, or saves. Only official redistributable binaries may
be packaged; no proprietary SDK source or binary belongs in this repository.

## Accessibility and user control

The renderer will provide brightness and HDR calibration, color-vision filters,
reduced flashes, reduced camera motion, effect-density controls, scalable text and
UI, subtitle-background controls, and clear defaults. Accessibility transforms
are applied after gameplay-relevant visibility calculations so they never alter
AI perception or simulation state.

## Current dependency chain

The portable data layer already decodes retail texture formats, mip levels,
palettes, UVs, vertex colors, grouped topology, and 40,071 validated
primitive-to-texture links. The SDL GPU platform now uploads a validated retail
triangle strip, index ranges, and decoded RGBA texture, applies the explicitly
diagnostic GMS object-source transform, and submits textured indexed draws through
Vulkan, Metal, or D3D12. The portable scene path already prepares deduplicated
resources and stable multi-instance commands. Its upcoming SDL presentation
remains a source-only diagnostic scene: it is not evidence for camera behavior,
RMC/RMI world placement, or Original material semantics. Faithful transform
composition, confirmed material semantics, render-state reconstruction, the
Original reference shaders, and then the Modern render graph remain.
A future camera path may reuse the established configurable FOV, near/far,
viewport/aspect, frustum-plane, and screen-conversion behavior. Modern ultrawide
output must not inherit the observed 4:3 compatibility multiplier
unconditionally, and Original camera fidelity remains gated on the unresolved
matrix, handedness, depth, and view conventions in
[CAMERA_EVIDENCE.md](CAMERA_EVIDENCE.md).
A renderer-facing binding table resolves every ordinary primitive to its optional TEX image while
preserving the still-opaque selector flag. It also supplies validated vertex-alpha
classes and GPU-oriented triangle-strip or line-list index buffers with explicit
draw ranges. Unknown fields remain explicitly opaque until corpus evidence and
private executable analysis agree.
