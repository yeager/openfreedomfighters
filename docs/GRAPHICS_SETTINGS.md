# Graphics settings overlay

F10 opens the in-game graphics-settings overlay. It is a renderer overlay rather
than a separate native dialog, so the same interaction and presentation work on
Windows, macOS, Linux, and Steam Deck. The scene remains visible behind it and
gameplay input is consumed while it is open.

## Interaction contract

- F10 opens the overlay and copies the confirmed requested settings into an
  editable draft. F10 again or Escape cancels unconfirmed edits and closes it.
- Arrow keys navigate and edit rows. Enter or Space activates a row. Controller
  D-pad, A, and B provide the same operations; a controller menu shortcut will be
  finalized with the input system.
- Escape cancels the overlay when it is open and must not also quit the game.
- Apply is transactional. Validation and runtime reconfiguration must both
  succeed before requested settings are committed or persisted.
- A display-mode change presents a 15-second confirmation. Timeout, Escape, or B
  restores the last known-good window, swapchain, and presentation settings.
- F10 cannot hide a pending display confirmation. Quit events remain available.

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

Graphics settings cannot affect fixed simulation time, input timestamps, RNG,
AI visibility, collision, damage, mission state, save/replay state, or authoritative
simulation hashes. Original, Modern, and Modern+ consume the same gameplay render
snapshot.

## Implementation stages

1. Platform-neutral requested/effective settings, validation, menu-session state,
   capability fallbacks, and transaction tests.
2. Bounded UI draw list plus a temporary permissively licensed ASCII atlas for
   the diagnostic English overlay.
3. F10 SDL event routing, keyboard navigation, overlay rendering, and safe
   window/present application.
4. Gamepad lifecycle, atomic persistence in the platform configuration directory,
   and a Unicode text backend for all supported localizations.
5. Enable advanced rows as their real Original/Modern/Modern+ renderer paths land.

Stage 1 now has an initial implementation for Original/Modern profile selection,
windowed or borderless mode, window size, and VSync/mailbox/immediate presentation.
The resolver preserves requested intent, emits deterministic capability fallbacks,
and rejects invalid enums and dimensions. The menu session implements non-repeated
F10 open/close, draft cancellation, runtime apply acknowledgement, a 15-second
confirmation deadline, explicit rollback acknowledgement, and commit only after
success. It performs no SDL calls or persistence I/O. Modern+, DLSS, resolution
scaling, and advanced rows remain specified but disabled until their broader
settings fields and real renderer capability paths are implemented.

Stage 2 now has a renderer-neutral, physical-pixel draw-list model with strict
command, hit-target, and text-byte budgets. It emits the backdrop, centered
panel, implemented setting rows, focus, Apply/Cancel, busy states, and timed
Keep/Revert confirmation. The pure session owns wraparound keyboard focus and
edits, so the view never invents interaction state. SDL GPU composition,
rasterized glyph rendering, mouse/controller routing, and runtime display
application remain Stage 3 work.

Hosted CI tests the pure model, menu state, event translation, draw-list bounds,
and fallback resolution on every target. Real fullscreen, HDR, refresh switching,
high-DPI resize, depth recreation, and GPU overlay composition remain hardware
smoke gates.
