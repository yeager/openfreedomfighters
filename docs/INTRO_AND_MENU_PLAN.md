# Intro and main-menu implementation plan

Source review: 2026-09-12. This is an implementation
plan, not a report of completed playback. The milestones below are all open;
the existing splash and individual tested components are inputs to them.

## Target

A normal launch, without diagnostic flags, must show the three-second
OpenFreedomFighters splash, play the authored intro with its original picture
transitions and sound, and enter a responsive main menu. Supported skip input
must reach the same menu without leaving intro events or audio running.

The menu must use the owned game's hierarchy, artwork, fonts and text, with
working navigation and settings. F10 must use the same visual language and
remain usable during display changes. Selecting a new game must produce the
verified mission request. Loading and playing that mission is the next
[roadmap](ROADMAP.md) target, not something this plan calls complete.

Preserve the current splash contract: release-derived version at lower left,
Daniel Nylander at lower right, responsive verification, and a localized error
popup over the artwork when required data is missing or invalid. Optional
soundtrack files must never be required to launch.

## What works, and what does not

| Area | Present implementation | Missing from normal execution |
| --- | --- | --- |
| Launch | Timed splash, full file hashing, worker preparation, error popup and same-window GPU handoff | Regression coverage across the complete intro-to-menu path |
| Intro data | Directory construction, retained source/resource identities, partial deferred readers and cold first-cut preparation | Complete required readers, concrete loader-tail services and global lifecycle |
| Renderer relations | Scene-owned initial relation lists resolved through canonical resources | Dynamic maintenance, separate List associations and accepted draw-record production |
| MovieControl | Source-checked factory phase-two callback and retained service binding | Phase one, live external services, global dispatch and event-16 activation |
| Intro picture | A source-backed static legal picture drawn using a generic fit projection | Authored camera, live cut timing, animation and admitted scene draws |
| Scene host | Checked ordering boundaries and a first-frame assembler | Connection to the scene session and a repeatable ordinary update/render loop |
| Intro audio | Source-bank bindings, decoders, channel service and SDL output adapters | Scene-driven start, readiness, update, stop and audible normal playback |
| Menu data | Checked FF-StartUp package, source hierarchy registry and BootMenu source discovery | Complete readers, menu activation and a concrete live scene factory |
| Menu drawing | A graphics-settings subset with six images and 77 draw groups | Full selected menu hierarchy, text, camera, state and ordered rendering |
| Input/settings | Working project-authored F10 overlay, display transactions and UI locale selection | Authored main-menu actions, focus and final retail-style F10 integration |
| Soundtrack | MP3/FLAC decoding and catalog/profiling tools | Verified per-cue replacement mapping and normal music playback |

The supported intro construction fixture contains 470 authored resources,
383 attachments and 420 queued readers. These counts describe a population,
not completion. Recompute reader coverage by family when implementing I1;
older twelve-reader summaries are historical, not a current acceptance target.

The current normal path is visible in [main.cpp](../src/main.cpp) and
[sdl_gpu_runtime.cpp](../src/platform/sdl_gpu_runtime.cpp). It prepares a cold
`NormalIntroSceneSession`, then passes a runtime pointer and static preview to
SDL. It does not construct `NormalIntroSceneHost`, complete the loader tail,
update a cut or commit a menu scene. Source-picture diagnostics and synthetic
BootMenu probes must remain separate from the production route.

## Delivery order

```text
R0  behavior contracts and integrated test cases
 |
 +--> I1 loaded and initialized intro --> I2 first cut --> I3 whole intro --+
 |                                                                       |
 +--> M1 live menu scene and complete presentation ------------------------+
                                                                         |
                                               M2 transition + navigation
                                                                         |
                                               M3 settings + localization
                                                                         |
                                               V1 platform/fidelity gate
```

M1 source/reader work can proceed alongside I1. Its presentation integration
reuses I2's completed renderer services. Build and test complete paths at each
milestone; do not count another disconnected adapter as completion.

### R0 — Establish the behavior contracts

