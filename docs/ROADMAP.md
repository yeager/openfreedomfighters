# Roadmap and acceptance gates

This is a multi-year reverse-engineering effort. Dates are deliberately omitted until the vertical slice establishes throughput.

## Phase 0 - Evidence, policy, and reproducibility

- [x] Identify the Steam installation and record aggregate inventory.
- [x] Establish clean-room and game-data policies.
- [x] Define platform/mode architecture and initial subsystem map.
- [x] Add a non-extracting installation inspector.
- [x] Add a native C++23 bootstrap that requires and verifies the supported retail data.
- [x] Expose Original and Modern runtime profile selection in the native bootstrap.
- [x] Complete private byte-covering and structured Ghidra disassembly.
- [x] Complete PE sections/import inventory with a host-independent parser.
- [x] Map PE data directories, exports, delay-import absence, and TLS callbacks.
- [x] Resolve all imported ordinals and record IAT addresses.
- [x] Identify statically named dynamic APIs and direct IAT call boundaries.
- [ ] Resolve runtime-computed module/API arguments and map load-config metadata.
- [ ] Convert the 332 exported class registrations into behavior-only subsystem specifications.
- [x] Produce aggregate archive/resource magic and size census.
- [x] Complete the first three-scene field-level consistency report.
- [x] Promote initial field hypotheses to corpus-wide validators.
- [ ] Infer and validate resource section schemas.
- [ ] Capture black-box boot/menu/first-level behavior specifications.

Gate: a new contributor can generate the same structural report from a supported install without creating redistributable artifacts.

## Phase 1 - Portable data layer

- [x] Bounds-checked read-only ZIP parser and archive overlay precedence.
- [x] Add loose-file overlays and explicit scene mount lifecycles.
- [x] Add bounded streaming views for global audio banks and other large files.
- [x] Parse all audio-bank headers and validate local/global payload ranges.
- [x] Decode confirmed PCM, IMA ADPCM, and Ogg Vorbis stream families.
- [ ] Safe, bounds-checked readers for all required formats.
- [ ] Texture, mesh, material, skeleton, animation, audio, localization, and spatial data models.
- [ ] Fuzz targets and synthetic fixtures for every parser.
- [ ] Local derived-cache format with versioning and invalidation.

Gate: all startup and first-level assets load into validated portable models under ASan/UBSan; no retail data enters test artifacts.

## Phase 2 - Render/audio/input vertical slice

- [ ] SDL3 platform layer and controller action map.
- [ ] Backend prototype and renderer decision.
- [ ] Original-mode material/lighting reference path.
- [ ] Modern render graph, shadows, scaling, and post-processing baseline.
- [ ] Audio-bank decoding/mixing and positional sound.
- [ ] Startup/menu and one static level render on Linux, Steam Deck, and macOS.

Gate: deterministic camera fly-through matches structural and image-difference tolerances on all target platforms.

## Phase 3 - Simulation vertical slice

- [ ] Fixed-step clock, entity/component lifetime, events, and serialization.
- [ ] Script/mission runtime.
- [ ] Character locomotion, camera, weapons, damage, interaction.
- [ ] Navigation, AI, squad recruitment, orders, and morale/charisma mechanics.
- [ ] Animation state machine and collision/physics behavior.

Gate: first playable mission segment completes from recorded inputs with deterministic checkpoints.

## Phase 4 - Campaign compatibility

- [ ] Every mission, rebel-base segment, and cutscene.
- [ ] Mission persistence, profiles, saves, difficulty, secrets, and unlocks.
- [ ] Original input semantics and controller mappings.
- [ ] Regression traces for campaign-critical state transitions.

Gate: campaign completes in Original mode on all three targets with no compatibility-critical divergence.

## Phase 5 - Localization and Modern mode

- [ ] Message-ID catalog, Unicode shaping, fallback fonts, plural/select support.
- [ ] 20 locale packs and in-context review, including Swedish.
- [ ] Modern graphics feature set, presets, accessibility, and performance budgets.
- [ ] Translation completeness and overflow automation.

Gate: every locale passes completeness and UI-layout checks; Modern and Original simulations produce identical state hashes for shared settings.

## Phase 6 - Release engineering

- [ ] Signed/notarized packages, crash reports with opt-in privacy, migration and diagnostics.
- [ ] CI across Linux/macOS architectures and Steam Deck hardware validation.
- [ ] Automated release audit proving absence of retail content.
- [ ] User documentation, troubleshooting, contributor specifications.

Gate: fresh installs locate user-owned data and complete the campaign without Wine, Proton, Rosetta, or another emulator/compatibility layer.

## Cross-cutting acceptance criteria

- No original game data in Git history, CI, releases, or issue attachments.
- Untrusted asset input is bounds checked and fuzzed.
- Original and Modern share one authoritative gameplay simulation.
- Performance target: stable 60 fps at native Deck resolution in Modern mode; Original supports the reference timing profile.
- Save format is documented and versioned; retail-save import is optional and isolated.
