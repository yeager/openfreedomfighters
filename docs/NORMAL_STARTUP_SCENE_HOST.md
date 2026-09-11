# Normal startup scene-host boundary

Normal startup constructs and retains the supported intro directory, but it
does not make that scene current. `NormalIntroSceneSession` owns the retained
runtime, its exactly-once postconstruction reader bracket, its cold reviewed
first-cut command session, and the following outer loader-tail transition. The
tail still requires every concrete service
from its caller; the session supplies no placeholder parser, association,
camera, scene operation, or saved-resource callback. Reader callback routing
no longer lives in `main`. `NormalIntroSceneHost` implements the later strict
ordering state machine described here, but is not yet connected to that session
or SDL. It prevents a shortcut that creates an audio device or a draw call
during loading and then incorrectly reports an active intro.

Normal startup currently stops after preparing the cold first-cut command
session. It does not call the session's loader-tail transition until those
production services exist. The
prepared scene now retains parser-validated, owned outer-loader source sections
(named/global, renderer payload, associations, and sizing rows) for that later
handoff; retaining them neither invokes a service nor advances the tail.
`NormalIntroSceneSession::outer_loader_tail_readiness()` exposes the exact
retained section sizes and the still-required service boundaries for diagnostic
and recovery work. It is preflight only: it does not provide placeholder
callbacks, parse a renderer container, or transition the session.

`IntroRendererPayloadObservation` is a narrower source-backed recovery step.
It validates the renderer payload's relocation groups through a caller-supplied
source-reference resolver and derives only workspace counts after every lookup
succeeds. It retains no renderer container and cannot advance the loader tail,
admit a frame, or substitute a renderer parser.

## Required ordering

The session owns one `IntroRuntime` for the lifetime of the admitted scene; the
host borrows it through checked session boundaries. It must not run before
source-directory construction has completed. It performs
the following stages exactly once, with real services at every boundary:

1. The session runs the post-construction reader bracket and outer loader tail.
   The host then continues the activation boundary at global lifecycle and
   MovieControl phase two; it must not replay either loader stage.
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
