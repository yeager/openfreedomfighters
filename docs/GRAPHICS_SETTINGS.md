# Graphics settings overlay

F10 opens the in-game graphics-settings overlay. It is a renderer overlay rather
than a separate native dialog, so the same interaction and presentation work on
Windows, macOS, Linux, and Steam Deck. The scene remains visible behind it and
gameplay input is consumed while it is open.

The overlay is the single supported in-game entry point for display, rendering,
quality, and advanced presentation controls. It is not a developer console and
must remain usable when an optional renderer feature fails to initialize.

## Original visual-language requirement

The shipping overlay must look and behave like an extension of the game's own
menus, not a generic desktop settings window. Layout hierarchy, panel placement,
spacing, focus treatment, selector treatment, colors, animation timing, and audio
cues must be recovered as behavior-only measurements from a legally owned retail
copy. At runtime it uses the applicable menu resources from that verified
installation whenever they are present and decoded. Modern-only rows may extend
the menu, but must use the same recovered visual grammar. Accessibility overrides
may deliberately change contrast, scale, motion, or input feedback.

No retail font, interface texture, string, screenshot, or sampled audio is copied
into this repository. Public tests use project-authored geometry and text only.
The temporary diagnostic skin is permitted solely as a labelled implementation
aid; it is not the design reference and cannot satisfy the final UI acceptance
gate. If a required retail menu resource cannot yet be decoded, the production
path reports that limitation instead of silently replacing it with synthetic
content. See [Game-data policy](../DATA_POLICY.md).

## Interaction contract

- F10 opens the overlay and copies the confirmed requested settings into an
  editable draft. F10 again or Escape cancels unconfirmed edits and closes it.
- Arrow keys navigate and edit rows. Enter or Space activates a row. Controller
  D-pad, A, and B provide the same operations; a controller menu shortcut will be
  finalized with the input system.
- Focus is always visible. Disabled rows remain readable, cannot receive an
  actionable focus state, and include a concise reason such as `Requires Modern+`
  or `DLSS runtime unavailable`.
- Escape cancels the overlay when it is open and must not also quit the game.
- Apply is transactional. Validation and runtime reconfiguration must both
  succeed before requested settings are committed or persisted.
- A display-mode change presents a 15-second confirmation. Timeout, Escape, or B
  restores the last known-good window, swapchain, and presentation settings.
- F10 cannot hide a pending display confirmation. Quit events remain available.

Mouse, controller, and localized text support are delivery requirements, not
permission to make the current keyboard route inaccessible. The final overlay
must support keyboard-only operation, controller-only operation, remappable
shortcuts, high-contrast focus and selected states, non-color status cues,
scalable text, safe-area padding, and reduced-motion descriptions. It must not
depend on hover, tiny sliders, rapid key repeats, or color alone. Screen-reader
and platform accessibility API integration remains an open design item and must
not be claimed before it has been validated on each target.

## Requested and effective values

The configuration stores user intent as `requested`. Runtime capability
resolution produces separate `effective` values and explicit fallback reasons.
For example, a retained Modern+ DLSS request can resolve to portable temporal
upscaling on unsupported hardware without overwriting the request. If support is
later available, the same request becomes effective automatically. The UI must
show the active renderer/API and exact loaded DLSS runtime rather than inferring
them from configuration.

## Sections

The complete overlay is organized into these sections:

- **Profile:** Original, Modern, and Modern+ capability status. Original locks
  rendering behavior that would invalidate reference fidelity.
- **Display:** monitor, windowed/borderless/exclusive mode, output resolution,
  refresh rate, VSync/present mode, frame cap, HDR, and UI scale.
- **Resolution:** native, fixed-scale, or dynamic rendering; scale bounds and
  target presentation rate. UI and subtitles render at output resolution.
- **Anti-aliasing and upscaling:** reference, FXAA, TAA, DLAA, portable temporal
  upscaling, and Modern+ DLSS 4.5 Super Resolution with quality and sharpening.
  Unsupported combinations are disabled or resolved with a visible reason.
- **Textures:** retail or independently licensed Modern+ source, bilinear,
  trilinear, anisotropy, and mip bias.
- **Shadows:** reference/off/modern mode, quality, map resolution, cascades,
  distance, filtering, and contact shadows.
