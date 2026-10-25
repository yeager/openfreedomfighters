# Light-owner ParamAnim deferred-reader specification

Status: behavior contract for independently implemented, bounded light-owner
and optional ParamAnim readers.
It does not authorize animation evaluation, scene activation, rendering, or
playback.

## Supported inputs

The reader accepts the complete loader-bounded deferred block, including its
four-byte DBLK total-length word. That declared extent must match the supplied
span. The owner body cursor starts immediately after the header. The suffix
returned to `DeferredReaderSession` must be a view into the session's copied
block and must end exactly at the end of that copy.

The supported owner families are spotlight, square spotlight, and
omnidirectional light. The owner class and attachment identities come from
the local parsed directory; record order and population do not select
behavior. This implementation may conservatively accept only owners whose
entire parsed attachment list is supported. In particular, an owner with any
unsupported sibling attachment remains unimplemented; the supported-class
contract must not be interpreted as asserting that no other owner can have
additional attachments.

The block is already bounded by the resource loader. Every cursor operation
must remain inside that span. A successful owner parse returns the remaining
suffix and an explicit extent for component dispatch. It must not infer a
component boundary from an observed block size or from a presumed row index.

## Owner-prefix behavior

The shared light prefix has a conditional compatibility section. If its first
value's low-six-bit class is 1 (binary64), consume two values through the
strict numeric float reader. Then inspect the next value's low-six-bit class:
exactly class 3 selects an integer-then-float pair; any other class selects a
float followed by three integer values. This branch selector is narrower than
the integer reader's accepted classes. If the first value is not class 1,
omit this compatibility section.

The strict integer reader accepts classes 3, 8, 10, and 11 and returns their
signed 32-bit values. The strict float reader accepts class 1 as binary64,
class 2 as binary32, and class 3 as a signed integer converted to binary32.
Other classes are rejected by these field readers. Tag class checks use the
low six bits; bits six and seven do not alter field selection. The optional
branch selector is exactly class 3, so classes 8, 10, and 11 select the other
branch even though the strict integer reader accepts them.

After that section, consume three required integer values. The last value
produces a typed `owner_flag_enabled` result from whether it is nonzero. When
committing that result to the owner's flags, clear its existing light-owner
option bit (`0x4`) and set it only when the result is true. The other consumed
values have no retained effect in this reader contract.

The base owner then scans through compact values to class 6 or the exact
terminal tag `0xff`. The separator class is identified from the tag's low six
bits; bits six and seven do not prevent recognition. On reaching a separator,
consume its tag. On reaching the terminal, leave it unconsumed. Spotlight
owners have one additional conditional section: if the next value is
binary64, consume three numeric float values; otherwise consume none. Those
values do not produce retained state. Spotlight then performs the same
separator scan. Square-spotlight and omnidirectional-light owners perform
that scan directly after the base prefix.

The component suffix begins at the cursor after this second separator, or at
the terminal if the scan reaches it first. Scanning follows the native compact
advance rules with explicit bounds checks: fixed-width values use their full
encoded extent: class 1 has an eight-byte payload; classes 2, 3, 8, 10, and 11
have four-byte payloads; classes 4 and 5 are NUL-terminated strings; and
classes 7, 9, and 12 use a checked declared extent. The length-delimited
extent includes its own four-byte word; the tag adds one more byte.
Reject a declared extent smaller than its length word, larger than the bytes
remaining after the tag, or one that would overflow cursor arithmetic.
Otherwise-unclassified value classes use the native one-tag-byte fallback
while delimiter-scanning only; this does not make them valid typed fields.
The bounded framing scanner used for owner and component delimiter scans must
implement that fallback without globally widening the general typed decoder.
A length-delimited value is opaque while skipping, so delimiter or
terminal-looking bytes in its body never end the scan. The scanner must not
search raw bytes for a delimiter.

Keep typed decoding stricter than framing: direct typed-field decoding still
rejects class 6 and otherwise-unclassified classes. The one-byte fallback is
only for the delimiter scanner, whose job is to find a bounded reader
boundary. This is a shared framing behavior change and requires its own
synthetic scanner tests before it can be used by component dispatch.

## ParamAnim component behavior

