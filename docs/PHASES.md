# Phase execution specifications

This document makes the [roadmap](ROADMAP.md) actionable. `ROADMAP.md` is the status ledger;
this file defines what each phase consumes, produces, and must prove before its
gate can pass. Later-phase work may start early, but that does not waive an earlier
gate. All public artifacts follow the [clean-room protocol](../CLEAN_ROOM.md) and
[game-data policy](../DATA_POLICY.md).

## Evidence classes used by every phase

- **Public CI evidence:** deterministic tests built entirely from independently
  authored fixtures, plus source, dependency, secret, and retail-content scans.
- **Private compatibility evidence:** reproducible local runs against a verified
  user-owned installation. Reports contain only aggregate facts, hashes, bounded
  measurements, and pass/fail outcomes suitable for clean-room publication.
- **Hardware evidence:** named target OS, architecture, GPU/API, driver, command,
  and result. A hosted compile is not a hardware or runtime pass.
- **Fidelity evidence:** behavior traces or image/audio comparisons against the
  retail reference, expressed as tolerances and results without publishing retail
  media.

Synthetic content never substitutes for available retail data in a user-visible
compatibility path. It is allowed for CI, fuzzing, security regressions, and
explicitly labelled developer diagnostics. A production feature that depends on
an undecoded retail resource remains incomplete and reports the limitation.

## Phase 0 - Evidence, policy, and reproducibility

Inputs are the supported executable and installation, black-box observations,
private static analysis, and public platform documentation. Deliverables are the
build provenance, technical/ABI/code maps, clean-room specifications, aggregate
format census, and reproducible non-extracting inspection tools.

Work is complete only when outstanding runtime-computed imports and loader
metadata are resolved, all exported registrations have behavior-only subsystem
specifications, remaining resource-section schemas are scoped, and boot/menu/
first-level black-box traces are recorded privately. The gate requires a fresh
contributor to reproduce the public structural report without emitting retail
payloads. See the [technical map](TECHNICAL_MAP.md), [ABI map](ABI_MAP.md),
[code census](CODE_CENSUS.md), [disassembly status](DISASSEMBLY_STATUS.md), and
[build provenance](BUILD_PROVENANCE.md).

## Phase 1 - Portable data layer

Inputs are validated archive views and the behavior-only format specifications.
Deliverables are bounded readers and portable models for every startup and
first-level dependency, streaming for large banks, a versioned local cache, and a
fuzz target with mutation corpus for every parser.

The gate requires ASan/UBSan parsing of every startup and first-level asset from a
verified installation, plus public malformed-input and resource-limit tests. It
also requires proof that CI artifacts, fuzz corpora, caches, and logs contain no
retail data. See the [data-layer contract](DATA_LAYER.md) and format documents
linked from the [README](../README.md).

## Phase 2 - Render, audio, input, and menu vertical slice

Inputs are immutable Phase 1 models plus recovered transform, camera, material,
menu, audio, and input behavior. Deliverables are native SDL platform paths,
controller actions, faithful world placement and Original presentation, a Modern
render baseline, audio mixing, and a startup/menu/static-level path on every
target.

The F10 production overlay is part of this phase. Its hierarchy, placement,
spacing, colors, focus and selector treatment, motion, and cues are derived as
behavior-only observations of the retail menus. Applicable fonts and interface
art are loaded from the verified installation at runtime. Added graphics rows
extend that visual grammar; the present synthetic diagnostic skin is not an
acceptance reference.

The gate requires a deterministic camera trace and structural/image-difference
tolerances on Windows, Linux, Steam Deck hardware, and macOS, plus audio and input
smoke evidence and a retail-data-backed F10 comparison. See the
[transform boundary](TRANSFORM_BOUNDARY.md), [camera evidence](CAMERA_EVIDENCE.md),
[scene asset](SCENE_RENDER_ASSET.md), [audio format](AUDIO_FORMAT.md),
[Modern graphics specification](MODERN_GRAPHICS.md), and
[graphics-settings specification](GRAPHICS_SETTINGS.md).

