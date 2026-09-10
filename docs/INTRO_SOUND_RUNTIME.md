# Retained intro sound state

Normal startup constructs the two source-backed sound owners as part of the
complete intro directory. Their records are allocated cold only after their
construction rows in the normal, non-restore path; no source reader assigns
audio data yet. Each owner uses a lease on the application's canonical
`SoundRecordRegistry`. Prepared SND/WHD data stays immutable; source loading,
owner preparation, later component control and acknowledgement processing must
share the mutable record.

This is a logical backend with no output device. It does not make the intro play.

## Authored attachment data

Preparation reads and owns the four sound attachment groups in source order:
SoundExtend, SoundNotify, SoundSegment and ZSetZDefine. Each field validates its
supported full tag, including the observed alternate high-bit forms. All groups
and the outer block must terminate exactly. Numeric floats must be finite and
Boolean words must be 0 or 1 under the native parser policy.

Extend retains its six floats, four integer controls, Boolean, category, two
options and authored output enum. Notify retains separate authored target and
event references. Segment retains its event references, all four integer time
groups, probability, subtitle Boolean and owned string. ZSet retains its parent
selection and owned property key. Unknown enum values remain available for
explicit execution validation; they are not silently changed to defaults.

These are data reads, not component construction or callbacks. They do not
resolve runtime identities, apply Extend parameters, convert segment times,
register events, insert properties or send SoundReady. Wider nonzero control
branches still need their real runtime services before they can execute.

Preparation also derives the four segment times while retaining their raw words.
For each group, whole seconds are `((hours * 60 + minutes) * 60 + seconds)`
with unsigned 32-bit wrap. Whole seconds and fractional units convert separately
to binary32; the latter multiply by binary32 `0.04` before a separately rounded
addition. No time-field clamp or fused multiply/add is used. Tests cover integer
wrap, high-bit values and rounding boundaries in every group. This calculation
does not construct the component or execute its playback callbacks.

`IntroRuntime::apply_sound_extension` implements the verified unchanged intro
parameter subset against the owner's published canonical binding. A zero binding
skips all writes. A present binding applies the specified gain multiplier,
category-selection bit/category and output mode; authored option values of one
preserve the current option bits rather than forcing them on. Other parameter
branches reject before mutation. The method rejects reentry during owner
preparation and never announces readiness. Reader-completion and phase-one
lifecycle admission are still separate; normal startup does not call this method
automatically before those boundaries are connected.

## Ownership

The registry retains at most 1,024 records. Native bindings are monotonically
allocated, nonzero and never reused. They are separate from scene owner handles,
component serials, SND references and WHD links. Components are destroyed before
their scene's sound leases. Lease release invalidates the binding and removes
its prepared and pending-stop entries; late acknowledgement lookup then fails.
This is a native whole-scene teardown policy, not the original owner's full
deleting-disposal implementation.

An active source starts unassigned, distinct from an assigned null reference.
Normal source loading fills it and applies the authored values. The source's
range input is not pitch: it updates range and derived range using the current
gain multiplier. Unknown position/fade/final fields remain unassigned until
their specified writes. Neither loading nor construction marks an owner active.

## Initialization order

The explicit owner pre-hook stores raw application time, parent identity,
existing record binding and alternate source before testing live hide state.
If visible, preparation resolves the SND source, retains its exact duration,
selects state 7 or 10 from the backend category's live selection flag, and
appends the binding to the prepared sequence. The success path stores live
spatial data, applies the optional owner-enable operation, then marks it active.

All owner pre-hooks run in forward owner order before either reverse component
phase. The host exposes the concrete sound-owner operation, but normal startup
does not invoke it yet: the complete global traversal, live resource flags and
component implementations remain missing. Required live services are explicit;
authored flags are not substituted for runtime flags.
Live callbacks must preserve host/application lifetimes. Recursive preparation
or stop through the same host is rejected; this is not a mutation-safe traversal.

## Component lifecycle contract

The two authored sound owners have a recovered ordering contract. It is a
contract for a complete lifecycle implementation, not permission to invoke one
callback from the current startup path.

1. Run both owner pre-hooks in forward owner order. Each one needs its live
   canonical record, assigned source, retained bank, clock, parent, flags and
   spatial services before it can publish its prepared state.
2. Run the first component phase in reverse owner order: owner 468, then owner
   467. Within an owner, invoke `ZSetZDefine`, `SoundSegment`, `SoundNotify`,
   then `SoundExtend`.
3. Complete every first-phase callback before starting the second phase.
4. Run the second phase in the same reverse owner order. Within each owner,
   invoke `SoundSegment`, then `SoundExtend`.

