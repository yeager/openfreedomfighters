# Conditional picture projection

The recovered descriptor emitter selects the ordinary pass/view projection
and requests identity WORLD and VIEW transforms. Expanded picture vertices
therefore still require perspective projection, but not another multiplication
by the picture basis. They are not pre-transformed screen coordinates.

## Explicit projection boundary

The CPU operation accepts already-resolved near `n`, far `f`, and signed
half-extents `h0` and `h1`. Its sixteen floats use D3D row order:

```text
n/h0  0      0           0
0     n/h1   0           0
0     0      f/(f-n)     1
0     0     -f*n/(f-n)   0
```

For an expanded vertex `(x,y,z,1)`, clip W is z. The horizontal and vertical
coordinates are scaled by n/h0 and n/h1, and clip Z uses the ordinary depth
coefficients above. The neighboring alternate-depth matrix is not selected
by this recovered emitter and is not substituted here.

These row-vector, perspective and viewport conventions are consistent with
[Microsoft's transformation pipeline](https://learn.microsoft.com/en-us/windows/win32/dxtecharts/the-direct3d-transformation-pipeline).
The source explains the API mathematics, not the game's camera selection.

## Viewport mapping

The caller supplies an already-established integer viewport origin X/Y and
positive Width/Height. With depth range 0..1:

```text
screen_x = X + (1 + clip_x/clip_w) * Width/2
screen_y = Y + (1 - clip_y/clip_w) * Height/2
screen_z = clip_z/clip_w
```

No half-pixel offset is introduced. Viewport Y flips normalized Y as described
in [Microsoft's viewport reference](https://learn.microsoft.com/en-us/windows/win32/direct3d9/viewports-and-clipping).
This CPU calculation does not clip or classify visibility: points behind the
camera or outside the frustum can have algebraic output, not drawable pixels.

## Portable safety policy

`convert_picture_viewport_request` implements the separate raw setter boundary:
each supplied binary32 X/Y/Width/Height in `[-2^63,2^63)` truncates toward zero
to signed64 and retains only the low unsigned32 bits. It does not round to
nearest, saturate or validate a viewport. For example, -1.75 becomes
`0xffffffff`, and exactly 2^32 becomes zero. Both signed zeros produce zero.
Non-finite/out-of-domain inputs are rejected as project policy; original
math-error handlers and floating-point status/trap side effects are not modeled.
The resulting raw viewport must still pass the separate mapping checks below.

Inputs and results must be finite, near must already be at least 5, far must
exceed near, and both signed half-extents must be nonzero. These checks reject
unsupported inputs rather than silently selecting a camera. The separate
producer's lower-bound operation is not repeated in this resolved-input API.
Negative half-extents remain signed. The matrix uses exact zero off-diagonal
entries under this finite contract.

Arithmetic uses double intermediates and checked binary32 output boundaries;
it does not promise bit-exact original extended-precision behavior. Zero clip
W is rejected; negative W is not treated as proof of visibility. Integer
viewport extents must be nonzero and right/bottom edges must fit uint32.
These are project safety constraints, not recovered error diagnostics.

## Remaining integration requirements

The resolved-input operation does not select the startup camera or choose
final draw-time pass state. The separate producer below derives half-extents
and raw viewport requests from explicit camera/rectangle inputs. No guessed near,
far, field of view or backbuffer rectangle is supplied to the native renderer.
GPU integration, clipping, rasterizer sample locations and final compositing
still require matching evidence.

Unit tests cover every matrix slot, signed half-extents, near/far depth,
perspective size changes, viewport origins and Y flip, negative W, integration
with expanded picture vertices, invalid parameters, and arithmetic overflow.
These are conditional mathematical tests, not original camera or pixel-fidelity
evidence.
Raw-conversion tests separately cover all four fields, fractional signs,
subnormal values, modulo wrap and both signed64 domain boundaries.

## Explicit camera/view producer

The separate view-parameter producer accepts a pass rectangle, raw near,
selected far, renderer dimension-query values D0/D1 and camera virtual aspect
A. These are supplied inputs, not dimensions inferred from texture images or
the host display. The near value is lower-bounded at 5 in this producer only.

For projection arithmetic, rectangle width W and height H are right-left and
bottom-top; a zero dimension is replaced with 1. Signed nonzero dimensions
remain signed. Define R=D0/D1, V=W/H and K=A*(R/V). The ordinary camera branch
uses explicit angle in radians and scalar E:

```text
h0 = tan(angle/2) * n
h1 = ((h0 * (1/R)) * K) * E
```

The alternate camera branch takes two separately named scalars p0/p1:

```text
h0 = 1/(2*p0)
h1 = ((1/(2*p1*(1/R))) * (1/R)) * K
f  = 2 * selected_far
```

The repeated ratio operations are not algebraically cancelled. The alternate
branch still constructs the ordinary perspective-shaped matrix with clip W=Z;
it is not labeled orthographic. E is explicit in the ordinary branch, although
the inspected D3D8 facade returns 1. No upstream camera-angle conversion or
final startup branch selection is inferred.

The portable producer uses ordered double arithmetic and checked binary32
resolved values before matrix construction. All consumed inputs must be finite;
zero divisors, nonfinite intermediates and unrepresentable outputs are errors.
The existing resolved projection checks far>near and nonzero half-extents still
apply. These numerical/valid-frustum policies do not emulate original exception
handling or claim bit-exact intermediate rounding. Tangent uses the host math
library; signed-zero clamp results are not a fidelity guarantee. Failure does
not return partial output.

A separate normalized viewport-request producer uses the original rectangle
dimensions, with no zero-dimension substitution. It lower-bounds a/b at zero
and upper-bounds c/d at one, then computes `(L+W*a,T+H*b,W*c,H*d)`.
It does not fully clamp all four components or treat c/d as endpoints. The
binary32 result is a raw request for the previously described conversion,
not proof of a valid viewport. Actual startup inputs and their event history
remain unverified.