Use the existing private observations and reviewed specifications first. For
each remaining unknown, record a bounded research question, the required
input/output behavior, and the milestone that consumes the answer.

- Record the complete boot route: initial scene, intro sequence, any external
  cuts, terminal event and menu destination. `FF-Intro` and `FF-StartUp` are
  different packages; the checked StartLoader route does not establish the
  intro's completion route.
- Measure cut order, durations, fades, camera/layout, sound onset and completion.
  Record skip, held-input, focus-loss and minimize behavior separately. A
  screenshot proves appearance at one instant, not timing or event delivery.
- Record menu entry/focus, navigation and submenu return paths, selection
  feedback, enabled states, audio cues and scene requests. Recover the active
  root and action targets rather than assigning meaning to archive order.
- Define integrated, independently authored test scenarios for the normal
  launch coordinator. Make them executable with each implementation milestone,
  using injectable clocks and failure points but production scene ownership
  and dispatch. Keep real-install runs separate.

**Exit:** a reviewed behavior/specification index and test cases for I1–M3,
with unresolved questions assigned to a work package. Start implementation
when its own contract is ready; later research need not hold it up. Exact timing
or image tolerances must come from observations and be recorded before claiming
a fidelity pass. This does not require resolving every game subsystem first.

Research, specification review and implementation follow
[CLEAN_ROOM.md](../CLEAN_ROOM.md). An implementer uses the reviewed contract,
not unpublished disassembly. Retail captures, text and audio remain private.

### I1 — Complete intro loading and initialization

**Inputs:** the retained intro package, deferred-reader catalog and reviewed
[lifecycle](INTRO_LIFECYCLE_INVENTORY.md) and
[scene-host](NORMAL_STARTUP_SCENE_HOST.md) contracts.

1. Inventory every required owner/attachment reader, its source range, live
   destination, downstream lifecycle effect and remaining contract. Start with
   the first-cut dependencies, then close the full required population. Hidden
   authored objects cannot be skipped merely because they are not on screen.
   Implement the required external-loader, source-script and reader-bracket
   hook effects too; current observations of their call order are not those
   implementations. A no-effect path needs its own reviewed behavior contract.
2. Integrate the implemented [null-reference named reader](INTRO_NAMED_GLOBAL.md)
   through the production tail. Its supported form needs no reference lookup;
   broader forms remain unsupported. Initial renderer-resource relation parsing
   and retained container ownership are implemented. Supply the paired allocation
   diagnostic-state services, typed List associations, source-lease release,
   camera-zero/fallback behavior, and required scene, spatial and saved-resource
   operations. Implement dynamic relation maintenance where the live path needs
   it. The saved state token is not a construction-reference release. Relation
   membership is not draw-record production. Keep serialized references,
   relocation addresses and native handles distinct.
3. Implement required owner hooks, component phases, event membership and
   ordinary-update admission. Extend completion validation beyond the current
   sound paths and source-bound first-cut FadeToBlack phase-one effects; owner
   coverage remains sound-only. MovieControl's factory-owned phase-two callback
   and exact reader/service binding now exist, but its phase one and surrounding
   global admission do not. Separate cold-entry validation of installed
   implementations and live services from the completed-effects report currently
   named `preflight_global_lifecycle()`. Requiring successful callback effects
   before their first global run would be circular. Check entry prerequisites,
   run the ordered lifecycle, then verify completion; do not invoke isolated
   callbacks to fill an entry gate. Remove throwing callbacks only where the
   corresponding implementation and tests exist.
4. Give the owning session a checked activation continuation. The normal path
   already ran its reader bracket; the current activation helper tries to run
   it again. Do not replay readers, bypass them with no-ops, or initialize
   MovieControl twice: its production factory now owns the phase-two callback,
   so a global phase-two pass must not be followed by the helper's separate
   controller initialization. Use one canonical controller and scene lifetime.
5. Connect the session/host to the SDL application lifetime. CPU preparation
   stays on the worker; GPU creation, presentation and SDL event handling stay
   on their owning thread. Introduce manager-owned current-scene dispatch for
   update, rendering, input, audio and teardown: SDL currently accepts either
   a diagnostic scene or one permanent raw intro pointer, not scene replacement.
   Failure must unwind the partially created scene.