- **Advanced:** renderer API, ambient occlusion, screen-space reflections,
  volumetrics, particles, bloom, motion blur, depth of field, film grain,
  chromatic aberration, tone mapping, and shader cache.

Each row shows its requested value and, when different, its effective value and
fallback reason. Destructive-looking actions such as clearing a shader cache
require a separate confirmation and must not share the normal Apply action.
Preset selection may populate individual rows, but later row edits mark the
preset as Custom rather than silently changing those edits.

Rows may appear before their renderer implementation is complete only when they
are visibly disabled and explain the missing capability. A setting must never
pretend to apply while doing nothing.

## Application scope

| Scope | Examples | Behavior |
|---|---|---|
| Next frame | filtering, quality values, post effects | update renderer state |
| Swapchain recreation | display mode, resolution, refresh, HDR | transactional runtime change |
| Renderer recreation | profile, upscaler, DLSS enablement | brief in-process renderer reload |
| Scene reload | replacement-asset source | reload presentation assets only |
| Application restart | explicit graphics API | persist and show restart-required badge |

Apply uses a last-known-good transaction:

1. Validate the entire draft without changing live state.
2. Resolve requested values against current platform, adapter, display, driver,
   and optional-runtime capabilities.
3. Prepare all required resources before releasing the current working renderer.
4. Switch atomically, or restore the complete previous effective state on any
   failure.
5. Request confirmation for risky display changes, then persist only after the
   user keeps the result. A rejected or timed-out change is not persisted.

Ordinary next-frame changes persist after successful application. Restart-bound
changes persist the requested value with a clear `Restart required` state while
the effective value continues to report the active renderer. Configuration is
written atomically in the platform configuration directory. A malformed or
incompatible file must be quarantined or ignored with a diagnostic and must not
prevent startup; the last known-good configuration and conservative defaults are
the recovery paths.

## Platform and feature boundaries

| Feature | Windows | Linux / Steam Deck | macOS |
|---|---|---|---|
| F10 overlay and portable settings | Required | Required | Required |
| Native and portable temporal paths | Required | Required | Required |
| Modern+ replacement assets | Portable asset contract | Portable asset contract | Portable asset contract |
| DLSS 4.5 Super Resolution | Planned for supported NVIDIA RTX, D3D12, driver, and licensed runtime combinations | Exposed only if an official NVIDIA SDK explicitly supports the active native stack | Not expected; portable temporal fallback remains available |
| DLSS 5 | Not a current setting or deliverable | Not a current setting or deliverable | Not a current setting or deliverable |

DLSS labels must name the API version actually loaded. The overlay must never
label a community shader, post-process filter, portable upscaler, or unavailable
future SDK as DLSS. The `Merserk/dlss5-visual-enhancer` project is not an engine
backend; see [DLSS.md](DLSS.md). If DLSS 4.5 cannot load, the retained request may
resolve to portable temporal upscaling or native rendering with a visible reason.
This fallback must not remove resolution controls or prevent Modern+ from running.

Graphics settings cannot affect fixed simulation time, input timestamps, RNG,
AI visibility, collision, damage, mission state, save/replay state, or authoritative
simulation hashes. Original, Modern, and Modern+ consume the same gameplay render
snapshot.

## Implementation stages

1. Platform-neutral requested/effective settings, validation, menu-session state,
   capability fallbacks, and transaction tests.
2. Bounded UI draw list plus a temporary permissively licensed ASCII atlas for
   the explicitly labelled diagnostic English overlay.
3. F10 SDL event routing, keyboard navigation, overlay rendering, and safe
   window/present application.
4. Recover behavior-only retail menu layout/style measurements and bind the
   overlay to menu fonts, interface art, control-state gating, and audio read from the
   verified installation at runtime.
5. Gamepad lifecycle, atomic persistence in the platform configuration directory,
   and a Unicode text backend for all supported localizations.
6. Enable advanced rows as their real Original/Modern/Modern+ renderer paths land.

