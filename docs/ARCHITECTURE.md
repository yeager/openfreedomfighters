# Architecture

## Strategy

A literal native recompile is impossible without source code. OpenFreedomFighters therefore combines clean-room behavioral reimplementation with optional static lifting of small, well-bounded x86 routines where lawful and technically useful. The public deliverable is independently authored portable source; it does not contain original code bytes.

The implementation targets C++23 with CMake, SDL3 for window/input/platform services, Vulkan through a portability layer on Linux, and Metal through MoltenVK or a native backend on macOS. Renderer choice will be locked by a prototype benchmark before Phase 2; the public interfaces do not depend on it.

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
| Linux | x86-64, arm64 later | AppImage/Flatpak and tarball | keyboard/mouse, SDL gamepads |
| Steam Deck | x86-64 | Steam-compatible Linux build | Deck controls, glyph switching |
| macOS | arm64, x86-64 if feasible | notarizable `.app`/DMG | keyboard/mouse, controllers |

## Compatibility profiles

**Original:** fixed simulation timing; original camera/FOV option; original material and lighting equations within measurable tolerance; aspect-correct UI; optional integer-like presentation constraints; bug-compatibility flags where required for saves or missions.

**Modern:** scalable rendering resolution; ultrawide; high-refresh presentation interpolation; improved shadows, tone mapping, anti-aliasing, texture filtering, draw distance, and accessibility. Gameplay state remains identical for the same inputs.

## Data ownership

The launcher asks for a retail path, validates a supported executable hash and asset manifest, then stores only path/config/cache metadata. A future Steam locator may discover installations but must not authenticate, download, or bypass Steam on the user's behalf.

