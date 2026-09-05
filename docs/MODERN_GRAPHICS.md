# Modern graphics specification

OpenFreedomFighters keeps one authoritative gameplay simulation and exposes three
presentation targets. `Original` is the compatibility reference. `Modern`
improves rendering while using the retail installation's assets. `Modern+` is a
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
| Anti-aliasing | Reference-compatible | Temporal or high-quality spatial AA | Same, plus optional DLSS 5 on supported RTX hardware |
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
budgets rather than average FPS alone. Linux, macOS, and Steam Deck must render
the same material semantics even when their native graphics backends differ.

## Temporal upscaling and DLSS

Modern always retains a portable native-resolution and temporal anti-aliasing
path. The renderer-facing temporal interface owns color, depth, motion vectors,
exposure, jitter, reactive-mask, and HUD-less inputs; UI is composed afterward at
output resolution. This contract allows quality-equivalent fallbacks on AMD,
Intel, Apple, and Steam Deck hardware without affecting simulation state.

Modern+ targets DLSS 5 as an optional NVIDIA RTX backend once NVIDIA publishes an
official, redistributable SDK under that name. Until then, the implementation may
use the newest supported official DLSS release, but must report its actual version
and must never label it DLSS 5. Capability checks choose between DLSS, the portable
temporal path, or native rendering at runtime. Original mode does not enable DLSS
by default.

DLSS frame generation is a later, independently selectable feature. It requires
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
primitive-to-texture links. The SDL GPU platform and clear-pass lifecycle are
operational; confirmed material semantics, render-state reconstruction, mesh
upload, the Original reference shaders, and then the Modern render graph remain.
A renderer-facing binding table resolves every ordinary primitive to its optional TEX image while
preserving the still-opaque selector flag. It also supplies validated vertex-alpha
classes and GPU-oriented triangle-strip or line-list index buffers with explicit
draw ranges. Unknown fields remain explicitly opaque until corpus evidence and
private executable analysis agree.
