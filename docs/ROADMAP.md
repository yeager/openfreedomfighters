# Roadmap and acceptance gates

This is a multi-year reverse-engineering effort. Dates are deliberately omitted until the vertical slice establishes throughput.
The phase checklist records status; [Phase execution specifications](PHASES.md)
define inputs, deliverables, validation evidence, and exit criteria for every
phase and the cross-cutting gates. A checked item is not evidence for an entire
phase unless its phase gate also passes.

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
- [x] Document recovered variable-delta and frame-pacing evidence boundaries.
- [ ] Infer and validate resource section schemas.
- [ ] Capture black-box boot/menu/first-level behavior specifications.

Gate: a new contributor can generate the same structural report from a supported install without creating redistributable artifacts.

## Phase 1 - Portable data layer

- [x] Bounds-checked read-only ZIP parser and archive overlay precedence.
- [x] Add loose-file overlays and explicit scene mount lifecycles.
- [x] Add bounded streaming views for global audio banks and other large files.
- [x] Parse all audio-bank headers and validate local/global payload ranges.
- [x] Decode confirmed PCM, IMA ADPCM, and Ogg Vorbis stream families.
- [x] Parse and corpus-validate scene-support dependency lists.
- [x] Parse texture catalogs, mip chains, palettes, indexes, and image sequences.
- [x] Decode all observed texture encodings to portable RGBA8.
- [x] Parse primitive catalogs, descriptor indexes, and grouped mesh topology.
- [x] Decode primary primitive vertices to portable attributes.
- [x] Resolve ordinary primitive texture selectors into paired TEX image bindings.
- [x] Decode and corpus-validate the packed ZGF/GMS resource envelope.
- [x] Parse ZGF embedded-resource directories and identify bundled font/script families.
- [x] Parse GMS object-source and identifier directories.
- [x] Reproduce GMS parent/child pool traversal and all 24 class assignments.
- [x] Decode every RMC/RMI geometry reference as a runtime object-pool handle.
- [x] Reverse-map every handle with a local GMS source and retain external handles explicitly.
- [x] Decode GMS object transforms and attachment tables and validate their BUF ranges.
- [x] Parse and corpus-validate RMC/RMI spatial indexes and object descriptors.
- [x] Decode and fully traverse RMC/RMI packed octrees.
- [x] Resolve every startup picture draw group to its bounded PRM texture-resource record.
- [x] Build an owning startup graphics asset that decodes only the six
  structurally selected retail images after paired GMS/BUF/PRM/TEX validation.
- [x] Upload the six validated startup composition images to dedicated native
  GPU textures without drawing them ahead of the transform/order evidence.
- [x] Reproduce the startup graphics live hierarchy traversal and immediate
  picture/group emission order without treating it as GPU completion order.
- [x] Build a bounded pre-raster startup plan preserving the recovered 21/77
  and 7/7 traversal shapes, raw quads, opaque authored picture controls,
  six-resource identities, and contiguous emission spans without generating
  unproven transforms or raster state.
- [x] Present a project-owned three-second native splash on normal launches,
  keep retail verification responsive on a worker, and report missing or
  invalid game data in a window-parented native dialog. Verification-only and
  informational CLI paths remain headless.
- [x] Implement bounds-checked world-space queries over RMC/RMI octrees.
- [x] Identify and validate packed geometry references in RMC/RMI descriptors.
- [ ] Safe, bounds-checked readers for all required formats.
- [ ] Texture, mesh, material, skeleton, animation, audio, localization, and spatial data models.
- [ ] Fuzz targets and synthetic fixtures for every parser, following the
  [cache and fuzzing specification](CACHE_AND_FUZZING.md).
- [ ] Local derived-cache format with versioning and invalidation, following the
  [cache and fuzzing specification](CACHE_AND_FUZZING.md).

Gate: every startup and first-level asset parses into validated portable models
under ASan/UBSan; public malformed-input and resource-limit tests pass; and CI
artifacts, fuzz corpora, caches, and logs contain no retail data.

