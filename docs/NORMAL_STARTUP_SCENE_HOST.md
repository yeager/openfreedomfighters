# Normal startup scene-host boundary

Normal startup constructs and retains the supported intro directory, but it
does not make that scene current. `NormalIntroSceneSession` owns the retained
runtime, its exactly-once postconstruction reader bracket, its cold reviewed
first-cut command session, and the following outer loader-tail transition. The
tail supplies the supported null-reference named reader when neither named
callback is provided and constructs the concrete renderer relation container.
The remaining services must come from its caller: allocation diagnostic-state
selection/restoration, List associations, camera, scene and saved-resource
operations. Reader callback routing
no longer lives in `main`. `NormalIntroSceneHost` implements the later strict
ordering state machine described here, but is not yet connected to that session
or SDL. It prevents a shortcut that creates an audio device or a draw call
during loading and then incorrectly reports an active intro.

Normal startup currently stops after preparing the cold first-cut command
session. It does not call the session's loader-tail transition until those
production services exist. It does present one static, source-backed legal
picture after the project splash, but that frame uses the project's generic
fit projection and is not host admission, a cut start, a camera view, or timed
playback. The
prepared scene now retains parser-validated, owned outer-loader source sections
(named/global, renderer payload, associations, and sizing rows) for that later
handoff; retaining them neither invokes a service nor advances the tail.
`NormalIntroSceneSession::outer_loader_tail_readiness()` reports retained
sections and outstanding service boundaries. It is preflight only: it does not
provide callbacks, construct a container or transition the session.

The [relation reader](INTRO_RENDERER_RELATIONS.md) uses original marked source
references and the runtime's canonical resource mapping. Queries preserve
authored member order and read each resource's current selector. The container
owns its source/workspace bytes but borrows the related resources. Its
allocation-state restoration token is not a reference-count release. Dynamic
relation updates, the separate outer-tail List associations and generic draw
production remain unfinished.

`IntroRendererPayloadObservation` is a narrower source-backed recovery step.
It validates the renderer payload's relocation groups through a caller-supplied
source-reference resolver and derives only workspace counts after every lookup
succeeds. It retains no renderer container and cannot advance the loader tail,
admit a frame, or substitute a renderer parser.

The `--probe-intro-renderer-payload` command runs this check against owned game
data. Its resolver returns the GMS-local slot address, which is a different
domain from the process-local `IntroRuntimeResourceHandle`; treating the latter
as relocation output would be fabricated and is rejected by design. The new
concrete relation reader resolves native resources directly from the original
source references instead of reinterpreting those proxy bytes.

`--probe-intro-named-global` separately profiles the named/global tagged block.
It reports aggregate tag framing only: the label and every payload value remain
private. This validates the bounded body grammar but deliberately does not
relocate references, invoke the typed reader, or advance the loader tail. The
probe accepts two opaque 32-bit words, an attachment delimiter, and a final
terminator. The separate [native named reader](INTRO_NAMED_GLOBAL.md) now handles
the narrower supported form: two named null references stored as type-16 values
in the scene's shared case-insensitive ASCII registry. Its loader-tail integration
is tested, but normal startup cannot call that tail until the other services exist.

MovieControl's concrete factory now retains a source-checked phase-two callback.
`bind_movie_control_phase_two_services` requires both exact reader receipts and
the completed ordinary reader bracket. Binding copies the service table without
calling it; execution uses the scene's canonical controller and application
clock/audio state. Required input, property and renderer services remain external.
Normal startup does not bind or invoke this callback yet. The future activation
continuation must dispatch it once through the real global phase-two pass, not
repeat it after that pass through a second controller helper.

## Missing MovieControl host contract

`NormalIntroSceneHost` is intentionally not constructed by normal startup.
Its constructor requires a live MovieControl component handle, a live owner
handle, and a signed `movie_delay`. The completed owner and component readers
can prove the source-backed owner, component, event array, resource mapping,
and reader receipts. They do not currently establish the delay consumed by
`MovieControlFirstUpdate` when it derives its deadline. The delay must not be
guessed from an event identifier, a source-directory offset, an authored
option, or a frame rate.

The next bounded implementation is therefore a recovery contract, not a host
wiring change:

1. Recover and validate the delay's producer, unit, signedness, and overflow
   behavior from private clean-room observation or disassembly evidence.
2. Add a source-backed `MovieControlHostEvidence` value that contains the
   already-proven owner/component identities plus the recovered delay, and
   rejects mismatched reader receipts.
3. Add a session factory that accepts that evidence and explicit production
   lifecycle services, constructs `NormalIntroSceneHost`, and does not invoke
   it during construction.
4. Prove with a source-free recording-host test that a missing delay or any
   missing lifecycle service produces no event dispatch, view admission,
   picture submission, audio output, or SDL GPU call.

Only after that contract exists may the normal path call the host's activation,
event-16, camera-route, view-admission, and frame-assembly stages. This keeps
the existing static preview useful for startup feedback without treating it as
evidence of recovered gameplay behavior.

## Required ordering

The session owns one `IntroRuntime` for the lifetime of the admitted scene; the
host borrows it through checked session boundaries. It must not run before
source-directory construction has completed. It performs
the following stages exactly once, with real services at every boundary:

1. The session runs the post-construction reader bracket and outer loader tail.
   The host then continues at global lifecycle, including the concrete
   MovieControl phase-two callback; it must not replay either loader stage or
   initialize the controller again afterward.
2. On a later ordinary frame, run the reviewed MovieControl event-16 update.
   It must be strictly past the phase-two deadline.
3. Route the selected first-cut camera and pass the live camera/view evidence
   through `FirstCutViewAdmissionGate`.
4. Only after a view is admitted, prepare a source-backed first-cut picture
   frame and submit it to `SdlIntroRenderer`.
5. In the same admitted ordinary-frame stage, perform the two sound receives,
   ordinary component update, position/bounds update, optional renderer
   traversal, and prepared-record sound processing in the documented order.
   The latter may create `make_sdl_intro_audio_playback` lazily, submit only
   admitted source commands, pump it, stop ordered channels, and deliver only
   its actual notifications.

Any absent callback, listener, selected camera, renderer backend, sound device,
or source record is a failed admission. It must not synthesize a draw, a scene
event, a start acknowledgement, or a replacement camera.

## Ownership

`IntroRuntime`, the admitted camera/view state, the audio playback service and
the scene host have the same scene lifetime. The startup window and SDL GPU
runtime may present their work, but neither owns scene activation. Destruction
stops channels before releasing readers and destroys the host before global SDL
shutdown.

## Verification plan

The current owned-data integration check constructs 470 resources, runs the
ordinary reader bracket with partial reader coverage and resolves the initial
relation lists through the same runtime. It validates MovieControl service
binding with zero bound-service calls. It does not complete the loader tail or
activate the scene. The following end-to-end checks remain required:

- A source-free recording host proves the stages above cannot be reordered and
  that a failed prerequisite emits no draw or notification.
- A generated PCM/fixture-resource integration test proves the first eligible
  frame reaches view admission before picture submission and sound processing.
- A GPU integration test proves the frame before the deadline is clear and the
  first admitted frame is source-backed.
- Private Steam observation may measure timing and transition behavior. It
  supplies no assets, strings, screenshots, executables, or derived material
  to this repository.

This host is intentionally not implemented by merely constructing the SDL audio
adapter. Audio requires the same lifecycle and ordinary-frame admission that
establishes the first-cut renderer view.
