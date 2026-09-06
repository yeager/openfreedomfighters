# OpenFreedomFighters

[![Build](https://github.com/yeager/openfreedomfighters/actions/workflows/build.yml/badge.svg)](https://github.com/yeager/openfreedomfighters/actions/workflows/build.yml)

A clean-room native reimplementation of *Freedom Fighters*, targeting Windows,
macOS, Linux and Steam Deck.

**Not playable yet.** The build opens a window and loads startup data, but does
not render the original intro or run the menus and gameplay.

You need your own copy of the game. The supported data set is the Steam Windows
release; other editions have not been validated. No original game files or
executable code are included here.

## Current state

- The Steam executable has been disassembled for private research.
- Archive, texture, geometry and supported audio formats have working readers.
- Startup shows the project splash for three seconds. Missing or invalid game
  data produces an error dialog over it.
- Game files are checked against a SHA-256 manifest at startup. Soundtrack files
  are optional; music playback is not implemented yet.
- The first intro sequence's camera, pictures and textures load from game data.
  One retained runtime owns their hierarchy and mutable picture state; intro
  textures upload to the GPU. Indexed drawing works in explicit integration
  tests; normal startup activation is not connected yet.
- Clock and sound preferences now share application-lifetime state across intro
  scenes. This does not add intro playback or audible sound.
- The two intro sound definitions resolve to their original audio-bank streams
  and can be decoded on demand. Playback and its readiness events are not connected.
- Their mutable sound records now share an application-owned backend with volume
  settings. Owner preparation and stop operations are implemented but not yet
  wired into the complete intro lifecycle.
- The intro runtime retains every authored component attachment and has a shared
  two-phase initialization path. Most concrete component implementations remain
  missing; a catalog entry is not an initialized component.
- F10 opens a working graphics-settings panel. Its current appearance is
  diagnostic; matching the game's menu design is still on the roadmap.
- A separate geometry preview is available with `--diagnostic-scene`. It is not
  a loaded level or a gameplay demo.

The picture renderer has passed GPU tests on Linux/Vulkan, Windows/Direct3D 12
and macOS/Metal. CI uses independent fixtures, not retail assets; these tests
do not establish complete intro playback.

Next: connect the scene's component lifecycle and update loop so normal startup
renders the original intro and reaches its main menu. This takes priority over
graphics polish and more isolated helpers. Details
are in the [intro notes](docs/INTRO_BOOTSTRAP.md) and [roadmap](docs/ROADMAP.md).

## Planned modes

These are targets, not working renderers:

- **Original:** the original gameplay and presentation.
- **Modern:** higher resolutions, improved lighting, shadows and filtering.
- **Modern+:** optional HD assets and DLSS 4.5 on supported hardware.

The settings panel has a mode selector; DLSS and HD asset support are not
implemented. See [Modern graphics](docs/MODERN_GRAPHICS.md) and [DLSS](docs/DLSS.md)
for scope and licensing. Localization in 20 languages, including Swedish, is
also planned.

## Build and run

Requires a C++23 compiler, CMake 3.25+, zlib, libogg and libvorbis development
packages. CMake uses a compatible installed SDL 3.2+ or downloads the pinned SDL
source release.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure

# Check your installation without opening a window.
./build/openfreedomfighters --data /path/to/FreedomFighters --verify-only

# Start the current native prototype.
./build/openfreedomfighters --data /path/to/FreedomFighters --mode original
```

Use the executable path produced by your build configuration; Windows builds
use `openfreedomfighters.exe`. Replace `original` with `modern` to select the
other profile.

F10 toggles settings. Close the window to exit. For development:

- `--frame-limit N` exits after a bounded number of rendered frames.
- `--show-graphics-menu` opens settings immediately.
- `--diagnostic-scene` opens the geometry preview instead of normal startup.
- `--screenshot /path/outside/repo/frame.bmp` saves a GPU readback. The file must
  not already exist. With a frame limit it captures the last frame; otherwise
  it captures the first.

Keep screenshots containing game assets outside this repository.

## Clean room and contributions

Research based on the original executable stays private. Public implementation
work follows reviewed behavior specifications. Real game content is read from
the user's installation; independent test fixtures are used in CI.

Do not submit game files, extracted assets, original dialogue, decompiled code
or disassembly listings. Read [CLEAN_ROOM.md](CLEAN_ROOM.md) and
[DATA_POLICY.md](DATA_POLICY.md) before contributing.

Before pushing, scan both history and the working tree:

```sh
gitleaks detect --source . --redact
gitleaks detect --source . --no-git --redact
```

## Documentation

Start with the [roadmap](docs/ROADMAP.md), [build provenance](docs/BUILD_PROVENANCE.md)
or [architecture](docs/ARCHITECTURE.md). The [documentation index](docs/README.md)
covers file formats, rendering research, tools and implementation plans.

Independent fan project; not affiliated with IO Interactive or Electronic Arts.
*Freedom Fighters* and its assets belong to their respective rights holders.