**Primary code:** [normal_intro_scene_session.cpp](../src/graphics/normal_intro_scene_session.cpp),
[intro_runtime.cpp](../src/graphics/intro_runtime.cpp),
[intro_startup_activation.hpp](../include/off/graphics/intro_startup_activation.hpp),
[normal_intro_scene_host.cpp](../src/graphics/normal_intro_scene_host.cpp).

**Exit:** the normal owned-data path completes all required readers, the real
loader tail and global lifecycle exactly once, executes MovieControl phase two,
and waits for an eligible ordinary update. Its presentation operations really
reach SDL. Missing coverage cannot produce an initialized-scene result.

**Tests:** invalid references, truncated payloads, wrong generations, failed
allocation and partial initialization; complete pre/post-hook and reverse-phase
order; rejection of duplicate activation; cancellation and resource cleanup.
Record an owned-data stage trace, not just fixture callback counts.

### I2 — Play the first cut, including sound

**Dependency:** I1. The result must replace the normal static preview with live
scene output, while keeping the explicit diagnostic command available.

1. Run the ordinary update order from the shared application/scene clocks.
   Preserve the strict MovieControl `now > deadline` condition and recovered
   pause/filter behavior. Never translate authored durations into a guessed
   number of display frames. Implement the required controller, cut-player,
   fade/animation and sound callbacks and their retirement paths: the current
   ordinary dispatcher only implements the default preview-camera component.
2. Bind source-derived player members, end position, commands and concrete
   target receivers. Apply camera selection, Center/position changes and fades
   to the same mutable state later read by rendering.
3. Register the requested live camera and view; propagate transforms,
   bounds and invalidation; collect accepted ordered picture records; resolve
   actual materials, textures, viewport and clear state. Add missing GPU state
   only when required by this traversal. A generic fit camera is not acceptable.
4. Replace the host's one-frame terminal state with an ordinary repeated-frame
   lifecycle. Separate one-time activation from per-frame update, submission,
   completion and teardown. Do not reset the cut clock on redraw or resize.
5. Connect original-bank audio in the reviewed order: receive, ordinary
   components, position/bounds, optional renderer traversal, auxiliary audio
   update, receive again, then prepared-record processing. Resolve the listener
   through its own route: explicit backend listener, otherwise the first
   renderer's registered camera at index zero. It is not automatically the
   selected view camera. Decode on workers, bound buffering, and admit
   readiness only from the real producer. Decoding completion alone is not a
   sound-start acknowledgement. Continue required audio processing when no
   render is presented; stop channels before releasing source leases.

**Primary code:** [sdl_gpu_runtime.cpp](../src/platform/sdl_gpu_runtime.cpp),
[first_cut_clocked_command_runner.hpp](../include/off/cutscene/first_cut_clocked_command_runner.hpp),
[first_cut_picture_frame.cpp](../src/platform/first_cut_picture_frame.cpp),
[sdl_intro_renderer.cpp](../src/platform/sdl_intro_renderer.cpp),
[sdl_intro_audio.cpp](../src/platform/sdl_intro_audio.cpp).

**Exit:** a normal launch plays the entire first cut. Success includes the
first eligible frame, later frames reflecting actual timeline changes, audible
source sound where authored, and a single natural-completion result. A static
frame plus advancing counters does not pass.

**Tests:** before/equal/after deadlines; low and high refresh rates; command
ordering and target failure; fade/transform changes in GPU readback; audio
underflow, delayed readiness and stop during refill; focus, minimize, resize
and close. Compare real output and timing against the reviewed reference.

### I3 — Play and leave the complete intro

**Dependency:** I2. First-cut support is not whole-intro support.

1. Enumerate every reachable authored cut and external dependency on the
   supported startup route. Implement their required readers, member behavior,
   cameras, animation and sound using the same scene services.
2. Drive progression from source-defined sequence state and completion events.
   Bind external cut/package loading where actually required; do not assume the
   intro is a video file, substitute a slideshow or sort archives into a playlist.
