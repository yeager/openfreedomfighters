# Architecture

## Strategy

A literal native recompile is impossible without source code. OpenFreedomFighters therefore combines clean-room behavioral reimplementation with optional static lifting of small, well-bounded x86 routines where lawful and technically useful. The public deliverable is independently authored portable source; it does not contain original code bytes.

The implementation targets C++23 with CMake and SDL3 for window, input, gamepad,
and graphics platform services. SDL GPU is the renderer portability layer: Vulkan
on Linux and Steam Deck, Metal on macOS, and D3D12 on Windows. It provides one
explicit command-buffer and render-pass model while retaining native APIs on every
target. OpenFreedomFighters uses SDL GPU's left-handed coordinates, `[0, 1]` depth
range, top-left texture origin, and backend-independent viewport convention as the
portable rendering contract. Shaders will be authored once and compiled ahead of
time into SPIR-V, DXIL, and Metal libraries; runtime shader compilation is not a
release dependency.

## Components

```text
launcher / data verifier
          |
          v
game-data VFS -> format readers -> immutable asset model -> local cache
                                           |
                                           v
input -> deterministic simulation <-> script/mission runtime
                  |                    AI/navigation
                  |                    animation/physics
                  v
            render snapshot -> Original renderer
                            -> Modern renderer
                  |
                  +---------> audio mixer
                  +---------> UI + l10n
```

The deterministic simulation is shared by both modes. Presentation is downstream of immutable render snapshots so Modern mode cannot silently change gameplay.

## Platform matrix

| Target | Architecture | Packaging | First-class input |
|---|---|---|---|
| Windows | x86-64 | signed installer and portable archive | keyboard/mouse, XInput and SDL gamepads |
| Linux | x86-64, arm64 later | AppImage/Flatpak and tarball | keyboard/mouse, SDL gamepads |
| Steam Deck | x86-64 | Steam-compatible Linux build | Deck controls, glyph switching |
| macOS | arm64, x86-64 if feasible | notarizable `.app`/DMG | keyboard/mouse, controllers |

The current runtime creates a resizable high-DPI SDL window, selects the native
SDL GPU driver, claims its swapchain, records render passes, submits command
buffers, and exits cleanly on window close or Escape. Before creating
the GPU device it selects a textured triangle-strip primitive from the startup
scene, copies its validated vertices, indexes, and draw ranges, decodes mip zero
to RGBA8, computes finite model bounds, and joins the primitive to the first exact
GMS object-source reference. The recovered basis is stored as three row vectors;
the diagnostic world position is `basis * local_position + position`. It then
creates the native shader pipeline, uploads vertex/index/texture resources through an SDL GPU copy pass,
and submits each preserved range as an indexed triangle-strip draw. The current
bounds-normalized projection applies that instance transform but is diagnostic;
RMC/RMI scene selection and hierarchy, camera matrices, depth, and reconstructed
materials remain separate milestones. It selects the
projection plane with the greatest indexed surface area and uses a neutral white
texture tint until the startup material's zero-coded vertex-color semantics are
confirmed.

## Compatibility profiles

**Original:** fixed simulation timing; original camera/FOV option; original material and lighting equations within measurable tolerance; aspect-correct UI; optional integer-like presentation constraints; bug-compatibility flags where required for saves or missions.

**Modern:** scalable rendering resolution; ultrawide; high-refresh presentation interpolation; improved shadows, tone mapping, anti-aliasing, texture filtering, draw distance, and accessibility. Gameplay state remains identical for the same inputs.

## Data ownership

The launcher asks for a retail path, validates a supported executable hash and asset manifest, then stores only path/config/cache metadata. A future Steam locator may discover installations but must not authenticate, download, or bypass Steam on the user's behalf.
