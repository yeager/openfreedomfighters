# Static scene admission audit

## Established chain

The only source-backed chain that currently reaches a static frame is the
supported first-cut legal-picture diagnostic route:

```text
verified `Scenes/FF-Intro.ZIP`
  -> `load_intro_prepared_resources`
  -> complete source-directory construction in `IntroRuntime`
  -> ordinary reader bracket
  -> first-cut member reference's legal Picture owner/component receipts
  -> owned picture draw plan and decoded image subset
  -> static SDL preview
```

The archive and GMS name are explicit supported-intro provenance, rather than
an inferred level or alphabetical scene selection. The reader receives the
member's retained authored reference and verifies the live owner/resource,
one `ZGEOM_Center` component, picture source shape, and prepared picture asset.
The new `admitted_first_cut_legal_picture` preview policy additionally requires
both receipts to agree with the source supplied to the static preview. A missing
or mismatched receipt fails before copying image pixels or producing preview
data. This is admission for the existing static preview only; it is not scene
activation, a camera selection, or a draw-record producer.

## Static-transition boundary

The normal fallback is deliberately one retained, static legal-picture
presentation. It cannot safely be made into a timed fade or a sequence of
static pictures yet.

`ZWINPIC_FadeToBlack` is not attached to the legal picture. The supported first
cut has three separate fade-picture owners, each with its own component reader
and authored resource join. Their CPU receiver has recovered command handling,
but its deadlines are values in the engine scene clock, not wall-clock
durations. Normal startup has no admitted scene-clock producer, command
dispatch, owner-control delivery, material propagation, or fade-picture draw
submission.

The existing `FirstCutFadeAdmissionPlan` therefore rejects every incomplete
route: all three reader pairs, visible live owners, and completed loader and
lifecycle phase-one/phase-two boundaries are required before even producing
registration specifications. This is intentionally stricter than decoding a
fade source or observing a command name. The legal-picture reader also states
explicitly that it does not register a fade controller.

SDL's `frame_limit` is a bounded test/capture control, not an engine clock or a
display-duration scheduler. Reusing it, a host monotonic clock, or a newly
chosen delay for the fallback would create synthetic timing and would still
leave the destination scene, camera, renderer traversal and menu transition
unknown. Fading the fallback itself would additionally conflate the legal
picture with a distinct fade owner.

Consequently the current normal route must keep presenting its labelled static
fallback without an automatic timeout or fade. A source-backed static
transition becomes admissible only after the complete live fade registration,
scene-clock/event dispatch, owner material path, accepted draw records, and
destination-route evidence are observed together. That work remains part of
the active intro lifecycle milestone, not a presentation-only shortcut.

## Missing evidence before a real scene pass

No source-backed path currently establishes all of the following together:

- completion of the outer loader tail with real allocation-state, List
  association, source-lease, fallback-camera, outer-scene, spatial, and saved
  resource services;
- global component lifecycle completion and a live ordinary-frame manager
  snapshot for MovieControl event 16;
- an event-16 update strictly after the phase-two deadline, then the resulting
  first-cut camera route and current enabled state;
- renderer traversal eligibility and a producer that converts the retained
  renderer relations into accepted picture draw records; or
- a renderer backend/view pass that consumes that record under an admitted
  camera.

The retained camera reader and converter prove authored camera state, but not
runtime selection, registration, enablement, renderer dimensions, or a
presentable view. Renderer relation membership proves resource relationships,
not generic draw records. Therefore normal startup must keep automatic
cutscene picture presentation disabled. Its one source-backed static fallback
is presentation-only and remains separate from a cutscene frame. Connecting
the existing host model or SDL renderer without the above inputs would
fabricate lifecycle, camera, or draw admission.
