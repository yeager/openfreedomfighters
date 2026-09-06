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

## Ordered admitted views

`graphics::AdmittedViewPass` implements the reviewed conditional view phase over
explicitly admitted records. It is not the renderer's separate camera-handle
registry and does not create startup admission. The constructor copies insertion
records and caches their ordering. Each key is the signed interpretation of
two's-complement negated priority, computed with unsigned arithmetic to avoid C++
overflow. Entries precede strictly smaller keys and follow equal keys. This gives
stable ascending camera priority except the signed-minimum wrap case; no float
priority conversion or per-frame priority re-sort occurs here.

The native policy rejects more than sixteen records. Duplicate view identities
remain distinct entries at this boundary. Per-frame camera associations and enabled
query results are explicit inputs indexed by original insertion position; absent
association is separate from identity zero. After the required frame-preparation
callback, every non-null enabled view receives begin, traversal and end callbacks
in that order. There is no single highest-priority winner or break after one view.

The original consumer walks a live array. This native subset instead requires
fixed admitted records and stable, immutable camera inputs/lifetimes throughout
the pass, with reentry rejected. It does not claim the original snapshots state
or safely permits callback destruction. Required callbacks and input sizes are
checked before the first effect. A callback exception propagates after its prefix;
no synthetic end hook or rollback occurs, and a retry starts the pass again.

The caller provides actual frame preparation, camera transform preparation,
component traversal and backend state. The helper does not implement those by
substituting empty backend operations. The private real-camera probe explicitly
assumes admission and enabled state using local identity tokens, then verifies
ordered stages and view calculations with declared test pass inputs. That is not
proof that startup invokes this pass or that its rectangle is fullscreen.

All 42 local CTest executables pass. The admitted-view tests also pass under GCC
and targeted ASan/UBSan, and the conditional owned-camera probe passes through
the ordered view callbacks without inventing startup admission.

## Renderer-to-state frame connection

`graphics::RendererFramePass` connects the reviewed outer renderer/backend phase
to caller-supplied state frame operations. Renderer admission and backend readiness
are explicit results, not forced-success defaults. Non-admission does nothing;
an admitted renderer whose backend is not ready skips state frames but still
reaches the outer backend-maintenance operation.

For a ready backend, the pass selects states by renderer identity and snapshots
their state identities in storage order before callbacks. It runs every selected
state frame, then every selected state's maintenance in the same order, then
backend maintenance. Duplicates are preserved. This outer snapshot is distinct
from the original inner live-view walk. Registration-list changes during callbacks
do not change the selected set, but the caller must retain the referenced states;
identity tokens do not confer ownership or represent authored source references.

Required hooks are validated before effects on admitted paths. Reentry is rejected
as a native policy. Exceptions propagate after their observed prefix without
forcing later state/backend maintenance; retry starts a new selection and frame.
Tests and the private actual-camera probe nest `AdmittedViewPass` under this state
callback boundary. They explicitly assume admission and stable local identities,
not an observed original application frame.

Further disassembly closes the two projection modes in concrete view preparation:
the boolean distinguishes finite-depth and infinite-far matrices, independently
of ordinary versus alternate camera shape. The traced geometry path consumes the
finite matrix. The existing finite projection helper is therefore the appropriate
conditional consumer; an unproved infinite-far use is not substituted into it.
Application-frame admission and the camera's runtime capability/enable state still
need their own integration evidence.
All 43 local CTest executables pass after this connection. The nested renderer/view
tests also pass under GCC and targeted ASan/UBSan, and the private owned-camera
probe verifies the conditional outer-to-inner phase order.

## Explicit runtime enable transitions

`CameraEnabledState` accepts an explicit runtime flag word, separate from the
authored camera reader. Its query reads bit `0x20`. Enable and disable are
idempotent; a changed state notifies a present renderer before modifying that
bit, preserving every other flag. With no renderer, the bit changes directly.
A required missing hook is rejected before effects. Native callbacks must not
reenter or destroy the owner; a throwing callback leaves the flag unchanged,
without rolling back effects already performed by the renderer hook.

This is not a camera-selection or registration operation. Although recovered
construction sets the enabled bit, copying and subsequent lifecycle operations
can change runtime flags. No complete constructor flag word or actual first-frame
enabled state is inferred here. Actual scene-property population and the resulting
first-cut query value, plus application admission, remain separate proof obligations.

All 45 local CTest executables pass after this addition. The enable-transition
tests also pass with GCC and ASan/UBSan, including disabled-view skipping and
reenabling under explicitly supplied admission and identity inputs.

## First registration and scene-property lookup

The camera setup's named-camera query reads a scene property, not the authored
resource directory or the registered-camera list. The caller initializes its
output handle to zero; a missing property leaves that value unchanged. An
existing valid handle property containing zero takes the same branch. A future
native property reader must validate the expected type and four-byte payload
before copying; the original diagnostic-and-copy behavior is not a safe parser
contract. This query is not implemented by searching display names.

The reviewed first-cut setup flag enters the registered-camera sweep when that
query produces zero, then registers the requested camera. The actual intro
camera's authored renderer selector is zero, for which the source reader skips
registration. Therefore a freshly constructed, unmodified and not previously
registered camera is outside the sweep and can retain its enabled state through
its subsequent registration. No synthetic enable operation is needed for that
bounded path.

Load completion can create a separate built-in fallback camera when registered
index zero is absent. It does not pick the first authored camera or automatically
populate the named-camera property. The two identities and collections must not
be conflated. The selected camera's generic member-start hook is a no-op, not an
additional enable operation.

Private inspection found no literal named-camera property key in the owned intro
GMS/BUF payloads, and a fresh property store is empty. Neither observation proves
the runtime query result: generic insertion, copying/restoration and intervening
lifecycle activity still require tracing. Thus these findings narrow the actual
startup path but do not yet establish an unconditional first-frame enabled state.
