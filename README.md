# OpenFreedomFighters

[![Build](https://github.com/yeager/openfreedomfighters/actions/workflows/build.yml/badge.svg)](https://github.com/yeager/openfreedomfighters/actions/workflows/build.yml)

A clean-room project to bring *Freedom Fighters* to Windows, macOS, Linux and
Steam Deck, running natively without an emulator.

**Not playable yet.** The current build verifies game files, opens a native
window and loads startup resources. The original intro, menus and gameplay do
not run yet.

You need your own copy of the game. The supported data set is the Steam Windows
release; other editions have not been validated. No original game files or
executable code are included here.

## Current state

- The Steam executable has been disassembled for private research.
- Archive, texture, geometry and supported audio formats have working readers.
- Startup shows the project splash for three seconds. Missing or invalid game
  data produces an error dialog over it.
- The first intro sequence's source records, camera, pictures and textures are
  loaded from the installation. Activation and rendering are still being connected.
- F10 opens a working graphics-settings panel. Its current appearance is
  diagnostic; matching the game's menu design is still on the roadmap.
- A separate geometry preview is available with `--diagnostic-scene`. It is not
  a loaded level or a gameplay demo.

Local graphics testing currently covers Linux/Vulkan. CI builds and runs tests
on Windows, macOS and Linux, without retail assets. Those checks do not establish
playability or visual accuracy.

The current focus is getting the original startup sequence running from real
game data. See the [intro notes](docs/INTRO_BOOTSTRAP.md) and
[roadmap](docs/ROADMAP.md) for the remaining work.

## Planned modes

**Original:** preserve the game's gameplay and presentation, with native platform
support and localization in 20 languages, including Swedish.

**Modern:** the same gameplay with higher resolutions, improved lighting,
shadows and filtering.

**Modern+:** an optional extension for separately licensed HD assets and DLSS 4.5
on supported hardware. DLSS is not implemented. A later version will only be
considered when official SDK documentation is available.

The mode selector already exists, but it does not mean these rendering features
are finished. See [Modern graphics](docs/MODERN_GRAPHICS.md) and
[DLSS](docs/DLSS.md).

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

F10 toggles settings; Escape or closing the window exits. For development:

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