## Phase 3 - Simulation vertical slice

Inputs are tick-addressed input and the finite gameplay compatibility surface
from Phase 0. Deliverables are authoritative component/save schemas, mission
scripts, locomotion, camera, weapons, damage, interaction, navigation, AI, squad
systems, animation, collision, and physics for the selected first mission segment.

The gate replays recorded inputs from a declared initial state and compares
versioned checkpoints across all target platforms. Original and Modern must
produce the same authoritative hashes. See the
[simulation runtime](SIMULATION_RUNTIME.md) and [timing evidence](TIMING_EVIDENCE.md).

## Phase 4 - Campaign compatibility

Inputs are the validated vertical slice and per-mission behavior specifications.
Deliverables cover every mission, rebel base, cutscene, persistence transition,
difficulty, secret, unlock, save/profile operation, and original input mapping.
Each campaign-critical transition receives a replayable regression trace and a
documented expected checkpoint.

The gate is a fresh-start campaign completion on Windows, Linux, Steam Deck
hardware, and macOS with no compatibility-critical divergence. Failures are
tracked by mission, checkpoint, platform, and reproducible input trace; completion
cannot be inferred from asset coverage alone.

## Phase 5 - Localization, Modern, and Modern+

Inputs are the shared simulation, retail-backed Original presentation, newly
authored licensed translations, and optional independently licensed enhancement
assets. Deliverables are Unicode shaping and fallback, 20 reviewed locale packs
including Swedish, accessibility, Modern presets and budgets, a replacement-asset
contract, and capability-gated DLSS 4.5 with native and portable temporal
fallbacks.

The gate requires catalog completeness, placeholder validation, pseudo-localized
overflow tests, in-context layout review for every locale, per-platform Modern
performance evidence, and identical shared-setting simulation hashes across
profiles. See the [localization plan](LOCALIZATION.md),
[Modern graphics specification](MODERN_GRAPHICS.md),
[graphics-settings specification](GRAPHICS_SETTINGS.md), and
[DLSS plan](DLSS.md).

## Phase 6 - Release engineering

Inputs are a campaign-compatible engine and its supported-data manifest.
Deliverables are signed Windows packages, notarized macOS packages, Linux and
Steam Deck packages, migrations, opt-in privacy-preserving crash diagnostics,
support documentation, and reproducible release provenance.

The release gate runs dependency/license, Gitleaks, source-history, package
content/signature, and retail-fingerprint audits; installs each package in a clean
environment; locates or requests the user-owned data; and completes platform
startup and campaign smoke matrices without Wine, Proton, Rosetta, or another
compatibility layer. Steam Deck requires a native Linux runtime hardware result,
not only an x86-64 Linux CI build. See the [CI specification](CI.md) and
[third-party policy](../THIRD_PARTY.md).

## Cross-cutting gates

These gates apply to every phase and cannot be deferred to release day:

1. No retail payload, identifiers unnecessary for interoperability, screenshots,
   extracted text, private analysis, credentials, or proprietary runtimes enter
   Git history, CI, caches, logs, releases, or issue attachments.
2. Every untrusted-input parser has explicit byte/count/allocation/decompression
   limits, negative tests, fuzz coverage, and sanitizer evidence.
3. Presentation profiles consume one authoritative simulation and cannot affect
   timing, input, RNG, AI, collision, damage, mission state, saves, or checkpoints.
4. Performance evidence records frame-time distributions and test conditions;
   the target is stable 60 fps at native Steam Deck resolution in Modern mode.
5. Save data is documented, versioned, bounded, atomically written, migration
   tested, and recoverable. Retail-save import, if implemented, is isolated.
6. Runtime compatibility uses verified retail data when available. Synthetic
   fixtures remain test-only or conspicuously diagnostic, and optional Modern+
   replacements are independently licensed and removable.

The current checked/unchecked state and the canonical one-line gate for each
phase remain in the [roadmap](ROADMAP.md).
