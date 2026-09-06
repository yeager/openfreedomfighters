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
window and prepares the verified startup UI archive, images and fonts. Normal
launches schedule no world draws: actual scene activation, camera and materials
remain unresolved. `--diagnostic-scene` explicitly enables the separate
source-only indexed geometry preview through SDL GPU's Vulkan, Metal or D3D12
backend path. Its lexical archive selection is not mission or bootstrap order.
Local runtime
validation currently covers Vulkan; CI compiles and tests Windows and macOS without
retail data or GPU hardware. A new scene asset builder follows ordered RMC/RMI
handles to exact GMS sources and PRM records, preserves both transform records,
retains unresolved outcomes, and includes line-list, untextured, and transparent
geometry. The startup RMC
source is not itself a primitive; normal startup does not substitute another
archive or a GMS diagnostic fallback. Primary and secondary scene handles receive explicit
stable manifest outcomes instead of silently disappearing. The projection is not
yet the scene camera or Original material model. An SDL-free GPU plan now produces
stable opaque/blended multi-instance commands, shared resource references, global
diagnostic bounds, and non-flattened clip depth for cross-platform testing.
The opt-in SDL integration is intentionally described as a multi-instance
source-only diagnostic scene; it will not establish the original camera,
RMC/RMI world placement, material behavior, or a faithfully loaded level.
Complete scene rendering, gameplay
simulation, menus, localization, and the complete native runtime remain under
development; no playable build exists yet.

