# Intro lifecycle recovery inventory

This is the implementation frontier for the supported, user-owned intro. It
records recovered behavior only; it does not make a component runnable. Global
lifecycle remains blocked until every eligible component has its real reader,
callback, owner state, and required services.

| Factory group | Phase-one recovery | Phase-two recovery | Current blocker |
| --- | --- | --- | --- |
| `ZGROUP_RootGroup` | Initializer and phase-one seam | None | Ordinary input and phase-two behavior |
| `ZGEOM_MovieControl` | None | Callback ordering | Full global admission, live services, event 16 and cut dependencies |
| `ZGEOM_Center` | Legal-picture owner/component provenance and position/cache seam | None | Global admission, position service and event delivery |
| `ZWINPIC_FadeToBlack` | Owner/component provenance and fade/dimension seams | None | Material/color path and event delivery |
| `ZGEOM_MatPosAnim` | KEYS preparation seam | None | Existing-state provider, events and special-owner behavior |
| `ZLIST_CutSequence`, `ZLIST_CutSequenceList`, `ZLIST_CutSequenceCommand` | Timeline/source sub-boundaries | None | Player registration, dispatch and completion timing |
| `ZSNDOBJ_SoundExtend`, `ZSNDOBJ_SoundNotify`, `ZSNDOBJ_SoundSegment`, `ZGEOM_ZSetZDefine` | Typed data and owner-side sub-boundaries | None | Output/channel admission and readiness producer |
| `ZCHAROBJ_CharFader`, `ZWINPIC_LogoFade` | None | None | Reader and event/mode behavior |
| `ZLIST_ExternCutSequenceCommand`, `ZSTDOBJ_VertAnim` | None | None | Reader, update and dispatch behavior |
| `ZGEOM_FilmGrainCamSetup`, `ZWINDOW_LensFlareControl`, `ZWINPIC_LensFlare`, `ZGEOM_ParamAnim`, `ZGEOM_ParticleEmitter`, `ZLIST_LensFlareLights`, `ZSTDOBJ_ScrollTexture` | Constructor-only state | None | Complete reader and runtime callbacks |

All 22 authored factory types are constructed and catalogued. Their generic
lifecycle callbacks intentionally fail closed until their real behavior is
recovered. MovieControl's phase-two callback is the only authored callback body
usable in isolation; it still cannot be called through normal startup.

## Required order

1. Complete the 420 deferred owner/component reader boundaries, including the
   separate MatPos provider route.
2. Implement loader-tail services: named/global data, renderer payload,
   associations, saved flags and live resource state.
3. Complete reader-backed component factories, owner hooks and event/ordinary
   membership.
4. Run the full reverse phase-one pass, then the reverse phase-two pass.
5. Recover ordinary updates, first-cut activation, camera/view admission and
   scene rendering.

The existing Window and MovieControl reader boundaries are deliberately narrow
examples of step 1. Neither is an independent startup path.

## Visible first-frame gate

The currently prepared first-cut data cannot be made visible by connecting an
SDL renderer directly. The required order is fixed by the admitted boundaries:

1. Complete the full reader, component, and owner coverage required by global
   lifecycle preflight.
2. Run the real global passes and the later MovieControl event boundary.
3. Establish the requested camera, live view, and positive-time first-cut
   activation.
4. Recover the renderer-record association and ordered traversal for the live
   legal picture.
5. Assemble and submit an admitted picture frame from an ordinary outer frame
   caller.

The current supported runtime has only eleven admitted reader identities out of
420. This is the first hard gate. Camera/view, picture-frame, audio, and SDL
bridge code remains intentionally downstream of it; joining any of those
pieces earlier would create a synthetic visible result rather than native scene
admission.

## Static-analysis priority

The next recovery pass follows the deferred-reader dispatcher from its queue
loop through the owner base reader and forward attachment-component dispatch.
It records only reader inputs, owner mutations, component registration, event
or lifecycle effects, failure behavior, and ordering. It does not publish
disassembly, addresses, retail strings, or binary-derived artifacts.

The first reader families are the direct first-cut dependencies:

1. `ZWINPIC_FadeToBlack` and `ZGEOM_Center`;
2. `ZLIST_CutSequence`, `ZLIST_CutSequenceList`, and
   `ZLIST_CutSequenceCommand`;
3. `ZSNDOBJ_SoundExtend`, `ZSNDOBJ_SoundNotify`, and
   `ZSNDOBJ_SoundSegment`.

This ordering removes the earliest fail-closed admission gate. Reversing it to
work on MovieControl phase two or event 16 first would not make normal startup
valid: the current runtime has 420 queued readers but only eleven source-backed
reader boundaries, so it has no complete owner/component population for the
global passes.

For the supported global initializer, the recovered structure is root pre-hook,
append-ordered additional-owner pre-hooks, reverse phase one, a fresh reverse
phase two, then root/additional-owner post-hooks. The synthesized ROOT is
separate from authored source 1, which reaches its owner pre-hook through the
additional-resource list. These are ordering constraints, not permission to
run the pass before every required reader and live service exists.

## First-cut component-reader boundary

The ordinary reader bracket already calls its boundaries in the recovered order:
prepare references, owner reader, then component reader. The first-cut owner
readers retain source/resource joins only. They do not parse attachment payloads
or imply component admission.

Known source facts are deliberately narrow:

- the sequence owner has one `ZLIST_CutSequence` attachment;
- the first-cut owner has `ZLIST_CutSequenceList`, followed by five
  `ZLIST_CutSequenceCommand` attachments in authored construction order;
- the generic deferred-reader session can preserve a bounded raw source block
  and dispatch an explicit component suffix/extent; and
- generic lifecycle callbacks for these components still fail closed.

The reviewed first-cut list owner-base envelope now proves one exact component
suffix and extent. `FirstCutOwnerReader` accepts only its fixed complete owner
block, validates the base envelope, and exposes the bounded six-component tail.
For the reviewed 171-byte owner form, the ordinary component-reader boundary
now parses the six payloads, retains their immutable component-owner state, and
cross-checks them against the independent GMS reader. The first-cut player
requires that state for the reviewed form. Other source forms remain outside
that route. This read-only boundary does not record component admission,
schedule an event, activate a cut, start audio, or submit a frame.

`BoundedComponentBlockCursor` now also exposes each delimiter-free attachment
payload, distinct from its compatibility `remaining()` view. The generic
dispatcher uses only this exact payload, so one reader cannot consume later
attachments or the owner terminator. The first ordered payload has a reviewed,
read-only parser for its fixed seven controls and one finite scalar. The five
following command payloads have equivalent bounded parsers. A separate atomic
read-only session validates the exact six-payload sequence and terminal, then
retains only parsed values. It is dispatched by the normal bracket for the
reviewed form and independently rechecked in the owned-data cold probe.

The next static-analysis pass recovers the six component-reader contracts and
their owner side effects. Its tests must
reject wrong work identity, attachment count/order, and malformed bounds; prove
that component reading cannot precede its owner reader; and prove that it has
no lifecycle, event, renderer, or audio effects.
