# Particle template deferred reader

This contract covers one bounded particle-template owner block and the live
parameter state it produces. It does not start emitters, dispatch events,
admit a global scene lifecycle, or claim that particles are rendered.

## Input and field order

The input is one complete DBLK, including its four-byte total extent. Require
an exact extent match and consume the compact body through its exact terminal.
Reject malformed values and unexplained trailing bytes before publishing any
state. This owner has no attachment payload to dispatch.

Read the body in this order:

1. A three-float vector, then five scalar floats.
2. Two angle floats in degrees, converted to radians using binary32
   multiplication by the binary32 value of pi/180.
3. If the next value's class is neither 4 nor 5, read two more degree angles
   and convert them the same way. Otherwise those two angles are zero and the
   string remains for the following field.
4. An optional class-3 integer boolean, defaulting to true when that class is
   absent. Other classes remain for the following field.
5. A palette-resource string (class 4 or 5), then a required integer mode.
6. An optional class-4 string, retained only when nonempty; an optional
   class-3 integer consumed without retaining its value; an optional class-2
   scale float defaulting to one.
7. An optional class-2 block of 32 floats representing eight RGBA keys. If
   absent, all keys are zero.
8. A final integer setting, followed by the exact block boundary.

Keep neutral names for fields whose gameplay meaning is not established.
Integer and float conversions follow the existing compact reader conventions:
integer classes preserve their 32-bit payload, integer-to-float uses the
unsigned payload, and string classes 4/5 plus length-delimited classes 7/9/12
convert to numeric zero while consuming their complete bounded encoding.
Unknown classes, separators and terminators in required fields fail.

## External palette text

An empty resource name uses the inline keys, or eight zero keys if the inline
block is absent. A nonempty name is passed unchanged to an injected
project-content file-service adapter. Do not interpret the name as an
arbitrary host filesystem path. The adapter owns the configured
project-content prefix and reproduces the native lookup: prepend the prefix
for the original full path, strip that prefix case-insensitively for
registered-provider lookup, remove one leading backslash, and select the
first registered provider that contains the resulting relative key. If that
provider's read fails, fall back to the original full path as a loose file;
do not try later providers. The exact order in which packages register
providers remains unverified. The adapter receives the authored name so it
can preserve both the transformed provider key and the original fallback
path. It must use the selected install's project-content namespace, not the
current scene ZIP's directory.

For bounded host parsing, accept eight rows of four comma-separated finite
ASCII decimal numbers. The maximum input size is 16 KiB. LF and CRLF line
endings are accepted. Reject missing rows, missing channels, invalid numbers,
extra non-whitespace data, and input above the documented byte limit. A
provider or parse failure leaves the
template unpublished. This is the host's safe resource policy; it does not
claim the native reader used the same error handling or text-library locale.

## Derived vector and palette

Clamp the first scalar to a minimum of 0.01 using the native unordered-aware
comparison: NaN and positive infinity remain unchanged, while negative
infinity is clamped. Compute the reciprocal and multiply each component of the
initial vector by that reciprocal, preserving separate binary32 operations.
Retain the resulting three-vector in live template state. Other nonfinite
compact floats flow through interpolation and packing; do not reject them
solely because they are nonfinite.

Generate 128 palette entries. For entry `i`, compute `position = (i / 127) *
7` in binary32, select the lower key by truncating the position, cap the upper
key at seven, and linearly interpolate each RGBA channel. Multiply each
interpolated channel by binary32 `1/255` before color packing.

The built-in D3D primitive-control service accepts four binary32 channels in
RGBA order and returns a packed `0xAARRGGBB` word. For each channel it multiplies
by binary32 255 and truncates toward zero. Red, green and alpha use a negative
255 multiplier followed by a negated contribution at shifts 16, 8 and 24;
blue uses positive 255 at shift zero. Contributions are added modulo 2^32.
There is no saturation or per-channel mask, so out-of-range values can carry
between packed channels. NaN and values outside signed-32 conversion range
produce the signed-32 minimum conversion result. Preserve the caller's prior
binary32 scaling before packing.

This color-service contract is established for the built-in D3D route. Do not
claim another renderer backend uses the same service until its implementation
is checked. An implementation that cannot provide the matching service must
leave the template unready rather than fabricate a packed palette.

## Deferred relocation boundary

The loader relocates class-8 and class-10 values throughout the complete
deferred block before owner readers run; separators and unread tails do not
stop that pass. Class-9 and class-12 containers can carry nested references.
The detached parser must retain each consumed value's class and raw payload so
numeric conversion is not confused with runtime relocation.

Until this host has a typed resource-handle representation for every field,
production admission rejects nonzero class-8 or class-10 values in all
ParticleTemplate fields. Zero values are safe. Reject nonempty class-9 or
class-12 payloads until their nested relocation grammar is implemented. This
is a bounded host admission policy, not a claim that the native reader rejects
those values. Perform these checks before publishing the template or any
registry/resource side effects.

## Publication and tests

Publish the owner's retained fields, derived vector, and all 128 packed colors
only after the complete block, provider read, text parse, interpolation and
packing succeed. Numeric or text failures leave no typed state or admission
receipt. A reader receipt alone does not mean particle emission or rendering is
active.

Synthetic tests should cover optional angles and boolean defaults, string
boundaries, all numeric classes, binary32 operation order, inline and external
keys, endpoint and midpoint interpolation, 0.5 quantizing to 127, the
no-clamp arithmetic packing behavior, malformed text, bounded provider
failure, malformed DBLK extents, trailing data, relocation provenance,
unsupported nested relocation containers and no state publication after late
failures. Tests must use synthetic bytes and an injected provider.