The data layer also has a restricted, bounds-checked intro-controller reader
that preserves authored references, destination bytes and optional-field presence.
Reviewed source-directory lookup now joins its actual sequence and group lists,
with bounded raw-list decoding and private ordered-entry verification. Restricted
first-cut readers also decode its settings, commands and referenced sequence
resource, including event-name and target-source joins. They do not yet initialize
sequence players or activate the intro; see
[intro bootstrap evidence](docs/INTRO_BOOTSTRAP.md).
An [explicit-clock timeline converter](docs/CUT_TIMELINE.md) now preserves the
recovered arithmetic. A bounded command pass now invokes synchronous visitors in
the reviewed conditional order, verified with real first-cut data and explicit
test-clock inputs. Neither supplies a startup clock nor applies rendered/audio
effects; the complete intro player remains unimplemented.
A [conditional picture-fade receiver](docs/PICTURE_FADE.md) now emits the reviewed
alpha and owner-control effects with explicit clocks, strict completion and
synchronous callback ordering. It does not yet apply those effects to rendered
intro pictures or establish initial owner state and draw admission.
The separate intro fade-picture reader now preserves authored picture fields and
resource keys with exact component guards. Existing checked PRM/TEX APIs resolve
the real textures; startup-only composition is not reused for intro data.
Explicit engine-dimension initialization now updates fade picture scales and
invalidates their transform cache without replacing authored geometry. Original
startup dimensions, resource admission and on-screen composition remain unresolved.
Fade alpha outputs now propagate into mutable descriptor colors and shared paired
material storage; fresh draw plans consume those changes while preserving real
textures and geometry. GPU submission and complete intro playback are still pending.
A [restricted intro camera reader](docs/INTRO_CAMERA.md) preserves the actual
authored camera fields reached through the first-cut resource. It does not select
an active camera, apply aspect policy or assume a fullscreen viewport.
The reviewed mode-zero camera conversion now feeds explicit view calculations,
preserving angle rounding and separate near-plane clamps without selecting a
camera or inventing renderer dimensions.
An ordered admitted-view pass now preserves cached signed-priority ordering and
visits every enabled, non-null view. Startup registration and concrete render-pass
preparation still need to be connected; it is not an active-camera shortcut.
The first-cut legal picture also has its own guarded reader and verified real
PRM/TEX joins. Its multi-piece authored content is preserved; visibility is not
inferred merely from its member reference or centering component.
The renderer-frame orchestration now snapshots matching states, executes their
view phases, then performs ordered state/backend maintenance with explicit
admission inputs. This does not yet force or establish application startup readiness.
A conditional existing-picture activation prefix now preserves parent-guarded
authored-hide clearing, phase-one requests and tracking order. A recorded request
is not treated as proof that the legal picture became visible.
Camera enable/disable transitions now preserve renderer notification before the
runtime flag change; registration is not used as an implicit enable operation.

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
Normal startup currently clears the window and can show F10; the original intro
and menu are not yet activated or rendered. Add `--diagnostic-scene` only to
inspect the separate geometry preview. It is never a normal-startup fallback.
The [intro bootstrap evidence and gates](docs/INTRO_BOOTSTRAP.md) distinguish
recovered controller behavior from the remaining decoding and activation work.
Every normal launch first presents project-owned OpenFreedomFighters artwork for
three seconds. Retail verification and subsequent CPU asset preparation run on
a worker so the window remains responsive. Preparation begins only after
successful verification; GPU work stays on the main thread. Missing, invalid,
unsupported, or omitted game data produces a
native error dialog attached to that startup window. `--verify-only`, `--help`,
and `--version` remain headless and never open the splash.
Archive integrity errors now identify the affected installation-relative archive
and member when known, while preserving the underlying validation failure.
Later CPU-loading failures also show a diagnostic over the artwork and exit
with runtime-error status 4, without falling back to placeholder assets.
The same native window now survives the transition from splash to GPU runtime,
with its software surface released before GPU use. The exact lifecycle is in
[Native startup splash](docs/STARTUP_SPLASH.md).
The refreshed red, ivory and black splash is generated project branding at
1672×941 native resolution, aspect-fitted without stretching; see
[artwork and generation provenance](assets/branding/README.md).
`--show-graphics-menu` opens the same overlay immediately for bounded visual
smoke tests. During normal play, F10 opens and closes it.
The current controller menu mapping is Start to toggle, D-pad to navigate or
adjust, south face button to activate/confirm, and east face button to cancel.
Hotplug and focus handling guard against stale confirmations. These are project
menu controls, not recovered original gameplay mappings.
Primary mouse clicks select and advance option values; Apply/Back/Defaults and
Keep/Revert are clickable. Hit testing accounts for window pixel density and
checks confirmation expiry before accepting a click.
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
chain. The parser and composition also preserve each picture's opaque base
render property, clamped alpha byte, validated alignment enum, and the presence
and clamped value of its optional extension control. A paired GMS/BUF/PRM/TEX
loader now owns and decodes mip zero for only
those six composition-selected retail images. The native runtime uploads them
to six validated GPU sampler textures after installation verification, but does
not render them until the remaining transform and GPU integration contracts are
recovered. A separate renderer-neutral plan now preserves duplicate identities,
filters hidden instances, visits eligible rows in recovered live hierarchy
order, emits both chrome instances before the persistent background, and keeps
each picture's groups in ascending authored order. It does not promise GPU
execution or completion order. A bounded pre-raster plan now resolves those
emissions to the six resources and retains raw one-quad descriptor records and
contiguous submission ordinals. It also owns each picture's uninterpreted base
render property and retains exact authored descriptor centers and full spans
alongside derived bounds, avoiding lossy reconstruction. It preserves authored
alpha, alignment enum, and optional extension control
without generating transforms, vertices,
indexes, topology, UV corner pairing, blending, or material state.
A separate CPU-only descriptor expansion now implements recovered corner/UV
pairing, indexed quads and per-channel color reduction for an explicitly
supplied transform. It is not wired into the GPU runtime; final startup
projection and compositing remain unresolved. See
[Descriptor picture expansion](docs/PICTURE_EXPANSION.md).
An owning expanded startup plan now joins that geometry to the prepared picture
and texture identities. Every picture requires an explicit identity-keyed
transform; missing or duplicated entries fail rather than generating defaults.
The join preserves source submission order and opaque authored controls, and
does not yet provide original startup transforms or GPU raster state.
The original texture-resource bytes and PRM-relative identity now survive the
image join per draw group; shared texture images do not merge distinct resource
records. These authored records are not substituted for mutable runtime state.
A separate conditional material-request model accepts the resolved runtime
word and explicit pass/cache inputs, preserving unrequested states as inherited.
It includes the recovered base-picture property mapping but does not use that
mapping to guess final resource state. See
[Picture material requests](docs/PICTURE_MATERIAL_STATE.md).
The base-picture alpha-to-material-bit transition is also modeled separately;
it does not invent a fade schedule or resolve shared-resource writes. The
remaining first-draw evidence requires an
[original Windows observation](docs/STARTUP_STATE_CAPTURE.md), not assumptions
from constructor defaults.
A CPU-only projection boundary now accepts explicit resolved frustum values
and an established integer viewport. It preserves perspective division by
emitted Z and does not repeat the picture transform. It neither selects the
original startup camera nor replaces the diagnostic renderer's projection.
See [Conditional picture projection](docs/PICTURE_PROJECTION.md).
The raw viewport setter conversion also preserves truncation and low-32-bit
wrapping, with unsupported math-error cases rejected explicitly. Conversion
does not itself establish a usable viewport or select the original camera.
An explicit-input camera producer now covers both ordinary and alternate
half-extent formulas, the near lower bound, and one-sided normalized viewport
bounds. It feeds the validated projection boundary without assuming which
camera or pass rectangle the startup frame selects.
The CPU picture submission cache now distinguishes absent and empty group
tables, recomputes on dirty/changed-position input, and still visits every
group on reuse. Other dependency changes require explicit invalidation; see
[Picture submission cache](docs/PICTURE_SUBMISSION_CACHE.md).
Recovered picture constructors default both extent scales to `1.0`; descriptor
bounds are not substituted for the still-explicit runtime extent, matrix,
visitor, or projection inputs.
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
