# Intro bootstrap evidence and implementation gates

The original intro is not yet activated by the native runtime. Normal startup
prepares the verified UI resources and clears the window; the optional
diagnostic world preview does not establish bootstrap order. The project-owned
three-second splash is a separate presentation requirement, not a cut-sequence
clock or a substitute for original completion events.

## Independently reviewed behavior

Private research of the supported Steam build, reviewed on 2026-09-06, establishes
the following bounded controller contract without publishing original code or
serialized asset payloads:

- The controller has ordered sequence and group references, another field whose
  role remains unresolved, a destination string, a serialized boolean and up to
  two optional references. Optional references default to zero.
- Destination storage accommodates at most 99 bytes plus a terminator. A native
  decoder must reject overlength input rather than depend on original assertions.
- A separate runtime flag selects a fixed destination. It is not the serialized
  boolean; the two must remain distinct.
- Initialization resolves the lists and starts at the first sequence item.
- A matching registered completion event increments the sequence index. If
  another item remains, the controller requests its start and issues group-state
  requests according to the selected index. Exact visibility semantics remain
  unproven.
- Exhaustion clears the scene stack and requests the configured destination,
  unless the separate fixed-destination runtime flag selects its other branch.
- Start and completion events have symbolic registrations. Numeric IDs from an
  original process are not portable event constants.

Input-driven skip producers exist, but their complete guards and policy are not
recovered. Natural completion timing and the readiness producer remain unresolved.
The authored destination must not be replaced with an assumed direct main-menu
transition.

### First-cut execution boundary

The controller's initial-start path is gated by a readiness notification and an
activation latch. It prepares sequence components, records engine-clock state
and advances the first cut. The notification producer and its prerequisites
remain untraced; splash expiry must not synthesize readiness.

The first cut has attached sequence-player and command components. It is not an
attachment-free reference list, even though it shares the same base source
class. Applying the restricted list reader to it must fail its identity guard.
The player retains the caller of its start request. Cleanup clears execution
state, sends completion to that retained caller when present, then clears the
caller reference. Natural timeline update can invoke cleanup, but its end-value
derivation, units, clock conversion and command dependencies are not yet fully
specified. This is evidence of an execution path, not a native playback contract.

## Restricted controller reader

`GmsImage::intro_movie_controller_source` decodes only the reviewed supported
intro component shape. The caller must establish supported intro provenance;
the method additionally checks the source class, single attached component
identity, zero attachment parameter, empty base-list wrapper and bounded tagged
block. It is not a generic component dispatcher and does not select a scene.

The result retains raw sequence/group/unknown references, raw destination bytes,
the authored option value and presence-preserving optional references. It does
not resolve those references through the unrelated runtime object-handle decoder
or equate the authored option with the fixed-destination runtime flag. Destination
bytes are not assumed to be UTF-8 or a filesystem path.

The reader rejects unsupported full tags, truncated fields, overlength or
unterminated strings, invalid wrapper/header forms and any trailing bytes inside
the declared block. Padding outside the declared block is left untouched. Zero,
one or two optional tokens are accepted in order; the shorter forms follow the
reviewed scalar-reader behavior, not additional observations of retail objects.
Other high-tag-bit combinations are unsupported by this reader, not proven
invalid in the original format.

Private verification on the owned intro archive successfully validates GMS/BUF,
finds exactly one component accepted by these guards and compares every returned
field against the reviewed authored-data observations. Neither the payload nor
its field values are emitted to public artifacts. This demonstrates field
decoding, not execution of the controller.
Public GMS tests cover all three optional shapes, raw bytes, string limits,
identity guards, malformed tags and every declared truncation of the compact
fixture. The full 35-test suite and targeted GMS ASan/UBSan run pass locally.

## Authored source lookup and list decoding

The reviewed deferred-load relocation builds its map in source-directory order,
before deserializing recorded tagged blocks. It does not use runtime pool order,
class ordinals or sorted names. `GmsImage::local_source_for_authored_reference`
provides the corresponding immutable identity lookup for source-reference scalar
and raw-list fields on this path. It does not mutate the original payload or
manufacture a runtime handle.

Only bit 31 is removed from the authored word. A nonzero resulting index selects
the preceding zero-based directory entry; both marked and unmarked indices are
eligible in this deferred-load mode. Raw zero returns no source. A high-bit-only
word is rejected because relocation leaves it unchanged and its later runtime
meaning is not established as null. Out-of-range indices are also rejected:
this is a deliberate safety policy instead of the original diagnostic-and-zero
behavior. Bit 30 is not another removable marker. The API is not a replacement
for runtime registry or object-slot lookup, nor a general interpretation of every
integer or reference family in a tagged stream.

`GmsImage::intro_source_reference_list` checks the reviewed attachment-free list
class shape, nonzero deferred offset, bounded header and exact raw-list framing.
It reads the inclusive byte count with alignment and subtraction-based bounds
checks, requires exact closing markers, and leaves external padding untouched.
Returned words retain their authored order, duplicates and zero or unresolved
values. Reading a list does not automatically resolve its entries, filter classes,
enforce equal sequence/group counts or create timings.

Together these APIs allow the controller's list references to identify the
actual source lists without display-name selection. General component dispatch,
successful runtime object creation and list-entry activation remain separate.
Private verification follows both references from the decoded controller and
compares the complete ordered lists, every resulting target index/type and the
optional source join against independent owned-data observations. No expected
retail vectors are published. Public fixtures exercise tagged and untagged
boundary indices, directory-versus-pool order, raw-list preservation and malformed
headers/counts/tags/termination. The 35-test suite passes with these additions.

## Required before activation

1. Extend component dispatch beyond the restricted intro shape only after its
   attachment order and wrapper rules are established. Preserve unknown values
   rather than assigning invented semantics.
2. Establish sequence-player payloads, factory initialization and resource
   dependencies beyond the decoded list identities. Resolving a source does not
   prove its runtime object was constructed or activated.
3. Recover configuration precedence and scene-name-to-resource resolution. The
   constructor default alone does not prove which scene an actual launch selects.
4. Recover initial activation, natural completion, timing dependencies and skip
   behavior. No guessed duration may drive progression.
5. Integrate with one scene-session owner for stack changes and load results.
   There is currently no native scene stack or script/event host. The generic
   simulation event queue provides ordering, not original event meanings or a
   proven intro update phase.

### Timing and event delivery

Do not route cut completion through the current fixed-step world queue merely
because it already transports events. That queue accepts only future ticks;
using it would introduce delivery latency if the original cut/controller
exchange is synchronous. The project's scheduler also clamps elapsed time,
limits catch-up steps and drops excess time. Those policies are not the
[recovered variable-delta service](TIMING_EVIDENCE.md).

The cut-player boundary must take explicit compatibility-time inputs until its
actual producer is established. Research must identify the selected clock
branch, start/reset/update ordering, completion comparison, scaling and
pause/load/focus suppression, as well as synchronous versus deferred delivery.
Do not convert authored durations to a guessed number of 60 Hz ticks. Original
and Modern should consume the same established compatibility clock and event
state; sharing graphics modes does not justify changing intro timing.

Each decoder requires public malformed-input tests and private verification on
the owned installation. Public fixtures must be independently authored, not
copied controller payloads. Runtime acceptance additionally requires an actual
intro-to-menu transition; parser success does not satisfy it. Original Windows
capture is currently unavailable, so camera and final pixel fidelity remain
unverified even as static research and native implementation proceed.