3. Recover and implement natural completion and supported skip handling,
   including press/release ordering. Stop pending commands and channels once;
   prevent held skip input from activating the first menu item.
4. Produce the verified terminal transition request. The existing sequence
   coordinator returns `sequence_exhausted`; that result does not identify a
   scene destination or perform the handoff.

**References:** [cut timeline](CUT_TIMELINE.md),
[scene transitions](SCENE_TRANSITIONS.md),
[cut_sequence_coordinator.cpp](../src/cutscene/cut_sequence_coordinator.cpp).

**Exit:** default startup traverses the complete observed sequence and emits
one correct menu request. Natural completion and every supported skip point
end with no remaining intro callbacks, voices or stale camera registrations.
Visible menu entry is verified together with M2.

**Tests:** transition at each cut boundary, empty/invalid external members,
missing resources, late audio notifications, repeated skip, quit while loading,
and repeated full launches. Compare the whole sequence, not only its first card.

### M1 — Construct and render the actual menu scene

**Inputs:** checked FF-StartUp factory inputs and the reviewed
[BootMenu](STARTUP_BOOTMENU_BOUNDARY.md) and
[source-picture drawing](STARTUP_SOURCE_PICTURE_DRAW.md) contracts.

1. Implement the concrete scene factory and native owner/component registry.
   Prepare the required companion resources and resolve support dependencies;
   retained opaque bytes are not loaded services. Construct the complete
   required hierarchy, read owner/component payloads, bind references, and
   complete lifecycle under the package lease. Intro-specific window readers
   must not be reused as proof of FF-StartUp reader coverage. Native handles
   must identify owned objects; arbitrary nonzero integers from the synthetic
   probe are not production registrations.
2. Keep BootMenu construction, common reading and one-time initialization
   distinct. Resolve its registry keys and retained target through real services.
   Recover any required script dispatch rather than replacing it with a
   hardcoded list of menu actions.
3. Implement the live coordinator that selects the active root and camera,
   registers an enabled view, and produces/updates the retained pass rectangle,
   projection and Y basis. Existing snapshot validators do not produce these
   values. Preserve the recovered container/leaf order, visibility, focus and
   hierarchy epoch. The BootMenu owner and graphics-settings subtree are not
   implied active roots.
4. Generalize source-backed presentation beyond the current eight graphics rows
   and six images. Load all required backgrounds, title elements, text and
   controls from the selected hierarchy. Bind its camera, transforms, pass state,
   texture residency and ordered draw traversal to the completed renderer.
5. Implement bounded retail LOC lookup and bind text through stable semantic
   string IDs. Load fonts from owned data, recover their intended roles and
   metrics, and test shaping/fallback where needed. Choosing the first font
   with a matching glyph does not prove the original font role.

**Primary code:** [startup_boot_scene_factory.hpp](../include/off/runtime/startup_boot_scene_factory.hpp),
[startup_boot_menu_admission.hpp](../include/off/runtime/startup_boot_menu_admission.hpp),
[startup_active_window_root.hpp](../include/off/runtime/startup_active_window_root.hpp),
[startup_source_picture_draw_admission.hpp](../include/off/graphics/startup_source_picture_draw_admission.hpp).

**Exit:** an integrated owned-data scene test creates a genuine live menu and
renders its complete initial composition and current text through the real
coordinator/view/pass producers. It must not return a construction token without
its backing objects. Normal transition integration is M2; interactive navigation
is not implied by this first menu frame.

**Tests:** complete hierarchy and sibling order; failed/mismatched registration;
stale generation/epoch; missing text/resource bindings; visible/hidden states;
full-tree draw ordering; text metrics and aspect-ratio changes. Public fixtures
use independently authored text and imagery, not extracted menu content.

### M2 — Join intro completion to menu navigation

**Dependencies:** I3 and M1, including the verified destination contract.

1. Connect the transition request to the scene manager's staged load/commit.
   Retain the old scene until the replacement is ready under the documented
   portable failure policy. Shut down old activity in order and never publish
   two current scenes. Switch normal update, render, input and audio dispatch
   together. Preserve any authored intermediate loading scene; the StartLoader
   three-update request is not evidence for jumping straight from intro to menu.
   A failed load reports its cause without a retry loop.
