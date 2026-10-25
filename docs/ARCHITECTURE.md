# Architecture

## Strategy

OpenFreedomFighters is an independently authored, portable implementation. It
does not include original executable code or depend on private analysis artifacts.
The project targets C++23, CMake, and SDL3 for windowing, input, gamepad, and
graphics platform services. SDL GPU provides a portable renderer boundary with
native backends on supported platforms. Shaders are project-authored and
compiled for their target APIs as part of the build.

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

Format readers produce validated, renderer-neutral data. The deterministic
simulation is shared by presentation modes; rendering consumes snapshots and
cannot change authoritative gameplay state. The F10 settings overlay keeps user
requests separate from detected capabilities and effective settings.

## Platform matrix

| Target | Architecture | Packaging | First-class input |
|---|---|---|---|
| Windows | x86-64 | installer and portable archive | keyboard/mouse and SDL gamepads |
| Linux | x86-64, arm64 planned | AppImage/Flatpak and tarball | keyboard/mouse and SDL gamepads |
| Steam Deck | x86-64 | Steam-compatible Linux build | Deck controls and glyph switching |
| macOS | arm64, x86-64 if feasible | notarizable `.app`/DMG | keyboard/mouse and controllers |

Platform-specific capabilities are optional. The application must retain a
portable fallback when a native graphics or audio feature is unavailable.

## Data ownership

The launcher accepts a user-selected data location and validates supported
inputs before use. It stores only application configuration and project-owned
cache metadata. Public fixtures and tests use independently authored synthetic
data. Installation content, private analysis, and derived inventories do not
belong in version control.