## Phase 2 - Render, audio, input, and menu vertical slice

- [x] SDL3 window, event, and GPU platform bootstrap.
- [ ] Controller action map and SDL device routing specified in the
  [input runtime contract](INPUT_RUNTIME.md).
- [x] Select SDL GPU and prove the native window/device/swapchain clear-pass lifecycle.
- [x] Prepare a validated diagnostic-scene mesh and texture for GPU upload;
  the startup UI archive has no directly resolved mesh in the current parser.
- [x] Upload and draw the first decoded retail mesh and texture.
- [x] Preserve GMS and RMC/RMI transform records separately and enforce a
  source-only diagnostic-rendering boundary.
- [x] Submit every validated scene GPU-plan command through SDL GPU as a
  multi-instance source-only diagnostic scene.
- [x] Select a non-empty diagnostic scene by deterministic validated structure,
  without treating archive order or identity as gameplay semantics.
- [ ] Recover and corpus-validate faithful GMS hierarchy and RMC/RMI transform
  composition before enabling world-space scene placement.
- [x] Materialize the validated GMS construction hierarchy as neutral parent
  links and direct children in serialized directory order; transform
  composition, visibility propagation, and draw scheduling remain separate.
- [ ] Establish camera matrix layout, handedness, depth mapping, and view
  convention before replacing the diagnostic projection with Original camera
  behavior.
- [ ] Original-mode material/lighting reference path.
- [x] Implement the recovered descriptor-emitter CPU geometry boundary with
  exact corner/UV pairing, authored center/span inputs, bounded indexed batches
  and packed-channel reduction. Final frame selection, projection and
  compositing remain separate gates; this does not enable startup drawing.
- [x] Join prepared startup submissions to owning CPU geometry with explicit
  identity-keyed picture transforms, retained texture identities and opaque
  controls, and no inferred transforms or cross-resource reordering.
- [x] Model conditional picture resource-binding and material-state requests,
  base-property expansion, and cache/suppression behavior without inferring
  draw-time material words, inherited states or final startup pass inputs.
- [x] Implement ordinary picture projection and viewport mapping from explicit
  resolved frustum values and an established integer viewport, preserving
  clip W=Z without repeating the picture basis or inventing camera defaults.
- [x] Implement both explicit camera half-extent producers and the normalized
  viewport-request producer, including distinct zero-dimension rules. Actual
  startup camera selection and GPU integration remain unverified.
- [x] Implement conditional picture submission-cache lifecycle and ordered
  group visitation on both recomputation and reuse. Runtime invalidation
  propagation and original first-frame cache state remain unverified.
- [ ] Modern render graph, shadows, scaling, and post-processing baseline.
- [ ] Audio mixing, device output, streaming voices, and positional sound as
  specified by the [audio runtime contract](AUDIO_RUNTIME.md). Audio-bank
  decoding is implemented; runtime playback is not.
- [ ] Startup/menu and one static level render on Windows, Linux, Steam Deck, and macOS.
- [x] Decode the restricted supported intro-controller field stream with bounded
  identity/wrapper checks and presence-preserving optional references; verify
  returned fields privately against owned data without executing the controller.
- [x] Decode authored intro sequence/group reference lists and resolve their
  source identities through the reviewed deferred-load mapping, preserving
  ordered raw values and verifying all joins privately against owned data.
- [ ] Decode sequence-player payloads and initialize their runtime resources;
  source identity alone does not establish a functioning player.
- [x] Decode the restricted first-cut component settings, ordered command fields
  and subordinate sequence payload; resolve authored event names separately
  from source references and verify the full route privately on owned data.
- [x] Convert explicit scene-clock words to cut timeline coordinates with the
  reviewed low-word retention and float rounding, without a guessed clock source.
