# Preview camera pointer update

`ApplicationServices` owns the shared preview-camera pointer history, toggle
latch and movement settings across scene loads. Its explicit update operation
uses the canonical clock's raw CRT delta, not scaled or frozen scene time and
not a caller-supplied replacement delta.

The caller supplies actual application input-query results, the live camera
pose/flags and its transform-update queue service. The first pointer sample
baselines history once. Subsequent finite mouse movement rotates the existing
basis; each update publishes history even when the raw delta is zero. Different
preview cameras share this history. There is no mouse-button, enabled-camera,
cursor-capture or pitch-clamp test inside this pointer helper.

Rotation uses the reviewed permuted basis mapping and Z, X, Y order with
separate binary32 operations. Native sine/cosine are an explicit numerical
interoperability choice, not a claim of bitwise agreement with the original
math library for every angle. The transform setter's equality fast path compares
position and the first six basis words bitwise. Otherwise it copies the full
pose, sets the resource dirty bit and queues that same resource.

Missing queue service, nonfinite input, active keyboard variants and collision
visualization are explicit unsupported results, not substituted zero inputs.
Arithmetic or queue failures retain completed state changes; callers must abort
the failed frame. Reentry through the shared updater is rejected. This is not a
thread-safe API, and callbacks must preserve resource/application lifetimes.

Tests exercise baselining, real pointer displacement, accumulated rotation,
cross-camera history, zero-delta publication, clock binding, unsupported inputs,
dirty-state visibility, queue failures and reentry.

This does not yet construct or admit the loader's DefaultCam/PreviewCamera,
register its console variables, process keyboard motion or consume the transform
queue. Normal startup does not invoke this explicit boundary automatically.
The audio listener must eventually use the actual registered camera resource,
not a separate pose copied solely for sound.