Dispatch the bounded suffix according to the parsed attachment identity and
preserve the existing `ParamAnimComponentPayloadReader` contract, including
its defaults, numeric conversions, and conditional reads. It returns the
component's neutral typed fields and consumed extent. Values left after
those fields remain available to the generic bounded dispatcher; they are not
silently claimed as ParamAnim fields.

The component's numeric helpers intentionally differ from the strict light
owner helpers. For component integer reads, binary64 and binary32 values
truncate toward zero to signed 32-bit; NaN and out-of-range conversion use the
signed 32-bit minimum result. Signed-integer classes are returned directly.
String classes 4/5 and length-delimited classes 7/9 advance the cursor but
yield zero for these numeric helpers. Component float reads cast binary64 to binary32, preserve
binary32, interpret the 32-bit payload of integer classes 3, 8, 10, and 11
as an unsigned value before conversion to binary32, and yield zero for string
or supported length-delimited values. Class 12 remains valid for generic
delimiter scanning but cannot satisfy a required ParamAnim field: that
component helper has no consuming advance for it. Reject it in required typed
positions, while permitting it in an unread trailing region handled by the
generic dispatcher. This differs from integer reads: a payload whose
signed interpretation is -1 converts to 4294967296.0F in a float field, while
an integer field retains -1. Preserve binary32 rounding of the unsigned value. A terminal or separator cannot satisfy a required
typed field. Use the component reader's existing typed state and defaults;
do not apply these permissive component conversions to owner fields.

Require dispatch coverage for every attachment in the accepted owner list and
a valid terminal continuation for the complete owner block. The supported paths are an empty attachment list or a single ParamAnim
attachment. No sibling reader is included in this contract. For an empty
list, dispatch with zero readers and require zero component delimiters before
the terminal. Retain only the owner flag and owner-reader identity; do not
create a synthetic component or ParamAnim payload state. Report this path as
a light-owner reader, distinct from the ParamAnim component path. Validate the full component slice with
the bounded generic dispatcher even when the ParamAnim reader consumes only
a prefix of that slice. Do not convert parsed values into animation updates,
callbacks, events, or lifecycle evidence.

Publish the owner flag and component state only after the owner prefix, the
complete component boundary, the ParamAnim payload, and the terminal
continuation have all passed validation. A failure must leave no reader
admission record or partially published typed state.

## Failure behavior

Reject an unsupported owner family, an extra attachment, a wrong
attachment identity, a truncated required value, a value of the wrong kind,
an invalid compact extent, a separator inside a typed field sequence, an
incomplete component dispatch, or a missing/invalid terminal. A terminal
before an owner separator remains the unconsumed suffix; if an attachment is
required, the later dispatch must reject the missing attachment boundary.
Keep the reader session failed after any such error. Unsupported data must
not be accepted by falling back to a whole-block generic scan.

## Synthetic validation

Use independently authored compact streams to cover:

- the shared prefix with and without its optional compatibility section;
- both branches of the compatibility section's type-dependent sequence;
- zero and nonzero final owner flag values;
- spotlight with and without its optional extension;
- square-spotlight and omnidirectional-light paths;
- separators between owner sections, a terminal at an owner boundary, and
  generic values before a separator;
- high-bit class-six separators, unknown one-byte values, and length-delimited
  classes 7, 9, and 12, with fake separator and terminal tags inside their
  opaque bodies;
- delimiter scanning that skips an unknown class by one tag byte while direct
  typed decoding continues to reject that same class and class six;
- an empty attachment list for each light family, including correct flag
  updates and rejection of unexpected component delimiters or malformed tails;
- a single matching ParamAnim attachment with values left after the reader's
  consumed prefix;
- component float fields receiving each integer class, including high-bit-set
  payloads and values near binary32 rounding boundaries; integer and boolean
  fields retain their existing signed conversion behavior;
- class-12 rejection in required component fields while preserving opaque
  class-12 framing in unread trailing regions;
- component strings and supported length-delimited fields yielding their documented
  numeric default while still advancing by their complete bounded extent;
- malformed lengths, truncated fields, wrong kinds, extra/missing
  attachments, bad delimiters, and invalid terminal continuation;
- atomic failure: neither the owner flag nor component state becomes visible
  when any later validation fails.

Successful tests prove only bounded reader behavior and typed state retention.
They do not prove animation playback, event behavior, component lifecycle, or
full intro compatibility.