- [x] Execute an already-admitted bounded command phase with explicit registration
  order and synchronous visitors, preserving cached insertion, strict boundaries
  and callback-before-cursor updates. Verify its conditional trace on owned data.
- [x] Implement the reviewed conditional picture-fade CPU receiver with explicit
  clocks, ordered owner effects and strict completion; no guessed initial color.
- [ ] Join intro picture owners and resource lifecycle to actual fade rendering;
  CPU alpha requests alone do not establish visible intro playback.
- [x] Decode the restricted intro fade-picture source grammar separately from
  startup pictures and verify real PRM/TEX resource and pixel joins privately.
- [x] Apply reviewed fade picture sizing from explicit engine dimensions, with
  change-only transform-cache invalidation and ordered resource notification.
- [x] Propagate fade alpha into mutable descriptor colors and paired materials,
  preserving shared storage and producing fresh renderer-facing draw snapshots.
- [x] Decode the restricted first-cut camera source with exact tagged bounds and
  privately verify all authored fields through the real member reference.
- [x] Convert the supported new-camera mode-zero fields with reviewed binary32
  boundaries and connect them to view helpers using explicit renderer inputs.
- [x] Execute the bounded admitted-view phase with cached signed-priority order,
  stable ties and begin/traversal/end callbacks for every eligible view.
- [x] Decode the distinct Center-attached first-cut legal picture and privately
  verify its real multi-piece picture and complete texture bindings.
- [x] Connect explicit renderer/backend admission to ordered state-frame snapshots,
  nested view phases and subsequent state/backend maintenance.
- [x] Implement the reviewed new-existing-picture activation prefix with parent
  hide guards, ordered lifecycle requests and distinct request/success semantics.
- [x] Implement explicit camera enable transitions with idempotence, preserved
  runtime flags and renderer notification before the enabled-bit write.
- [x] Connect admitted Center phase one to authored-derived local position,
  ordered resource-service notification, cache invalidation and component status.
- [x] Implement the shared position-service immediate/deferred scheduling and
  bounded handle queue, with explicit downstream bounds/maintenance operations.
- [x] Complete final-batch scene behavior for validated all-plain-picture owners,
  rejecting unknown/mixed classes instead of assuming every queued owner is a picture.
- [x] Compute plain-picture local bounds from checked PRM group spans with
  explicit scale, binary32 boundaries and separate raw/clamped extents.
- [x] Apply materialized picture bounds after the explicit renderer-resource
  base query, preserving both result branches and ordered runtime writes.
- [x] Connect the ordinary legal picture's retained renderer ID to the concrete
  zero-ID bounds query without substituting its PRM key or invented query data.
- [x] Recompute group resource and child-only owner bounds from ordered live
  children, preserving filters, root fallback conditions and distinct extent rules.
- [x] Connect eligible identity-basis parent-space bounds to expansion-only shared
  ancestor updates, preserving no-expansion stops and existing sibling bounds.
- [x] Bind reviewed concrete intro owner opt-outs and privately verify complete
  root child membership plus deferred two-ancestor updates using owned resources.
- [x] Retain owning first-cut source, picture and texture resources in normal
  CPU startup preparation, with checked joins and a pre-decode memory budget.
  This prepares data without claiming lifecycle admission or intro rendering.
- [x] Decode the first-cut window source through the camera's authored parent
  and validate its reciprocal camera reference without parsing unrelated windows.
- [x] Construct the fresh intro camera's reviewed CPU fields with one canonical
  runtime flag word; keep window effects and renderer registration separate.
- [x] Join prepared camera services and live picture/camera transform chains to
  cache preparation, retaining the synthesized root, own-bounds XY alignment
  and explicit undefined-Y policy. Verify real legal-picture expansion privately.
- [ ] Connect admitted intro views and ordered picture submissions to GPU drawing,
  including the complete draw-pass reset, resource keys and inherited state.
- [x] Build checked picture ordering keys from current view/control bytes and
  selected texture IDs; merge explicit retained/rebuilt partitions with documented
  native equal-key policy. Runtime queue admission remains separate.
