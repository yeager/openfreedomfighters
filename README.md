# OpenFreedomFighters

[![Build](https://github.com/yeager/openfreedomfighters/actions/workflows/build.yml/badge.svg)](https://github.com/yeager/openfreedomfighters/actions/workflows/build.yml)

OpenFreedomFighters is a clean-room, native reimplementation and static-recompilation research project for the Windows version of *Freedom Fighters* (2003). The target platforms are Windows, macOS, Linux, and Steam Deck. A legally purchased PC installation is required at runtime; this repository contains no game assets, original executable code, or encryption keys.

User-visible compatibility paths are retail-data-first: when required content is
available in the verified installation, OpenFreedomFighters reads it there rather
than substituting synthetic content. Project-authored fixtures are reserved for
CI, fuzzing, security tests, and clearly labelled developer diagnostics.

The project has two runtime profiles plus an optional presentation layer:

- **Original** preserves gameplay, timing, presentation, and content while adding a portable renderer, modern input, stable widescreen output, and localization infrastructure for 20 languages including Swedish.
- **Modern** uses the same gameplay simulation and user-supplied data, with modern rendering, scalable resolution, improved lighting and shadows, higher-quality filtering, accessibility options, and unlocked presentation frame rates where simulation safety permits.

An optional future **Modern+** layer targets independently licensed HD assets and
DLSS 4.5 on supported NVIDIA RTX hardware. A later DLSS generation will be
considered only after NVIDIA publishes its official SDK and documentation. Retail
assets and portable native/temporal rendering always remain the fallback, and no
replacement pack is distributed from this engine repository.

This is an independent fan project and is not affiliated with or endorsed by IO Interactive, Electronic Arts, or the relevant rights holders. *Freedom Fighters* and related names and assets belong to their respective owners.

## Status

Phase 0 and Phase 1 gates remain open, while the Phase 2 render, audio, input,
and menu vertical slice is underway and foundational Phase 3 simulation work has
landed. No phase gate has passed. The Steam
digital Windows build has been inventoried and fully disassembled in private
clean-room storage. The native C++ bootstrap now:

- verifies the supported user-owned installation and selects Original or Modern;
- provides a bounds-checked archive/directory overlay VFS;
- decompresses all 180 ZGF/GMS resources and parses 1,019 embedded ZGF resources;
- validates 179,838 GMS object sources, 115,977 PRM links, 154,941 identifiers,
  29,450 pool groups, and 2,998 locally resolved RMC/RMI runtime handles;
- inventories all 3,002 RMC/RMI geometry uses as 2,801 primary and 201
  secondary references, including 220 direct local primitives, four handles with
  no local source, and 2,778 local non-primitive sources;
- parses 23,522 texture images, 61,451 primitive records, 2,820,961 vertices,
  461,344 topology batches, and 4,412,738 range-checked indexes;
- resolves all 40,071 nonzero primitive texture selectors into their paired TEX
  images and classifies 46,140 opaque, 12,751 variable-alpha, and 2,533 fully
  transparent ordinary primitives;
- resolves all 1,144 startup picture draw groups through their bounded PRM manager
  keys to 334 distinct startup TEX images, with exact reverse-key validation; and
- produces renderer-facing index buffers and draw ranges for 57,284 triangle-strip
  and 4,140 line-list primitives while preserving every source batch; and
- builds an owning multi-map scene asset that preserves every resolution outcome
  and instance identity while deduplicating directly resolved PRM meshes and TEX
  images.

Texture, primary-vertex, topology, spatial, and supported audio data decode into
portable representations. The executable now opens a resizable high-DPI native
window, loads a validated textured triangle-strip preview from `FF-StartUp.ZIP`,
uploads its retail vertices, indexes, and decoded RGBA texture, and issues indexed
draws through SDL GPU's Vulkan, Metal, or D3D12 backend path. Local runtime
validation currently covers Vulkan; CI compiles and tests Windows and macOS without
retail data or GPU hardware. A new scene asset builder follows ordered RMC/RMI
handles to exact GMS sources and PRM records, preserves both transform records,
retains unresolved outcomes, and includes line-list, untextured, and transparent
geometry. The startup RMC
source is not itself a primitive, so the visible preview still uses the documented
GMS diagnostic fallback. Primary and secondary scene handles now receive explicit
stable manifest outcomes instead of silently disappearing. The projection is not
yet the scene camera or Original material model. An SDL-free GPU plan now produces
stable opaque/blended multi-instance commands, shared resource references, global
diagnostic bounds, and non-flattened clip depth for cross-platform testing.
The upcoming SDL integration is intentionally described as a multi-instance
source-only diagnostic scene; it will not establish the original camera,
RMC/RMI world placement, material behavior, or a faithfully loaded level.
Complete scene rendering, gameplay
simulation, menus, localization, and the complete native runtime remain under
development; no playable build exists yet.

## Build the native bootstrap

A C++23 compiler, CMake 3.25 or newer, zlib development files, and
libogg/libvorbis development files are required. A compatible SDL 3.2+ development
package is used when available; otherwise CMake downloads the checksum-pinned SDL
3.4.10 source release during configuration.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
./build/openfreedomfighters --data /path/to/FreedomFighters --mode original --verify-only
```

Replace `original` with `modern` to verify the second runtime profile.

Omit `--verify-only` to open the current native renderer prototype. Close the
window or press Escape to exit. `--frame-limit COUNT` is available for bounded
GPU smoke tests and still requires successful retail-data verification first.
`--show-graphics-menu` opens the same overlay immediately for bounded visual
smoke tests. During normal play, F10 opens and closes it.
`--screenshot FILE.bmp` captures the final composed frame when `--frame-limit`
is present, or the first frame otherwise, by GPU readback. The destination must
not exist. Retail-derived screenshots are local artifacts and must be written
outside this repository.
F10 is reserved for the in-game graphics-settings overlay. Its transactional
state, keyboard focus/editing, bounded cross-platform layout, timed display
rollback, and SDL GPU composition are implemented for the current settings.
The current panel exposes Original, Modern, and Modern+ intent; window mode;
output resolution; presentation mode; 50–200% render scale; native, portable
temporal, or DLSS upscaling intent; reference/high/ultra shadows; and
Apply/Cancel/Defaults. Capability resolution keeps unsupported requests separate
from effective runtime values. Advanced renderer paths, including a verified
DLSS 4.5 backend on supported Windows/NVIDIA systems, remain capability-gated
until their real implementations land; the menu does not establish DLSS support
by itself.

The verified startup data now supplies a fail-closed neutral graphics-row
composition: eight row owners, each with one persistent background picture and
two chrome pictures co-gated by the same recovered visibility-state mask. Their
24 instances bind 88 draw groups to six retail images while preserving, but not
composing, every authored inclusive root-to-instance construction and transform
chain. A paired GMS/BUF/PRM/TEX loader now owns and decodes mip zero for only
those six composition-selected retail images. The native runtime uploads them
to six validated GPU sampler textures after installation verification, but does
not render them until the remaining transform and GPU integration contracts are
recovered. A separate renderer-neutral plan now preserves duplicate identities,
filters hidden instances, visits eligible rows in recovered live hierarchy
order, emits both chrome instances before the persistent background, and keeps
each picture's groups in ascending authored order. It does not promise GPU
execution or completion order.
The visible focus marker and extended Modern/Modern+ rows
remain project-authored diagnostic UI until this recovered composition is wired
into the renderer.
The current skin is a labelled diagnostic implementation. The production F10
overlay must recover the original menu's layout and visual language and load its
applicable fonts and interface resources from the user's verified installation at
runtime; retail UI assets are never committed or shipped by this repository.

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
- [Derived cache and parser fuzzing specification](docs/CACHE_AND_FUZZING.md)
- [Campaign compatibility contract](docs/CAMPAIGN_COMPATIBILITY.md)
- [Audio-bank header format](docs/AUDIO_FORMAT.md)
- [Scene-support dependency format](docs/SCENE_SUPPORT_FORMAT.md)
- [Texture-catalog format](docs/TEXTURE_FORMAT.md)
- [Primitive-catalog format](docs/PRIMITIVE_FORMAT.md)
- [PRM-backed UI picture-resource format](docs/PICTURE_RESOURCE.md)
- [Window-picture transform contract](docs/PICTURE_TRANSFORM.md)
- [Packed ZGF/GMS resource envelope](docs/PACKED_RESOURCE_FORMAT.md)
- [ZGF resource-bundle format](docs/ZGF_FORMAT.md)
- [GMS object-source image and runtime handles](docs/GMS_FORMAT.md)
- [RMC/RMI spatial-map format](docs/RENDER_MAP_FORMAT.md)
- [Scene-transform evidence boundary](docs/TRANSFORM_BOUNDARY.md)
- [Camera and projection evidence boundary](docs/CAMERA_EVIDENCE.md)
- [Timing evidence and portable simulation policy](docs/TIMING_EVIDENCE.md)
- [Input and controller runtime specification](docs/INPUT_RUNTIME.md)
- [Audio mixing and positional runtime specification](docs/AUDIO_RUNTIME.md)
- [Deterministic simulation runtime](docs/SIMULATION_RUNTIME.md)
- [Gameplay simulation execution specification](docs/GAMEPLAY_SIMULATION.md)
- [Owning scene render asset](docs/SCENE_RENDER_ASSET.md)
- [Architecture](docs/ARCHITECTURE.md)
- [Modern graphics specification](docs/MODERN_GRAPHICS.md)
- [F10 graphics-settings overlay](docs/GRAPHICS_SETTINGS.md)
- [Retail font runtime contract](docs/RETAIL_FONT_RUNTIME.md)
- [Retail UI texture runtime contract](docs/RETAIL_UI_TEXTURES.md)
- [DLSS 4.5 integration plan](docs/DLSS.md)
- [Roadmap and acceptance gates](docs/ROADMAP.md)
- [Phase execution specifications](docs/PHASES.md)
- [Localization plan](docs/LOCALIZATION.md)
- [Clean-room protocol](CLEAN_ROOM.md)
- [Continuous integration and releases](docs/CI.md)
- [Release engineering specification](docs/RELEASE_ENGINEERING.md)
- [Third-party dependencies](THIRD_PARTY.md)
