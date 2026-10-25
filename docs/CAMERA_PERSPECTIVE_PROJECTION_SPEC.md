# Canonical camera perspective projection

This contract connects the ordinary camera to actual SDL draw projection.
It supplements the display and viewport contracts. A projection matrix alone
does not admit scene lifecycle or animation playback.

## Inputs and admission

Read the selected canonical camera's current angle in radians, near and far
distances, and stored viewport height/width ratio. Use the current full window
pixel dimensions and the parent render rectangle, separately. Do not derive a
camera from geometry bounds or from rounded viewport dimensions. The concrete
renderer pixel-aspect factor is one.

The ordinary branch requires camera runtime flag mask `0x1` to be clear.
Reject the alternate branch until its source fields and consumer are covered;
do not substitute a conventional orthographic matrix. This mask is unrelated
to aspect mode, render control, preview toggles, or fog flags.

Host admission requires nearest-even rounding, finite representable arithmetic,
positive dimensions and ratio, a field of view strictly between zero and pi,
and far greater than the clamped near distance. These are explicit validation
policies. Reject invalid inputs before recording GPU work. Set N=max(5,near)
and F=far. The separate earlier camera-source near clamp does not replace this
projection-stage clamp.

## Evaluation order

Use independently rounded binary32 operations without FMA or algebraic
cancellation for the following calculations. Parent dimensions come from
unsigned edge subtraction before conversion, as in the viewport contract.

```
A = float(displayWidth) / float(displayHeight)
B = float(parentWidth) / float(parentHeight)
Q = A / B
I = 1 / A
R = storedCameraRatio * Q
H = angleRadians * 0.5
T = float(tan(double(H)))
X = T * N
U = X * I
V = U * R
Y = V / 1
```

The original ratio product passes through an extended register before its
binary32 store. For normal finite results with nearest rounding, two binary32
operands and ordinary 24/53/64-bit significand precision, rounding their product
to binary32 gives the same result. Do not extend that identity claim to
underflow, flush modes, or exceptional values; the host uses the stated
binary32 policy. The concrete pixel-aspect reciprocal is exactly one. The
host tangent implementation remains a portability boundary: respect its
explicit input/output conversions, but do not claim bitwise identity across
CRT implementations. Do not silently describe this as full numerical parity.

Negate X and Y by changing their sign bits, obtaining NX and NY. Compute
DX=X-NX, DY=Y-NY, NN=N+N, and D=F-N as separate binary32 operations.
Calculate the finite-depth matrix using these coefficients:

```
horizontal scale = NN / DX
vertical scale = NN / DY
horizontal offset = (X + NX) / DX
vertical offset = (NY + Y) / DY
depth scale = F / D
negativeFar = F * -1
depth offset = (negativeFar * N) / D
```

Retain the computed offsets rather than replacing their arithmetic by literal
zeros. For the SDL column-major array and column-vector multiplication, store
the scales at indices 0 and 5, offsets at 8 and 9, depth scale at 10, literal
one at 11, and depth offset at 14. Other entries are positive zero. This is the
transpose representation of the row-vector transform, with clip W equal to
eye-space Z. Positive Z is forward; near/far map to depth zero/one. Viewport
origin does not alter this matrix.

An alternate depth matrix is known, but selecting it is outside this contract.
Do not use it as a fallback. Preserve existing explicitly separate utilities
without presenting them as an admitted runtime mode.

## Production integration

Resolve projection and viewport together from the selected camera in
`SdlIntroRenderer::prepare`, before GPU allocation/recording. Copy the resolved
matrix into the actual draw descriptors consumed by the vertex shader. Reuse
or improve existing picture projection helpers rather than leaving another
unconnected calculation path. Keep their numerical contracts accurate if
shared helper behavior changes.

Expanded picture vertices already contain object/view transforms, as specified
by `SdlIntroDraw`. Do not apply another camera basis, view matrix, axis swap,
or model transform. The existing identity model upload remains correct.

Refresh full display pixel dimensions on the SDL window thread when preparing
an admitted frame; pass the actual parent target rectangle independently.
This does not change the renderer node's construction-time aspect snapshot.
The explicitly diagnostic fallback bypasses both canonical viewport and
projection and retains its documented fitting matrix. An absent selection
does not fabricate a selected camera or a projection receipt.

## Synthetic validation

Check near/far depth, N clamping, horizontal/vertical frustum boundaries,
window versus parent aspect, ratio changes, and viewport-origin independence.
Use inputs where binary32 sequencing differs from a collapsed double formula.
Verify invalid values and alternate flags reject before recording commands.
Preserve existing picture/world-view transform tests and test the actual SDL
matrix upload through pixel readback: the selected camera must override a
deliberately different supplied draw matrix, while the diagnostic bypass
retains it. Keep visible verification of both independent draw batches and
textures. Synthetic GPU tests prove this rendering connection, not game parity.