- [ ] Recover original bootstrap selection, scene-name resolution, scene-stack
  transitions, initial cut activation and natural completion events before
  enabling the intro-to-menu path. The project splash is not an intro timer.
- [ ] Replace the diagnostic F10 skin with a retail-data-backed overlay matching
  the recovered original menu layout and visual language.
- [x] Recover and bounds-check the startup tagged window-picture stream through
  its authored state exponent, opaque base-render property, clamped alpha,
  validated alignment, presence-preserving optional extension, and picture
  asset reference.
- [x] Parse every startup PRM-backed picture resource with bounded descriptors,
  draw-group texture references, and portable descriptor spans.
- [x] Recover the draw-group texture-resource allocation and its bounded TEX-image
  join; all 1,144 startup draw groups resolve to 334 distinct images in both
  directions without names, dimensions, catalog order, or pixel heuristics.

Gate: a deterministic camera trace meets structural and image-difference
tolerances on Windows, Linux, Steam Deck hardware, and macOS, with audio/input
smoke evidence and a retail-data-backed F10 comparison.

## Phase 3 - Simulation vertical slice

Execution details and evidence requirements are defined in the
[gameplay simulation specification](GAMEPLAY_SIMULATION.md).

- [x] Portable rational fixed-step scheduler and tick-addressed input snapshots.
- [x] Deterministic generational entity lifetime, future-tick event queue, and
  canonical state checkpoints.
- [ ] Gameplay component model and save-game serialization.
- [ ] Script/mission runtime.
- [ ] Character locomotion, camera, weapons, damage, interaction.
- [ ] Navigation, AI, squad recruitment, orders, and morale/charisma mechanics.
- [ ] Animation state machine and collision/physics behavior.

Gate: recorded inputs replay from a declared initial state with versioned
deterministic checkpoints on every target platform, and Original and Modern
produce identical authoritative hashes.

## Phase 4 - Campaign compatibility

- [ ] Every mission, rebel-base segment, and cutscene.
- [ ] Mission persistence, profiles, saves, difficulty, secrets, and unlocks.
- [ ] Original input semantics and controller mappings.
- [ ] Regression traces for campaign-critical state transitions.

Gate: a fresh-start campaign completes in Original mode on Windows, Linux, Steam
Deck hardware, and macOS with no compatibility-critical divergence.

## Phase 5 - Localization, Modern, and Modern+

- [ ] Message-ID catalog, Unicode shaping, fallback fonts, plural/select support.
- [ ] 20 locale packs and in-context review, including Swedish.
- [ ] Modern graphics feature set, presets, accessibility, and performance budgets.
- [ ] Optional Modern+ replacement-asset contract and curated HD content pipeline.
- [ ] Optional Modern+ DLSS 4.5 backend with portable temporal and native fallbacks.
- [ ] Evaluate later DLSS generations only from published NVIDIA SDKs and documentation.
- [ ] Translation completeness and overflow automation.

Gate: every locale passes catalog completeness, placeholder, pseudo-localization
overflow, and in-context layout checks; Modern meets per-platform performance
requirements; and Modern and Original produce identical simulation hashes for
shared settings.

## Phase 6 - Release engineering

Packaging, trust, privacy, migration, clean-install, support, and reproducibility
requirements are defined in the
[release engineering specification](RELEASE_ENGINEERING.md).

- [ ] Signed/notarized packages, crash reports with opt-in privacy, migration and diagnostics.
- [ ] CI across Windows, Linux, and macOS architectures plus Steam Deck hardware validation.
- [ ] Automated release audit proving absence of retail content.
- [ ] User documentation, troubleshooting, contributor specifications.

Gate: every release package passes dependency/license, Gitleaks, source-history,
retail-content, signature, and clean-install audits, locates or requests
user-owned data, and completes the campaign natively on every target platform.

## Cross-cutting acceptance criteria

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