Stage 1 now has an initial implementation for Original/Modern profile selection,
windowed or borderless mode, window size, and VSync/mailbox/immediate presentation.
The resolver preserves requested intent, emits deterministic capability fallbacks,
and rejects invalid enums and dimensions. The menu session implements non-repeated
F10 open/close, draft cancellation, runtime apply acknowledgement, a 15-second
confirmation deadline, explicit rollback acknowledgement, and commit only after
success. It performs no SDL calls or persistence I/O. Modern+, DLSS intent,
render scaling, upscaling, and shadow-quality rows are represented in the model;
their real renderer implementations remain capability-gated and incomplete.

Stage 2 now has a renderer-neutral, physical-pixel draw-list model with strict
command, hit-target, and text-byte budgets. It emits the backdrop, centered
panel, implemented setting rows, focus, Apply/Cancel, busy states, and timed
Keep/Revert confirmation. The pure session owns wraparound keyboard focus and
edits, so the view never invents interaction state.

Stage 3 now routes F10, arrows, Enter, Space, and Escape through SDL and renders
the list after the scene in a physical-pixel, alpha-blended, depth-free GPU pass.
The runtime opens an embedded retail font directly from the validated startup
archive bytes and rasterizes the current UTF-8 labels through checksum-pinned
SDL_ttf without extracting or publishing the font. Bundle-order selection is
provisional until the original font-role semantics are recovered. The implemented
window mode, size,
presentation mode, and Original/Modern profile apply transactionally through
SDL, including the Keep/Revert timeout. Mouse/controller routing, persistence,
font fallback, complex-script shaping, and recovered retail styling remain open.

Stage 4 has recovered the authored coordinate space, title and two-column
anchors, row rhythm, and shared action anchor. A fail-closed startup extractor
now identifies eight neutral row owners and binds each to one persistent
one-group background plus two five-group chrome instances co-gated by the same
visibility-state mask. This does not establish GPU draw scheduling. It
preserves complete inclusive root-to-instance construction and local-transform
chains without composing them.
The current focus rectangle remains explicitly project-authored diagnostic UI;
the two chrome siblings are not focused/normal alternatives. Retail draw order,
GPU transform composition, motion, sound cues, and action behavior mapping are
still incomplete.

The recovered visibility evaluator keeps exactly one of the duplicate final-slot
rows authored-hidden. Initial state `0x01` exposes seven backgrounds and both
chrome instances for all seven visible rows. States `0x08`, `0x10`, and `0x20`
retain those backgrounds while hiding both chrome instances together; unknown
requested masks use the retail `0x01` child-selection fallback only when they
contain no recovered allowed-state bit. Mixed masks that contain an allowed bit
remain effective. Picture identities are retained. A separate
traversal/emission plan reproduces the recovered live hierarchy preorder:
eligible rows are visited in reverse authored sibling order, and each row emits
chrome instance 0, chrome instance 1, then its persistent background. Every
picture's groups remain in ascending authored order. This is CPU-side immediate
emission order, not GPU execution or completion order. The hardcoded
allowed-state mask is deliberately
limited to this recovered startup graphics control family; it is not a generic
window-state contract.

The `--screenshot FILE.bmp` diagnostic path captures the final bounded frame
(or the first frame without a frame limit), renders it into an application-owned
color target, blits it into SDL's write-only swapchain, and
downloads the same target through a 256-byte-row-aligned transfer buffer after
a GPU fence. SDL writes the local lossless BMP without another image dependency.
The writer uses a sibling `.part` file, never overwrites an existing result, and
renames only after a complete save. Captures made with retail data must remain
outside the repository and are never CI or release artifacts.

Hosted CI tests the pure model, menu state, event translation, draw-list bounds,
and fallback resolution on every target. Real fullscreen, HDR, refresh switching,
high-DPI resize, depth recreation, and GPU overlay composition remain hardware
smoke gates.

## Preview and screenshot procedure

The current diagnostic overlay can be opened directly for a bounded capture:

```sh
./build/openfreedomfighters \
  --data /path/to/FreedomFighters \
  --mode modern \
  --show-graphics-menu \
  --frame-limit 1 \
  --screenshot /path/outside/repository/f10-graphics-menu.bmp
```

The retail installation must validate first, and the destination must not exist.
Use an absolute destination outside the source tree because the scene behind the
overlay may contain retail-derived imagery. A release or CI artifact must never
contain that capture. The screenshot demonstrates the current diagnostic layout,
not fidelity to the original menu design, completed advanced controls, DLSS
integration, localization, accessibility, or final art direction.
