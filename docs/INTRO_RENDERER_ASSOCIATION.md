# Intro renderer association boundary

The supported first-cut picture is a real, source-backed scene resource after
the GMS construction pass. Its owner, resource, authored parent link, picture
descriptors, texture bindings, and decoded image data are retained by
`IntroRuntime`.

That fact does not make the picture a renderer record or a drawable frame.
The original loader also retains a separate renderer-resource payload. Its
outer envelope is bounded, but no complete internal record family has been
recovered. In particular, the leading payload words cannot safely be treated
as a count, stride, or generic record header.

## What is implemented

- GMS construction creates the real legal-picture owner/resource association.
- The source-backed image and descriptor data can be prepared and uploaded by
  `SdlIntroRenderer`.
- `IntroFirstCutAcceptedPictureRegistry` retains a generic record only when a
  caller separately proves it belongs to that live legal picture.
- Camera registry replay, state-zero admission, pending-camera handling, and
  view ordering have explicit bounded models.

None of those boundaries manufactures renderer membership, a generic draw
record, texture residency, device acceptance, or presentation.

## Missing producer

The missing association is:

```text
bounded GMS picture resource
  -> renderer-payload association and traversal eligibility
  -> paired submission record
  -> accepted generic draw record
  -> ordered GPU submission
```

The accepted-picture registry is deliberately downstream of that missing
producer. Its generic record identity, owner-context identity, and renderer
resource identity are opaque values; they are never inferred from directory
indices, typed owner handles, or texture IDs.

## Recovery requirement

A renderer-record decoder may be added only after private clean-room research
establishes, for one complete record family:

1. A cursor or offset inside the bounded retained payload.
2. A finite count or terminator and its end rule.
3. Every consumed byte, including alignment and optional branches.
4. The runtime object created or updated from that record.
5. Rejection when the record would cross the payload boundary.

Until then, normal startup must leave first-cut playback disconnected. An empty
renderer container, guessed record layout, or preview camera must not be used
as a substitute for an accepted original draw path.
