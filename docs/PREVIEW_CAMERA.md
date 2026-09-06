# Preview camera controls

`ApplicationServices` owns the shared preview-camera pointer history, toggle
latch and movement settings across scene loads. Its explicit update operation
uses the canonical raw CRT delta for the pointer and `last_scaled_increment`
for keyboard movement. Neither comes from caller-supplied replacement values.
Scene freeze zeros neither input; one-shot clock suppression retains the prior
keyboard increment. This binding currently requires the supported CRT clock mode.

The caller supplies actual application input-query results, the live camera
resource pose/flags and its transform-update queue service. The first pointer sample
baselines history once. Subsequent finite mouse movement rotates the existing
basis; each update publishes history even when the raw delta is zero. Different
preview cameras share this history. There is no mouse-button, enabled-camera,
cursor-capture or pitch-clamp test inside this pointer helper.

Y toggles camera-owner flag `0x10000` once per shared held-key latch. It does not
stop movement or notify the renderer. Q/W adjust translation speed; E/R adjust
rotation speed. Only event state exactly one changes a setting. Scales are
captured before those changes, so they affect the following update.

Unmodified arrows move forward/back and yaw. Control plus arrows translates
vertically/sideways; Shift plus arrows pitches/rolls. Alt alone suppresses the
unmodified branch, but not the Control or Shift branches. Control+Shift suppresses
all arrow branches. Opposite keys are processed individually in their original
order. Input arrays must be a real stable frame snapshot; this is not a platform
key-repeat or edge-state producer.

Rotation uses the reviewed permuted basis mapping and Z, X, Y order with
separate binary32 operations. Native sine/cosine are an explicit numerical
interoperability choice, not a claim of bitwise agreement with the original
math library for every angle. The transform setter's equality fast path compares
position and the first six basis words bitwise. Otherwise it copies the full
pose, sets the resource dirty bit and queues that same resource.
Resource hide/dirty flags are distinct from camera-owner enabled/toggle flags;
the updater never puts resource dirty state into the camera-owner flag word.

Translation uses a portable 64-bit-significand intermediate policy before
binary32 storage. Fixed-width integer arithmetic implements ordered rounded
sums, including cancellation, subnormals and signed zeros; it does not depend
on a platform's `long double` format. The original live x87 precision-control
setting remains unproved, so this policy is not a universal bit-exactness claim.

Missing queue service, nonfinite input, non-nearest rounding and enabled collision
visualization are explicit unsupported results, not substituted zero inputs.
Arithmetic or queue failures retain completed state changes; callers must abort
the failed frame. Reentry through the shared updater is rejected. This is not a
thread-safe API, and callbacks must preserve resource/application lifetimes.

Tests exercise baselining, real pointer displacement, accumulated rotation,
cross-camera history, zero-delta publication, all 128 modifier/arrow combinations,
speed edges, shared Y latch, frozen/suppressed timing, dirty-state visibility,
queue failures and reentry. Translation tests include fixed rounding vectors
and, where available, comparison with independent native extended80 arithmetic.
Input fixtures are synthetic; no original keyboard capture is claimed.

`PreviewCameraComponent` owns the real four live-variable bindings. The static
check variable deliberately aliases the dynamic-check field; the separate static
field remains distinct. Typed handles reject stale/foreign access, duplicate names
remain separate registrations, and destruction releases bindings in registration
order. The component update reads its live collision setting and uses the same
application's clock and shared pointer history, not a caller's replacement state.

`PreviewCameraResourceView` binds directly to the retained hierarchy and resource
flags. The integrated DefaultCam factory and ordinary dispatcher use that view;
they do not update a detached preview/listener pose. The loader still needs its
preceding real root-state producer and remaining component implementations.
Platform input, collision visualization and transform-queue consumption remain
unconnected in normal startup. See [camera membership](CAMERA_REGISTRATION.md).
