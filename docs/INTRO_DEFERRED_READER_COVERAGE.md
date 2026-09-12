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
