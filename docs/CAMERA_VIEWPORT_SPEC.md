# Camera tuple to pixel viewport

This contract supplements `CAMERA_RENDERER_LIST_SPEC.md`. The camera tuple
represents normalized viewport x, y, width, and height. It is relative to a
parent pixel rectangle, with an upper-left origin, x increasing rightward and
y increasing downward. It does not specify the GPU projection matrix.

## Mapping

Work on a copy of the stored tuple. Clamp negative x and y to zero and width
and height above one to one. These bounds are asymmetric: do not clamp x/y
above one or width/height below zero. Do not clip x+width or y+height to one.
Leave the stored tuple and its height/width ratio unchanged.

For unsigned 32-bit parent edges L, T, R, B in the supported ordered range,
form W=R-L and H=B-T. Convert the unsigned values to binary32. Compute each
product and sum separately in binary32, without fused multiply-add:

```
pixelX = x * float(W) + float(L)
pixelY = y * float(H) + float(T)
pixelWidth = width * float(W)
pixelHeight = height * float(H)
```

Truncate each completed result toward zero independently. Add the parent
origin before truncation. Width and height are extents, not right/bottom
coordinates. Submit the resulting viewport with minimum depth zero and
maximum depth one. The stored camera ratio remains based on the original
tuple, not on rounded pixel dimensions.

## Host policy and integration boundary

Require finite tuple values, ordered parent edges with positive dimensions,
representable output integers, and viewport dimensions accepted by the actual
backend. Report unsupported inputs explicitly. These are host validation
rules; the asymmetric comparisons alone do not validate all inputs.

Do not invent parent/target clipping, clearing outside the viewport, or
letterbox-bar rendering. Those behaviors require their own contracts. This
mapping does not establish field-of-view units, near/far distances, projection
matrices, camera activation, or successful frame admission. The production
backend must consume the selected camera's viewport during actual rendering.

## Synthetic validation

Cover the full parent rectangle, nonzero parent origin, fractional products
near integer boundaries, and origin addition before truncation. Verify
asymmetric clamping, including negative origins, extents above one, origins
above one, negative extents, and origin-plus-extent above one. Pure mapping
tests may expose values that a backend rejects; do not submit invalid viewport
dimensions merely to test arithmetic. Verify repeated application leaves the
stored camera tuple and ratio unchanged. Check separate binary32 operations
and unsigned parent-coordinate conversion.