`SoundNotify` first phase snapshots the duration from the live canonical record
and rejects a missing record; it never sends readiness. `SoundExtend` first
phase applies only its proven unchanged parameter subset after its ordinary
membership rule, while its second phase is a no-op. `SoundSegment` first phase
only queries restore mode; its second phase additionally requires live hide,
random, record and complete disposal/stop services. `ZSetZDefine` needs a
property store with independent key/value ownership and complete component
retirement semantics.

The generic lifecycle deliberately rejects these callbacks today. Enabling only
the sound callbacks would violate the all-or-fail global pass and could invent
playback or readiness, so native startup keeps this contract unreachable until
the complete lifecycle and output-channel services are admitted.

The concrete owner-reader boundary is now retained for both sound owners. At
the matching processed deferred record it first confirms the four
delimiter-bounded payloads beginning at the parsed owner-prefix boundary, then
applies the already parsed owner prefix to that owner's freshly allocated
canonical record exactly once and copies each attached component's
parser-validated fields into its cold runtime state. A scene without a record
backend still retains those component fields but does not mutate a record. This
reader does not resolve authored event or target
references, prepare a record, allocate a channel, register component events or
enter either lifecycle phase.

An explicit isolated phase-one probe now verifies the recovered order for these
two owners only: reverse owner order, then `ZSetZDefine`, `SoundSegment`,
`SoundNotify` and `SoundExtend`. It requires completed readers and owner
pre-hooks, snapshots the live duration, applies the approved Extend subset and
retains the defined property. It never invokes generic component callbacks,
sets global phase flags, retires components, emits readiness or starts a sound;
normal startup does not call it.

The probe intentionally stops before second phase. `ZSetZDefine` retirement
belongs to the complete global traversal: the property survives independently,
but component disposal, attachment unlinking and serial-map removal cannot be
substituted by clearing retained metadata. `SoundSegment` second phase needs a
live hide flag, native random source, ordinary-membership operation and actual
owner deleting-disposal; a binding stop alone is not equivalent. `SoundExtend`
second phase has no direct work, but its lifecycle completion bit still belongs
to the same global traversal. These paths remain fail-closed until their real
services and the full first pass exist.

Failures keep completed mutations. Where the original would destroy an owner,
the current host reports unsupported disposal and prevents further sound-owner
use. It must not continue initialization as if destruction had succeeded.
Additional backend preparation mode is explicitly unsupported.
Its mode Boolean is the same retained field that suppresses category-volume
state-5 traversal, not a separate copy of that state.

## Volume, stop and acknowledgement

The application's default sound-preference resolver targets this same registry.
Mode 0 uses the signed volume directly as a linear response; it does not clamp.
Mode 2 uses an independently reconstructed integer response, verified against
all 101 inputs from 0 through 100. Values outside that range are not clamped.
Category multipliers use separate binary32 operations. A positive selection
transition can change matching prepared records to state 5. The backend retains
the pending-volume flag for its future consumer; this is not an audio command.
Explicitly supplied external preference backends remain an injection boundary,
not a second canonical record store.

The backend initialization contract requests sound-effects, music and speech
categories (0, 1, 2), in that order, using mode 0 when preferences are present.
It then attempts device initialization. The registry supports those volume
operations, but the startup caller and output-device initialization are not yet
connected. Category 3 is not implicitly initialized, and a volume request never
opens a device or enables command processing.

Binding stop and owner disposal are different operations. Stop can append a
pending stop, remove one prepared entry and return the live record to state 3;
it does not free the owner or replace source tokens. Prepared and pending-stop
sequences allow duplicates. Removal replaces the first match with the last entry.

The start-acknowledgement receiver updates shared progress and duration using
the raw 1,024-unit clock and ordered binary32 arithmetic. Missing bindings are
ignored; a zero elapsed interval stays zero. No production caller generates
this acknowledgement yet. Parsing, successful decoding, elapsed duration and
the splash timer must never generate it. The real channel/stream service and
SoundNotify's ordinary event route still need integration.

An [incremental Vorbis input path](INTRO_AUDIO_STREAMING.md) now retains real
encoded input and PCM across worker requests. It does not itself admit a channel
or produce an acknowledgement.

## Verification boundary

Independent fixtures cover source assignment, shared record identity, volume
transitions, explicit live pre-hooks, stop membership, pool exhaustion, stale
bindings and teardown. Owned-data probes verify the actual two intro sources
through the same public loaders. These checks do not establish audible output,
automatic global initialization or completed intro playback.