2. Initialize the selected menu root, focus, listener and music, then route
   input to its actual controllers. Bind LinkMenu and related retained-object
   routes; do not reinterpret registry results as platform keycodes or mission IDs.
3. Support keyboard, pointer and controller navigation, activation and back.
   Define input ownership between intro, menu, F10 and display confirmation so
   one press cannot act in two layers. Validate focus loss and controller removal.
4. Connect authored submenu routes and each in-scope action to its real service:
   settings, return, quit and verified new-game selection. Derive mission requests
   from the recovered route, not a convenient archive name. Save/profile screens
   must not display fabricated entries; dependent gameplay services remain
   separately tracked and visibly unavailable until implemented.

**Exit:** a normal launch reaches the same responsive menu after natural
completion or supported skip. Navigation and back preserve correct focus;
quit exits cleanly; new-game selection issues exactly the verified mission
request. This gate proves an interactive menu, not playable gameplay or save/load.

**Tests:** complete launch-to-menu fixture and owned-install run; held skip;
rapid back/confirm; submenu return; failed replacement; pending input at commit;
controller hotplug; repeated transitions with no stale objects or growing
retained resources.

### M3 — Finish settings, localization and menu presentation

**Dependency:** M2. Keep Original-mode behavior as the reference before visual
enhancements.

- Apply the measured menu layout, spacing, focus/selection treatment, transitions
  and sound cues to F10. Use available decoded retail UI resources at runtime;
  the current project-authored composition is not the final fidelity reference.
- Retain the existing draft/apply/confirm/revert transaction. Verify resolution,
  window mode, refresh/presentation choices, render scale and full-resolution
  text with the live menu. Persist only successful confirmed changes; restore
  the last usable display after timeout or failed reconfiguration.
- Select the system language by default, retain explicit locale overrides,
  and resolve text by stable IDs. Verify Swedish and every declared menu locale
  in context. Existing 20-locale F10 catalogs do not establish retail menu-text
  coverage. Keep retail strings in user data and use licensed, newly authored
  project translations for added text. Check glyph coverage, shaping and overflow.
- Show requested and effective graphics capabilities separately. DLSS/FSR/XeSS
  rows must explain unavailable backends; do not present stored preferences as
  functioning upscalers. Their renderer implementation is outside this plan.
- Use original game audio for the first complete path. Optional replacements
  follow only after content, cue offsets, duration, loop and synchronization
  mapping are verified: eligible FLAC first, eligible MP3 second, original
  game audio otherwise. Similar duration or an energy signature alone is not
  a verified cue match. Soundtrack absence/corruption must not block the menu.

**Exit:** working settings survive relaunch, unsafe display changes recover,
F10 matches the reviewed menu design, and each claimed locale has passed layout
and interaction checks. Optional enhancements cannot break the Original path.

**References:** [graphics settings](GRAPHICS_SETTINGS.md),
[localization](LOCALIZATION.md), [input](INPUT_RUNTIME.md),
[audio](AUDIO_RUNTIME.md), [installation manifest](INSTALL_MANIFEST.md).

### V1 — Verify the complete path on target platforms

**Dependencies:** all milestones above. Run the same normal launch scenarios
on native Windows/D3D12, Linux/Vulkan, macOS/Metal and Steam Deck hardware.
Include native ARM64 execution where supported. CI compilation and software
GPU tests remain separate from physical-device playback evidence.

| Scenario | Required result |
| --- | --- |
| Fresh launch, valid data | Full three-second splash, complete intro with source audio, responsive menu |
| Slow verification | Responsive events; existing loading-surface behavior; no premature scene activation |
| Missing or corrupt required data | Localized actionable popup over splash; no partial scene admission |
| Soundtrack absent, invalid or unmapped | Original source music path; no startup failure or silent replacement claim |
| Skip at each supported stage | One correct menu handoff, no leaked input or continuing intro sound |
| Resize, minimize, focus loss, different refresh rate | Correct clock behavior, usable layout, no duplicate events |
| Keyboard, mouse and controller | Reachable actions, correct back/focus, safe disconnect and reconnect |
| F10 during intro/menu and display confirmation | Defined input ownership, clear focus and reliable rollback |
| Failed scene/GPU/audio operation | Specific error or explicitly supported degradation; bounded cleanup |
| Repeated launches and scene teardown | No monotonic resource growth, stale callbacks or stuck channels |

