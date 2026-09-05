# OpenFreedomFighters

[![Build](https://github.com/yeager/openfreedomfighters/actions/workflows/build.yml/badge.svg)](https://github.com/yeager/openfreedomfighters/actions/workflows/build.yml)

OpenFreedomFighters is a clean-room, native reimplementation and static-recompilation research project for the Windows version of *Freedom Fighters* (2003). The target platforms are macOS, Linux, and Steam Deck. A legally purchased PC installation is required at runtime; this repository contains no game assets, original executable code, or encryption keys.

The project has two runtime profiles:

- **Original** preserves gameplay, timing, presentation, and content while adding a portable renderer, modern input, stable widescreen output, and localization infrastructure for 20 languages including Swedish.
- **Modern** uses the same gameplay simulation and user-supplied data, with modern rendering, scalable resolution, improved lighting and shadows, higher-quality filtering, accessibility options, and unlocked presentation frame rates where simulation safety permits.

This is an independent fan project and is not affiliated with or endorsed by IO Interactive, Electronic Arts, or the relevant rights holders. *Freedom Fighters* and related names and assets belong to their respective owners.

## Status

Phase 1, portable data layer. The Steam digital Windows build has been inventoried and fully disassembled in private clean-room storage. The native C++ executable verifies the supported user-owned installation, selects Original or Modern mode, provides a bounds-checked overlay VFS, decompresses all 180 ZGF/GMS resources, parses 1,019 embedded ZGF resources plus 179,838 GMS object-source entries and 154,941 identifier references, decodes all 3,002 present RMC/RMI runtime object handles, parses all 23,522 texture images, 61,451 render primitives, and 2,801 spatial entries, fully traverses and queries their 3,946 octree nodes, and decodes texture, primary vertex, topology, and audio data to portable representations. No playable build exists yet.

## Build the native bootstrap

A C++23 compiler, CMake 3.25 or newer, zlib development files, and libogg/libvorbis development files are required.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
./build/openfreedomfighters --data /path/to/FreedomFighters --mode original --verify-only
```

Replace `original` with `modern` to verify the second runtime profile. Omitting `--verify-only` intentionally stops after validation until the native runtime is implemented.

## Repository rules

Do not upload game files, extracted assets, decompiled source, disassembly listings, original strings/dialogue, or generated files that substantially reproduce the original program. See [CLEAN_ROOM.md](CLEAN_ROOM.md) and [DATA_POLICY.md](DATA_POLICY.md).

Before every push, scan both history and the working tree:

```sh
gitleaks detect --source . --redact
gitleaks detect --source . --no-git --redact
```

## Inspect your installation

The inspector records hashes and structural metadata without extracting or copying assets:

```sh
python3 tools/inspect_install.py /path/to/FreedomFighters
```

Use `--private-paths` only for a local report that will not be committed.

Generate an aggregate resource-format census (at most 32 bytes read from each archive member):

```sh
python3 tools/resource_census.py /path/to/FreedomFighters
```

With the optional analysis dependency installed, generate a code-boundary census that emits no instruction listing:

```sh
python3 tools/code_census.py /path/to/FreedomFighters/Freedom.Exe
```

Full instruction listings are private research artifacts. `tools/private_disassemble.py` refuses to write inside this repository. Never commit or distribute its output.

## Documentation

- [Current technical map](docs/TECHNICAL_MAP.md)
- [Resource-format census](docs/FORMAT_CENSUS.md)
- [Windows ABI replacement map](docs/ABI_MAP.md)
- [Code-boundary census](docs/CODE_CENSUS.md)
- [2003 versus digital-build provenance](docs/BUILD_PROVENANCE.md)
- [Private disassembly status](docs/DISASSEMBLY_STATUS.md)
- [Portable data layer](docs/DATA_LAYER.md)
- [Audio-bank header format](docs/AUDIO_FORMAT.md)
- [Scene-support dependency format](docs/SCENE_SUPPORT_FORMAT.md)
- [Texture-catalog format](docs/TEXTURE_FORMAT.md)
- [Primitive-catalog format](docs/PRIMITIVE_FORMAT.md)
- [Packed ZGF/GMS resource envelope](docs/PACKED_RESOURCE_FORMAT.md)
- [ZGF resource-bundle format](docs/ZGF_FORMAT.md)
- [GMS object-source image and runtime handles](docs/GMS_FORMAT.md)
- [RMC/RMI spatial-map format](docs/RENDER_MAP_FORMAT.md)
- [Architecture](docs/ARCHITECTURE.md)
- [Roadmap and acceptance gates](docs/ROADMAP.md)
- [Localization plan](docs/LOCALIZATION.md)
- [Clean-room protocol](CLEAN_ROOM.md)
- [Continuous integration and releases](docs/CI.md)
- [Third-party dependencies](THIRD_PARTY.md)
