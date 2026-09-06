# Restricted intro camera source

`GmsImage::intro_camera_source` reads the reviewed camera-family tagged block
referenced by the supported intro's first cut. The caller establishes supported
intro provenance. The reader additionally requires the exact source class, zero
class data, no attachments, a nonzero deferred offset and a bounded fixed-length
block with the reviewed full tags and terminal. It is not a generic camera loader.

The returned structure preserves authored double distances and angle, raw RGB
integers, auxiliary scalars, priority, renderer-list selector, aspect mode, opaque
flag options, viewport tuple and final boolean word. No integer field is silently
normalized to a boolean or truncated to a color byte. The angle remains degrees.
Binary32 fields and binary64 fields retain their original precision and signed
zero. Nonfinite values are rejected. Doubles outside the finite binary32 range
are conservatively rejected before narrowing as an explicit native policy; no
runtime float conversion occurs inside this decoder.

All fields and closing markers are mandatory. Other full-tag variants, shorter
optional forms and trailing bytes inside the block are unsupported, not asserted
invalid across all original camera classes. External padding remains untouched.
Malformed input never supplies guessed camera defaults.

## Separate runtime boundaries

The original camera reader narrows near distance and clamps it to at least one;
the later projection helper separately clamps near to at least five. Angle
conversion narrows to binary32 before multiplication by binary32 pi and division
by 180. These operations must not be collapsed into the raw source reader or
confused with feeding degrees directly into a tangent operation.

Authored viewport coordinates are pass-relative. Their presence does not prove
fullscreen rendering, a selected camera or a particular display resolution.
Aspect-policy branches, viewport composition, renderer registration, camera
collection traversal and final pass admission remain separate integration work.
Likewise, renderer-dimension updates are not proof of active-camera selection.

The private probe follows the actual controller and first-cut resource reference
to its camera source and compares every decoded field against independently
reviewed owned-data observations. Public fixtures use independently authored
values and malformed cases; no original camera payload or private expected vector
is published. Decoding those real fields moves camera integration forward but
does not activate the intro or establish Original visual parity.

Verification: all 40 CTest executables pass locally, including camera tests for
every field, signed zero, retained double precision, range boundaries, malformed
full tags, source guards and every declared truncation. The targeted GMS
ASan/UBSan executable and the owned intro probe also pass.

## Conditional mode-zero conversion

`graphics::convert_intro_camera_mode_zero` now converts a newly constructed
camera's reviewed aspect-mode-zero, renderer-list-zero fields. The authored
structure is retained separately. Near distance is narrowed then bounded to one;
far is narrowed without repairing ordering. Angle conversion explicitly rounds
the narrowing, pi multiplication and division separately to binary32. Combining
the operations into a constant would change some results. RGB packing retains
the low byte of each integer, with a zero high byte. Registration priority is
interpreted as signed before float conversion, and the final boolean uses nonzero
truth; opaque flag options do not mutate runtime flags.

The converter retains viewport composition arithmetic from the constructor tuple
even when it appears to be an identity operation, preserving its signed-zero
behavior. It computes the stored height/width ratio separately. Nearest-even
rounding, finite representable arithmetic and positive extents are explicit native
policies; other aspect modes and renderer-list selectors are unsupported. The
converter returns no partial state on rejection.

Converted near/far and radians can feed the existing view producer, whose later
near clamp to five remains distinct. The composed viewport feeds the separate
pass-relative viewport request. Pass rectangle, renderer dimensions, aspect and
renderer scalar remain explicit inputs. This connection does not extend exact
conversion arithmetic claims to the view helper's portable tangent/projection
math, nor prove a camera has been admitted for rendering.

The private probe checks the real authored camera's converted fields and then
exercises the view/viewport connection with clearly declared test pass inputs.
Those inputs are not original startup measurements or replacement defaults.

All 41 local CTest executables and the targeted conversion/view/projection
ASan/UBSan executable pass. The GMS and conversion tests also pass with GCC.
The preceding camera-reader CI run exposed a missing direct standard-library
include in its test; this revision fixes that portability error rather than
depending on Clang's transitive includes.
