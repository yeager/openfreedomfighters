# OpenFreedomFighters

[![Build](https://github.com/yeager/openfreedomfighters/actions/workflows/build.yml/badge.svg)](https://github.com/yeager/openfreedomfighters/actions/workflows/build.yml)

![OpenFreedomFighters startup splash](assets/branding/openfreedomfighters-splash.png)

A clean-room native reimplementation of *Freedom Fighters*, targeting Windows,
macOS, Linux and Steam Deck.

**Not playable yet.** The build opens a window and loads startup data, but
normal startup does not yet run the original intro, menus, or gameplay.

You need your own copy of the game. Buy *Freedom Fighters* from the
[official Steam store page](https://store.steampowered.com/app/1347780/Freedom_Fighters/).
The Steam purchase includes the game and the *Freedom Fighters Soundtrack*.
The supported data set is the Steam Windows release; other editions have not
been validated. No original game files, executable code, soundtrack files, or
other retail assets are included here.

On Linux and Steam Deck, `cmake --install` also installs a standard desktop
entry. It launches the native binary and still requires a verified owned game
data directory at runtime.

By default, OpenFreedomFighters looks for the owned data in
`~/.openfreedomfighters` (or `%USERPROFILE%\\.openfreedomfighters` on Windows).
Pass `--data PATH` to use another location, or set `OPENFREEDOMFIGHTERS_DATA`
for a portable installation.

## Current state

Status reviewed 2026-10-27: the project is a native prototype, not a playable
game. A local macOS arm64 Debug executable is available; its current build has
the optional SDL Vulkan interop adapter disabled. This confirms neither the
other platform targets nor runtime rendering parity. The Steam installation
is available locally and contains the Windows x86 `Freedom.Exe` and
`Launcher.exe`; the native prototype has not been verified against that
installation in this review.

Normal startup loads the user's local resources and presents a labelled static
intro fallback. A bounded macOS arm64 Modern run captured that fallback at
1280x720 on the SDL GPU/Metal renderer. This confirms the static title image is
visible; it does not confirm animated intro playback, intro audio, original
menus, or gameplay, which remain incomplete. The GPU runtime receives no active
intro frame bridge.

The latest status review leaves this unchanged: normal startup has no verified
path that carries an active intro scene through draw submission and presentation.
Diagnostic pictures, geometry previews, and isolated renderer checks do not
verify animated playback. The intro remains a prerequisite for the original
main menu and playable-game milestones.
Camera projection and viewport utilities exist, but no admitted gameplay camera,
world transform, and normal geometry draw are joined into a parity frame. The
static startup image retains its separate diagnostic fit projection. Static
analysis confirms a per-view rendering callback consumer, but has not established
a normal visible world-geometry draw.

An optional build on TRV2 now compiles the pinned FidelityFX FSR 3.1.4 host
and Vulkan backend. This is SDK build support only; the SDL renderer still has
no FSR frame adapter or validated gameplay color, depth, and motion inputs, so
FSR remains unavailable at runtime. See the [FSR integration status](docs/FSR.md).

The diagnostic scene preview now composes authored GMS source poses through
their parent hierarchy. Map-entry placement and camera/view transforms remain
separate, so this improves diagnostic placement without establishing gameplay
world-transform or rendering parity. See the [transform boundary](docs/TRANSFORM_BOUNDARY.md).

Archive, texture, geometry, and supported audio readers are implemented. A
retained scene session owns resources and the deferred-reader bracket. Remaining
reader coverage, loader services, lifecycle activation, scene updates, and draw
production must be connected before normal intro playback can begin. See the
[normal startup scene host](docs/NORMAL_STARTUP_SCENE_HOST.md).

Installation validation discovers local files and hashes their contents. The resulting fingerprint identifies
local content for cache and save compatibility; it does not certify publisher
authenticity. Optional soundtrack discovery must remain independent of required
game data. Soundtrack cue mapping and playback remain incomplete. See
[installation validation](docs/INSTALL_MANIFEST.md).

The SDL GPU renderer supports diagnostic picture and geometry presentation.
`--diagnostic-intro-picture` selects the static intro presentation, while
`--diagnostic-scene` opens a geometry preview. An explicitly selected scene
archive is retained by a package-scoped world-scene session that carries map,
geometry-provider, and GMS-parent identities into that diagnostic plan.
Campaign packages nested below `Scenes/` are matched against their full
relative archive path, and room/group map owners can expand to source-derived
diagnostic geometry. It remains a preview: neither the session nor the renderer
establishes intro or world-scene parity. Static review follows a top-level GMS
child conditionally through the shared database-root fallback, its PRM
candidate reference, D3D factory initialization, and the same pooled slot
consumed by the ordinary view command. For supported ordinary, unflagged
descriptor categories, the typed callbacks have device draw endpoints. It does
not establish that a selected scene candidate has one of those categories,
passes its runtime gates, or produces visible output. No map-derived GMS/PRM
candidate is identity-joined to the active view, and the provider that
supplies candidate objects remains unresolved. Private installation observations and captures
remain outside this repository; synthetic renderer checks cannot prove
complete game playback.

The external [dxwrapper project](https://github.com/elishacloud/dxwrapper)
continues to publish Windows DirectDraw/Direct3D compatibility fixes; its
[latest listed release](https://github.com/elishacloud/dxwrapper/releases) is
`v1.9.8700.25`. Its release instructions say to choose one proxy stub DLL and
place it with `dxwrapper.dll` and `dxwrapper.ini` beside the Windows game
executable. The local Steam executable imports Direct3D 8, so `d3d8.dll` is
the relevant stub for a compatibility experiment. No such experiment has been
run. The checked Steam installation currently contains no `RenderD3D.dll`,
`RenderOpenGL.dll`, or dxwrapper files. The published *Hitman 2: Silent
Assassin* guide is title-specific and does not verify a Freedom Fighters fix.
dxwrapper could help run the legacy Windows game; it is not a dependency or
backend for this project's native SDL renderer and does not provide macOS,
Linux, or Steam Deck parity. See the [release instructions and notes](https://github.com/elishacloud/dxwrapper/releases)
and the project's [IOI shared-renderer investigation](docs/IOI_SHARED_RENDERER_INVESTIGATION.md).

The optional `OFF_ENABLE_SDL_VULKAN_INTEROP` build adds an SDL external-pass
boundary for future native GPU integrations. No gameplay pass currently uses
it, and it is not a DLSS, FSR, or XeSS implementation. Temporal and vendor
upscaler capabilities remain unavailable until normal scene inputs and a real
backend submission path are connected.

F10 opens the project-authored graphics settings panel. Keyboard and controller
navigation, render scaling, and saved preferences are implemented. An explicit
`--mode` overrides saved preferences for that launch. See
[graphics-settings persistence](docs/GRAPHICS_SETTINGS_PERSISTENCE.md).

Audio decoding, channel services, and an SDL output adapter exist, but the normal
startup audio lifecycle remains disconnected. Localization lookup and camera
registration also need their remaining runtime connections. Project-authored
simulation replays are tooling data and are not retail save files.

Next: connect the scene's component lifecycle and update loop so normal startup
renders the original intro and reaches its main menu. This takes priority over
graphics polish and more isolated helpers. The
[intro and main-menu plan](docs/INTRO_AND_MENU_PLAN.md) sets the work order,
dependencies and acceptance tests. Format and behavior details remain in the
[intro notes](docs/INTRO_BOOTSTRAP.md); project-wide gates are in the
[roadmap](docs/ROADMAP.md).

## Planned modes

These are targets, not working renderers:

- **Original:** the original gameplay and presentation.
- **Modern:** higher resolutions, improved lighting, shadows and filtering.
- **Modern+:** optional HD assets and future DLSS 4.5, AMD FSR, and Intel XeSS-SR backends on supported hardware.

The settings panel has a mode selector and a working 50--200% render-scale
control. The current SDL Modern path is spatial-only: it records DLSS, FSR, and
XeSS requests but uses fixed render scaling and linear output scaling until a
verified native adapter exists. No portable temporal fallback or resolver is
implemented or exposed. HD asset support is not implemented. See [Modern graphics](docs/MODERN_GRAPHICS.md),
[DLSS](docs/DLSS.md), [FSR](docs/FSR.md), and [XeSS](docs/XESS.md) for scope and licensing.
Project-authored UI strings already select the system locale across 20 locales,
including Swedish. Complete game-text localization, shaping and full retail-font
coverage remain planned. After a verified normal start, a private local cache
extracts supported English LOC source records under opaque stable IDs. A private
runtime session chooses a matching local translation pack by system locale and
falls back to that cache; it does not enable retail-text lookup or publish game
text.

## Build and run

### Required

Build from source needs a C++23 compiler, CMake 3.25 or newer, and development
files for zlib, libogg, libvorbisfile, and FreeType. `libvorbisenc` is optional:
when present, CTest additionally builds independent synthetic Ogg/Vorbis fixture
tests; its absence does not prevent a native runtime build. Ninja is the
recommended generator, but any CMake generator can be used. Git is only needed
to obtain the source tree; it is not consulted by CMake.

The configured native target must be x86-64 on Windows, Linux, or Steam Deck;
macOS accepts arm64 and x86-64 (including an explicit universal pair). CMake
rejects other architectures before it resolves dependencies.

| Platform | Toolchain and direct dependencies |
|---|---|
| Ubuntu/Debian | `build-essential cmake ninja-build pkg-config zlib1g-dev libvorbis-dev libasound2-dev libfreetype-dev libgl-dev libegl-dev libx11-dev libxext-dev libxcursor-dev libxi-dev libxrandr-dev libxss-dev libxtst-dev libxkbcommon-dev libwayland-dev libdecor-0-dev libdbus-1-dev libibus-1.0-dev libsamplerate0-dev libpulse-dev` |
| macOS | Xcode Command Line Tools, then `brew install cmake ninja zlib libvorbis freetype` |
| Windows | Visual Studio 2022 with **Desktop development with C++**, a current Windows SDK, CMake, and either Ninja or the Visual Studio generator. Provide zlib, FreeType, libogg, and libvorbis (including `vorbisfile`; `vorbisenc` enables the additional synthetic fixture tests) through `CMAKE_PREFIX_PATH`. The pinned source-build recipe used by CI is [`.github/actions/windows-dependencies/action.yml`](.github/actions/windows-dependencies/action.yml). |

SDL3 is the only graphics/window/input dependency. CMake uses an installed SDL
3.2+ package when available; otherwise it downloads checksum-pinned SDL 3.4.10
and SDL_ttf 3.2.2 source archives while configuring. That fallback requires
network access and the platform headers SDL itself enables. On Ubuntu/Debian,
the full package list above was verified with a clean Ubuntu 24.04 x86-64
fallback build. Installing a compatible system SDL3 package is usually simpler
for local builds. SDL_ttf is always built from the pinned archive and uses the
required system FreeType.

MP3 and FLAC soundtrack support uses the vendored `dr_mp3` and `dr_flac`
headers, so no MP3 or FLAC development package is needed. Shader compiler tools,
Capstone, and Python analysis dependencies are optional developer tooling;
normal builds use the checked-in generated shaders. See
[THIRD_PARTY.md](THIRD_PARTY.md) for versions, licenses, and the exact optional
tooling roles.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure

# On a Linux builder without a desktop session, run the SDL GPU tests through
# a software-rendered virtual display (install the `xvfb` package first).
LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a ctest --test-dir build --output-on-failure

# Optional: verify LOC candidate discovery against your owned installation.
OFF_LOC_DATA_ROOT=/path/to/FreedomFighters ./build/off_loc_string_index_tests

# Check your installation without opening a window.
./build/openfreedomfighters --verify-only

# Verify the source-only StartLoader to FF-StartUp package handoff. This does
# not construct a scene, present a menu, or admit lifecycle, rendering, or input.
./build/openfreedomfighters --probe-startup-route-cold

# Inspect locally hashed optional MP3/FLAC soundtrack editions without playing
# or naming a game cue. The probe reports aggregate layout and duration facts,
# plus a duration-only candidate count against unique game Vorbis streams.
./build/openfreedomfighters --probe-soundtrack

# Profile the owned startup LOC resource without printing any retail text.
# This is format-recovery evidence, not a localization catalog.
./build/openfreedomfighters --probe-localization

# Verify the bounded MovieCuts loader and main-package readers without
# selecting a route, opening a window, or beginning a cinematic.
./build/openfreedomfighters --probe-movie-cuts

# Inspect the checked, non-playing first-cut reader boundary. This opens no
# window and never starts audio, schedules an event, or renders a cutscene.
./build/openfreedomfighters --probe-first-cut-cold

# Report only aggregate intro readiness and blocker categories. This stops
# before constructing a cut command session or invoking any lifecycle.
./build/openfreedomfighters --probe-intro-readiness

# Exercise both recovered first-cut initialization phases against retained
# owned-data identities. This is still cold: no clock, scene host, audio, or
# renderer is admitted.
./build/openfreedomfighters --probe-first-cut-initialization

# Validate the retained intro renderer payload through its source-reference
# relocation and workspace grammar. This creates no renderer container or frame.
./build/openfreedomfighters --probe-intro-renderer-payload

# Profile the retained named/global tagged block without printing its label or
# payload. This does not relocate references or run its typed reader.
./build/openfreedomfighters --probe-intro-named-global

# Start the current native prototype.
./build/openfreedomfighters --mode original
```

Use the executable path produced by your build configuration; Windows builds
use `openfreedomfighters.exe`. Replace `original` with `modern` to select the
other profile. Pass `--data /path/to/FreedomFighters` when the owned install is
not in the default location.

F10 toggles settings. In the panel, Page Up/Page Down and controller shoulder
buttons move between rows; D-pad, left stick, arrows, and pointer controls also
work. Close the window to exit. For development:

- `--frame-limit N` exits after a bounded number of rendered frames.
- `--wide-screen` enables the explicit camera WideScreen setting for that launch.
  Its default is false, independent of fullscreen and window aspect. Camera
  display setup runs after SDL backend creation; this does not admit intro
  playback.
- `--show-graphics-menu` opens settings immediately.
- `--diagnostic-scene` opens the geometry preview instead of normal startup.
- `--diagnostic-scene relative/archive.ZIP` inspects one owned scene archive
  below `Scenes/` through the source-only diagnostic renderer. It is not
  gameplay, original camera behavior, or a faithful level renderer.
- `--diagnostic-startup-graphics` draws the retained startup archive's
  decoded images and 77 source-backed quads through the same source-only GPU
  diagnostic path. Its fit projection is a comparison aid, not recovered menu
  camera, material, or layout behavior.
- `--diagnostic-intro-picture` draws the retained first-cut picture through the
  indexed SDL GPU renderer. It uses the real decoded picture images and quads,
  but a project-authored fit projection and baseline render state; it is not
  automatic intro playback or a faithful cutscene camera.
- `--diagnostic-first-cut-picture-step COMMAND_INDEX` selects one still image
  by its retained first-cut command-array index. It accepts only the
  reader-admitted legal-picture or FadeToBlack targets and does not dispatch,
  time, fade, activate, or play the cutscene.

After the three-second OpenFreedomFighters splash, normal startup presents one
labelled static fallback from a reader-admitted first-cut picture while native
cutscene playback remains incomplete. The generic-fit diagnostic commands are
separate inspection surfaces; none is a reconstructed cutscene, camera, or
menu transition. See
[the intro and menu plan](docs/INTRO_AND_MENU_PLAN.md) and
[the startup-menu presentation audit](docs/STARTUP_MENU_PRESENTATION_AUDIT.md).
- `--probe-startup-boot` runs an opt-in, no-window structural diagnostic for
  the checked `FF-StartUp` BootMenu source. It prints only call order and GMS
  directory ordinals plus a content-free hierarchy fingerprint; it creates no
  scene or runtime service. See
  [STARTUP_BOOT_SCENE_PROBE.md](docs/STARTUP_BOOT_SCENE_PROBE.md).
- `--probe-startup-boot-profile` is a separate opt-in, no-window diagnostic
  for the checked BootMenu deferred envelope. It emits only aggregate framing
  counts and a framing-tag digest; it does not emit source values, bytes,
  strings, handles, ordinals, or a framing notation. See
  [STARTUP_BOOT_SCENE_PROBE.md](docs/STARTUP_BOOT_SCENE_PROBE.md).
- `--probe-startup-route-cold` validates the complete owned-data StartLoader
  handoff through the checked FF-StartUp package and its BootMenu directory
  source. It runs only the recovered three-update deferred route and reports
  construction, lifecycle, rendering, and input as unavailable. It is not a
  title-screen or playable-menu test. See
  [SCENE_TRANSITIONS.md](docs/SCENE_TRANSITIONS.md).
- `--probe-soundtrack` opens every locally hashed optional soundtrack edition
  selected by the catalog and reports aggregate format, sample-rate and channel
  coverage. It also profiles metadata from every verified scene audio header,
  without retaining paths or audio payload. It compares only duration against
  unique global Vorbis streams as a future fingerprinting filter; duration
  cannot establish track equivalence or authorize substitution. It neither
  plays audio nor derives a game-cue mapping; game music remains the fallback
  until that mapping is recovered.
- `--probe-first-cut-cold` runs the equivalent no-window integration check for
  the reviewed first-cut reader bracket. It checks aggregate bracket ordering,
  the source-backed MovieControl owner/component boundary and its retained
  cut/group reference translations, then runs due source-backed commands
  through observer-only target callbacks. Its aggregate target-owner counts
  distinguish the already-bound legal picture from admitted FadeToBlack picture
  owners without printing authored references or resource data. Cutscene
  lifecycle, gameplay effects, audio and rendering remain cold. See
  [INTRO_BOOTSTRAP.md](docs/INTRO_BOOTSTRAP.md).
- `--probe-intro-readiness` is the narrower owned-data readiness report. It
  prints only aggregate reader, unresolved-reader-family, and lifecycle
  coverage categories, then explicitly reports lifecycle, renderer, audio, and
  playback as not started. It stops before cut-command-session construction and
  never exposes source values, labels, handles, offsets, or payload data.
  Its private, source-free aggregate receipt workflow is documented in
  [INTRO_READINESS_RECEIPT.md](docs/INTRO_READINESS_RECEIPT.md).
- The private loader-tail observation workflow has a repeat-gated contract:
  two byte-identical complete traces and a distinct terminal failure are
  required before the concrete boundary can be reviewed. It does not admit
  startup or replace unresolved services. See
  [INTRO_LOADER_TAIL_CONTRACT.md](docs/INTRO_LOADER_TAIL_CONTRACT.md).
- `--probe-first-cut-initialization` extends that check through the two
  recovered first-cut initialization phases, using retained runtime identities
  from the owned installation while recording aggregate lifecycle evidence.
  It also verifies the closed command receiver's timing order without exposing
  commands. It is not a cutscene player: no clock, host callback, audio, or
  rendering is started. See [INTRO_BOOTSTRAP.md](docs/INTRO_BOOTSTRAP.md).
- `--probe-intro-renderer-payload` relocates the retained intro renderer
  payload through the source GMS slot domain and reports aggregate workspace
  structure. It does not create a renderer container, submit a frame, or
  start audio.
- `--probe-intro-named-global` profiles the bounded named/global tagged block
  from owned data using only framing tags. It reports no labels or payload
  values, validates its recovered two-word handoff, performs no relocation,
  and does not invoke the still-unimplemented typed reader.
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

Start with the [roadmap](docs/ROADMAP.md) or
[architecture](docs/ARCHITECTURE.md). The [documentation index](docs/README.md)
covers implementation plans and public platform boundaries.

## License

The project's original code is available under the [MIT license](LICENSE).
This does not cover the original game's code, graphics, music or other assets.
Third-party dependencies retain their own licenses.

Independent fan project; not affiliated with IO Interactive or Electronic Arts.
*Freedom Fighters* and its assets belong to their respective rights holders.
