# Cut timeline conversion and execution boundary

The native `off::cutscene::timeline_position` function converts an explicit
current/start scene-clock word pair to the cut player's timeline coordinate.
It does not sample a host clock, select a rate, activate a player or dispatch
commands. The [scene-clock evidence](TIMING_EVIDENCE.md) and
[intro bootstrap gates](INTRO_BOOTSTRAP.md) remain separate.

## Reviewed conversion

1. Subtract the start word from the current word using 32-bit unsigned arithmetic.
2. Interpret the resulting bits as a signed 32-bit value.
3. Multiply by 25600 in signed 64-bit arithmetic and arithmetic-shift right ten.
4. Retain the low 32 bits and interpret them as signed.
5. Convert to float, then multiply by exactly 1/1024 at float precision.

The product fits signed 64-bit range. In this particular expression, 25600 is
divisible by 1024, so the shift loses no fractional information. The subsequent
low-word retention and float conversion are observable and must not be replaced
by an unbounded double-precision elapsed-time formula. Deliberate bit operations
avoid C++ signed-overflow undefined behavior. They reproduce this conversion;
they do not establish overflow policy for the upstream engine clock.

Tests use explicit numerical inputs, not invented retail captures or wall-clock
timings. Boundary vectors cover zero, negative differences, subtraction wrap,
signed extrema, multiplication's retained low word and integer-to-float rounding.
The 36-test suite and targeted converter ASan/UBSan run pass locally. These checks
validate arithmetic, not an original-clock capture or rendered intro.

## Execution ordering still under research

The recovered active-update phases visit member starts, then member ends, then
overdue commands. Member activation/deactivation returns before the corresponding
fired flag is stored. A command executes before advancing its cursor. Both member
boundaries and command schedules use strict greater-than comparisons.

Cleanup is checked after those phases. An end request made by an earlier callback
can therefore affect the same update. The subsequent end-condition calculation
can set a new end request without immediately invoking cleanup. Start and cleanup
also have post-callback state writes. These facts do not justify transactional,
deferred or non-reentrant behavior without further evidence.

Command insertion is not a proven stable sort: the observed ordered container
places equal keys before its selected equal candidate, while actual lifecycle
registration order remains unresolved. Parsed attachment order must be preserved
as data, but must not automatically become a claimed execution order. Complete
cursor behavior, admission branches, callback delivery and resource
activation still block a faithful complete player. No 60 Hz queue or splash timer
is substituted for those contracts.

## Bounded admitted command pass

`off::cutscene::CommandPass` owns copies of commands supplied in an explicit
registration order. It implements only the command phase of an already-admitted
update. It does not initialize a scene, start cut members, resolve targets, issue
global events or perform end-of-cut cleanup. A synchronous visitor receives each
due command and its original registration index.

Registration skips signed-negative schedule words and preserves the recovered
cached-node insertion behavior using float schedule keys. It is neither a stable
sort nor an unconditional reversal of equal schedules. The constructor validates
the finite derived end and both sentinel conversions: add the constant at float
precision, then truncate to signed integer with explicit range checks.

Start/reset leaves the cursor absent and next schedule zero. A strict comparison
selects the first node, then rechecks its actual schedule before any callback.
Each due callback returns before the cursor advances. Ordinary return consumes
the command even when the visitor found no target. Empty and exhausted cursors
retain their respective sentinel states rather than a fabricated permanent-done
flag.

The native supported subset requires a finite position strictly below the empty
list's converted sentinel. This intentionally excludes extreme-time reinitializing
loops; rejection must not be described as original behavior. Non-finite or
out-of-range sentinel conversions and empty visitors are rejected before running
the phase. This is a validation boundary, not a host-clock clamp.

Commands and their list remain immutable during callbacks. Recursive `run` or
`reset_start` calls are rejected; moving/copying the pass is disabled, and callers
must keep it alive through dispatch. These are native safety constraints, not
proof that original callbacks never mutate or recurse. If a visitor throws, the
selected command remains current for a later retry and successful earlier
callbacks remain consumed. External callback effects are not rolled back.

The separately reviewed successful, live and unmutated phase-one/phase-two
lifecycle path registers commands once in authored attachment order. This permits
a conditional owned-data trace through the pass, but actual startup entry into
that lifecycle path remains unproved. An explicit test-clock trace is not an
original runtime capture or a playable intro.
Private verification follows the decoded controller, lists, first cut and
subordinate resource, runs explicit test positions through the command pass, and
compares all conditional visitor indices, event names, target identities and
arguments. Zero/equality boundaries, later advances and reset/overdue batches
match the reviewed expectations. The 37-test suite passes with sentinel-rounding,
ownership, exception-retry and reentrancy regressions. No retail trace values are
published and visitors do not yet apply rendering or audio effects.
The targeted command-pass ASan/UBSan run also passes locally.

## Direct event delivery evidence

The traced command-owner direct-target route invokes the target object's event
hook first, then its shared component dispatcher, before returning to the caller.
It passes a 16-bit registered event identifier and an argument word. This route
does not enqueue a later frame. It is distinct from a reference-target helper
which first resolves a runtime reference; neither requires relocating an authored
GMS source reference again after target resolution.

After the target hook returns, the component dispatcher collects currently valid attachment handles into an
ordered snapshot, removing stale source entries. It re-resolves each snapshot
handle before its turn and skips entries that no longer resolve. Additions during
a component callback are not appended to that existing snapshot; removals can
prevent a later callback. After a component callback it also re-resolves the owner
and stops if the owner no longer exists.

Component lifecycle/readiness/disable guards still apply. Their complete rules,
target-specific hook behavior, post-callback maintenance and recursive mutation
contracts remain unresolved. The traced boundary has no general recursion guard:
a native prohibition would be a safety policy, not original behavior. Inline
invocation does not prove that every target hook completes all side effects
immediately or that no other route schedules work.

This establishes synchronous invocation on the traced shared route, not a
universal event system. Post-load command registration order still depends on
phase invocation and eligibility. A complete player cannot yet be claimed from
these dispatch observations.

## Construction and deferred payload order

The reviewed successful-construction path visits the authored attachment table
from first to last and creates each component through an append operation. It
does not sort by component name or scheduled command time. A later loader pass
consumes the object's base payload, then the component payloads in that same
runtime attachment order. The first-cut sequence component and its command
components therefore retain their authored construction/deserialization order.

This does not prove every component was successfully created or eligible for its
post-load callback. Command registration occurs in a later lifecycle phase;
the scheduler's invocation of that phase, first-cut admission and component guards
must still be joined to this ordering before asserting an actual execution trace.
