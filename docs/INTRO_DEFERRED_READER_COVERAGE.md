# Intro deferred-reader coverage frontier

Status: source-free planning record. This document reports only the aggregate
inventory exposed by the supported cold probe; it contains no retail payload,
source identity, byte sequence, or decoded authored value.

## Highest-frequency unimplemented candidate

`ZGEOM_ParamAnim` is the next investigation target. The current aggregate
inventory reports 11 attachment owners, 11 attachment instances, and 11 owners
with a queued deferred block. Those blocks share one bounded structural shape
in the supported probe. The separately inventoried `ZGEOM_ParticleEmitter`
family has deferred-block owners, but the public inventory intentionally does
not establish a single dominant shape or a larger required population.

This is a prioritization result, not reader admission evidence. The current
ParamAnim inventory retains compact framing statistics only. It does not prove
the owner payload grammar, the component payload grammar, decoded field
meaning, destination state, mutation timing, callback registration, or any
activation/playback effect. The constructor's empty ParamAnim storage is not a
valid inferred destination.

## Implementation decision

Do not implement a ParamAnim reader yet. A parser that accepts the observed
shape and writes constructor storage would classify or route data by guesswork,
and could accidentally make an ordinary or lifecycle path appear covered.
Existing behavior remains fail-closed: the family is constructor-only and its
generic callbacks reject when reached.

## Required next evidence

A reviewed, source-free observation must establish one concrete reader form
before implementation. It must state:

1. The exact bounded owner and component input forms, including terminal and
   attachment-delimiter rules, accepted/rejected variants, and trailing-byte
   policy.
2. The exact portable destination contract: fields written, their raw-value
   preservation rules, ownership, duplicate/re-entry behavior, and rollback
   on failure.
3. The lifecycle boundary: whether the write occurs at the owner reader or
   component reader, required ordering relative to deferred preparation, and
   which later callback (if any) may consume the resulting state.
4. Negative observations proving that malformed input and unsupported owner or
   attachment combinations reject before state mutation, with no event,
   rendering, activation, or playback side effect.

Only after all four are reviewed can a bounded reader be added with admission
and failure tests. That implementation must preserve the existing separation
between reader-local state and global lifecycle completion.

## Private observation schema

`tools/paramanim_deferred_reader_observation.py` is the only repository-side
format for collecting the missing evidence. It accepts a private JSON file and
produces a new private JSON file; both paths must be outside the repository and
the output is never overwritten. It does not inspect executable or asset data.

Each bounded observation record carries only these categorical relations:

- owner and component input form: `accepted_bounded`,
  `rejected_malformed`, or `rejected_unsupported`;
- terminal rule, attachment-delimiter rule, and a no-trailing-bytes policy;
- reader boundary for a destination write, the required deferred-preparation
  and owner-reader prerequisite ordering, raw-value preservation, local
  ownership, and duplicate/re-entry behavior;
- rollback/no-write behavior, a possible later-callback consumer, outcome, and
  the required `none` side-effect category.

The sanitizer requires exact keys and bounded enum values. It rejects every
other field, including IDs, arbitrary strings, paths, assets, bytes, addresses,
offsets, symbols, and screenshots. The enum labels are schema categories, not
captured source strings. Rejected grammar forms must report failure, no write,
no side effect, and `no_write` rollback. A reported write must occur at the
declared owner- or component-reader stage and include complete grammar,
preservation, ownership, and re-entry evidence. A later consumer may be
reported only at the later-callback stage.

For a reported write, the schema also requires the complete prerequisite
relation: deferred preparation precedes an owner-reader write; deferred
preparation and the owner reader precede a component-reader write. This is a
source-free ordering fact, not an inferred callback or activation rule.

This schema records evidence for review; it neither admits a reader nor changes
ParamAnim construction, lifecycle, rendering, activation, or playback.