Record commit, build configuration, OS/architecture, GPU/API/driver, scenario,
expected behavior, measured result and pass/fail/skip. Measure frame-time
distribution, peak/resident memory and audio underruns over the complete path.
Validate image/audio/timing fidelity against the reviewed tolerances. Keep
captures outside the repository and publish only permitted aggregate results.

Run parser/ownership/lifecycle tests under sanitizers, then integrated GPU,
audio and input tests. Expand the normal-path test rather than accumulating
only isolated service tests. For example, the current host test checks missing
boundaries, and renderer tests use small synthetic images; neither proves
normal intro playback.

## Next implementation batch

Start with I1, not soundtrack matching or graphics polish:

Implemented I1 work includes the shared named-reference registry, the three
source-bound first-cut FadeToBlack callbacks, the scene-owned initial renderer
relation container, and MovieControl's canonical factory phase-two binding.
I1 remains open. Normal startup still does not run the loader tail, bind these
live services or dispatch the global lifecycle.

The current remote Linux verification passes 141 C++ tests and 79 Python tests;
the platform CI remains the cross-platform release gate. A separate
owned-data run constructs all 470 resources, runs the ordinary reader bracket
with partial reader coverage and resolves the relation lists through canonical
resource mapping: 157 assigned selectors, 313 unassigned resources, 97 empty
assigned groups and 248 expanded members. It validates MovieControl service
binding with zero service calls. The Steam manifest and 36 optional soundtrack
candidates passed hash checks. These checks do not execute the full loader tail,
activate a scene or establish playback. A two-frame normal SDL/Vulkan startup
run also completed, retaining 470 resources and 384 components and uploading
26 intro images. It displayed the current static picture with a generic fit
projection; the timed intro and menu remain disconnected.

1. Produce a current required-reader/owner/component coverage matrix from the
   checked data and current validators; tie each gap to its lifecycle consumer.
2. Complete the loader-tail consumers around the concrete relation container:
   allocation-state pairing, typed List associations, saved-resource operations
   and required dynamic maintenance. Recover the accepted draw-record producer
   without treating relation members as draw records. Join the supported named
   reader and container through the same production tail.
3. Implement the required reader families and concrete lifecycle state, separate
   entry validation from completion checks, then join the loader tail and checked
   session continuation. Land an integrated normal-start test with that batch;
   do not mark I1 complete on parser counts.
4. In parallel, recover M1's common window/BootMenu reader and full selected
   menu resource/text requirements. Reuse the reviewed contracts, not the
   diagnostic graphics subset as a substitute menu.

Use separate research/review and implementation assignments. One integrator
owns normal startup, shared scene services and the end-to-end tests; menu,
audio and renderer work should have explicit non-overlapping edits. Large
builds and sanitizer suites run on the designated build host; machine-specific
connection details belong only in local notes.

If a behavior contract is unavailable, continue another ready work package
while investigating that exact gap. Do not weaken admission checks, fabricate
readiness or report a blocked milestone as complete to keep a demo moving.

## Completion and reporting

Report the last passed runnable milestone and the next failing scenario.
Do not calculate completion from disassembly coverage, source-file count,
individual helper tests or parsed-resource counts. Estimate remaining work
only after complete-path milestones establish a measured implementation rate.

The implementation covered here is complete when V1 passes and the gameplay/save-system
limitations are accurately reported. Update this plan and the
[roadmap](ROADMAP.md) with commit-linked evidence as gates pass. Public changes
stay English, contain no private machine paths or game payloads, and pass
`git diff --check`, Gitleaks and the relevant tests before publication.
