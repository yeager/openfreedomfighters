# Intro renderer association boundary

The supported first-cut picture is a real, source-backed scene resource after
the GMS construction pass. Its owner, resource, authored parent link, picture
descriptors, texture bindings, and decoded image data are retained by
`IntroRuntime`.

That fact does not make the picture a renderer record or a drawable frame.
The loader also retains a separate renderer-resource payload. Its
[authored relation lists](INTRO_RENDERER_RELATIONS.md) now have a concrete
scene-owned reader and typed queries. Their members are canonical resources,
not generic draw records. The payload's remaining workspace slots must not be
interpreted as a draw-record grammar.

## What is implemented

- GMS construction creates the real legal-picture owner/resource association.
- The renderer relation container retains copied source/workspace bytes and
  ordered member lists, queried through each live resource's current selector.
- The source-backed image and descriptor data can be prepared and uploaded by
  `SdlIntroRenderer`.
- `IntroFirstCutAcceptedPictureRegistry` retains a generic record only when a
  caller separately proves it belongs to that live legal picture.
- Camera registry replay, state-zero admission, pending-camera handling, and
  view ordering have explicit bounded models.

Relation membership does not establish traversal eligibility, a generic draw
record, texture residency, device acceptance, or presentation.

## Missing producer

The missing association is:

```text
bounded GMS picture resource
  -> decoded authored resource relations
  -> live traversal eligibility and record production (remaining)
  -> paired submission record
  -> accepted generic draw record
  -> ordered GPU submission
```

The accepted-picture registry is deliberately downstream of that missing
producer. Its generic record identity, owner-context identity, and renderer
resource identity are opaque values; they are never inferred from directory
indices, typed owner handles, or texture IDs.

## Recovery requirement

A draw-record producer may be added only after private clean-room research
establishes its complete source/runtime contract. For a serialized record family,
that includes:

1. A cursor or offset inside the bounded retained payload.
2. A finite count or terminator and its end rule.
3. Every consumed byte, including alignment and optional branches.
4. The runtime object created or updated from that record.
5. Rejection when the record would cross the payload boundary.

If records are constructed from runtime state rather than serialized, recover
that constructor, its consumers and ownership instead of inventing a payload
layout. Dynamic relation maintenance and the separate outer-tail List
associations also remain unfinished.

Until those prerequisites are connected, normal startup must leave first-cut
playback disconnected. A populated relation container, guessed record layout,
or preview camera is not a substitute for an accepted original draw path.
