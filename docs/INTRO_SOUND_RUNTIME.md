# Retained intro sound state

Normal startup creates source-backed sound owners in `IntroRuntime`. Each owns
a lease on the application's canonical `SoundRecordRegistry`. Prepared SND/WHD
data stays immutable; source loading, owner preparation, later component control
and acknowledgement processing must share the mutable record.

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
