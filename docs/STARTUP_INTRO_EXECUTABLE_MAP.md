# Startup and intro executable evidence map

This map separates private executable observations from public implementation
authority. It contains no instructions, addresses, strings, assets, screenshots,
or data extracted from the owned game.

## Evidence ladder

| Boundary | Private evidence | Native implementation status | What is still required |
|---|---|---|---|
| Process entry | The supported image is a 32-bit Windows executable with no TLS callback. A launcher boundary precedes the game process. | The portable application has its own process and startup lifecycle. | Retail command-line, launcher-to-game arguments, and failure behavior. |
| Platform bootstrap | Static review identifies a single renderer-factory caller in a larger startup path. Its factory is retained and checked before later renderer work. | SDL owns the portable window and GPU backend. | Device/presentation selection, render-target ownership, and frame scheduling. |
| Intro archive admission | Owned-data validation identifies one supported intro archive and verifies its complete resource family before construction. | The archive can be parsed, retained, and prepared without executing retail code. | The source loader's complete state/service contract. |
| Deferred readers | The cold source path has a finite deferred-reader queue, with only a reviewed subset admitted by native readers. | Reviewed reader forms are bounded and fail closed; the remainder is not dispatched. | Exact per-family reader behavior, registration, mutations, and failures for every required dependency. |
| Global lifecycle | Static review ties the controller registration to metadata, but does not uniquely identify a phase-one callback. | Phase-two models are isolated test contracts only. Normal startup does not invoke global passes. | Two fresh source-free phase-one traces, a matching failure trace, and independent review. |
| First-cut handoff | A behavior-only contract bounds the event-to-player relation, but native integration is intentionally disconnected. | Preparation and isolated models exist; no automatic player start occurs. | Repeatable success and failure observation of the real dispatch and player activation. |
| Camera and draw pass | Source readers establish retained camera/picture state and a source-backed static diagnostic picture route. | A diagnostic picture can use SDL GPU; normal startup never claims an admitted scene pass. | Runtime camera selection/enabling, renderer traversal, draw-record production, and an admitted view. |
| Cinematic/menu presentation | Private black-box observation establishes an opening cinematic sequence before an interactive menu. | Project splash and diagnostic UI are distinct from the retail sequence. | Timeline, picture ordering, timing, input/skip policy, audio cues, aspect policy, and menu activation. |

## Candidate rule

A static candidate is not a callable contract. Candidate selection may narrow
the private observation target, but it cannot authorize a native callback,
invent a completion state, or enable normal intro playback. The phase-one and
handoff gates require source-free, fresh-process success and failure traces.

## Practical observation order

1. Produce the private phase-one candidate matrix from the owned executable.
2. Collect two independent source-free lifecycle success traces and one failure
   trace; validate them with the documented private sanitizers.
3. Collect matching MovieControl-to-first-cut success and failure traces.
4. Observe player activation, camera/view admission, then renderer-frame
   production as separate contracts.
5. Only after those contracts pass, compare the resulting native presentation
   to private reference observations without retaining retail media.

The relevant gates and tools are documented in
[MovieControl phase one](MOVIE_CONTROL_PHASE_ONE.md),
[MovieControl-to-cutscene dispatch](MOVIE_CONTROL_CUTSCENE_DISPATCH.md),
[CutSequence player lifecycle](CUT_SEQUENCE_PLAYER_LIFECYCLE.md),
[static scene admission](STATIC_SCENE_ADMISSION_AUDIT.md), and
[private intro observation](INTRO_OBSERVATION.md).
