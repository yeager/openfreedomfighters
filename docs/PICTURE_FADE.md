# Conditional picture fade receiver

`off::cutscene::PictureFade` implements the independently reviewed CPU receiver
for the intro's picture fade component. It does not load components, initialize
owner colors, admit a scene, or draw pictures. Owner display names do not select
the fade algorithm. Construction emits no effects, including no initial alpha.

The caller supplies a symbolic event, raw argument, signed scene clock and
synchronous effect visitor. `owner_control` is a boolean resource-control request,
not a substitute for proven renderer visibility. Alpha outputs must eventually
reach the owner's packed color and material propagation path; this receiver does
not pretend that updating an unrelated vertex buffer completes that integration.

| Event | Immediate effects | Timed update | Completion |
| --- | --- | --- | --- |
| Zero-argument FadeIn | Owner control true, no alpha write | None | Idle clear |
| Timed FadeIn | No owner effects | Alpha from 254 toward 0 | Alpha 0, owner control true, idle clear |
| Zero-argument FadeOut | Owner control false, alpha 254 | None | Idle covered |
| Timed FadeOut | Owner control false, alpha 1 | Alpha from 0 toward 254 | Alpha 254, idle covered |

Timed arguments are interpreted as signed words, converted to binary32,
multiplied by binary32 0.001 and then -1024, and truncated to a signed integer.
Subtracting that result from the scene clock gives the deadline. These are nominal
milliseconds in the recovered engine clock, not measured wall-clock timing.
Updates use the integer 1024-step fraction before reducing it to alpha. Exact
deadline equality writes the endpoint but leaves the transition active; completion
requires a strictly later clock. Repeated events replace transition timing without
sampling current alpha. Unknown events ignore their arguments and do nothing.
Alpha 254 is intentional and is not interchangeable with 255 in the owner material
path.

## Explicit native limits

The original timing reads are separate and occur after the timed FadeOut owner
callbacks. A single supplied clock is a conditional snapshot policy: the clock
must remain unchanged across those effects and reads. This API is not a recovered
host-clock producer or complete update scheduler.

Negative arguments, unsupported float-to-integer conversions, nonpositive effective
durations, duration/deadline overflow, clock wrap and updates before the transition
start are rejected. Unsupported event inputs are validated before effects as a
native safety policy, not original exception semantics. Updates may revisit a
clock within the supported transition interval; no additional monotonicity rule
is invented. Idle updates emit nothing.

Visitors may not reenter or destroy the receiver. State changes follow the
corresponding synchronous effects. If a visitor throws, emitted prefix effects
are not rolled back, and subsequent state writes have not occurred. Retrying may
therefore repeat effects; callers must not assume transactional delivery.

Initial authored owner state, resource lifecycle and actual draw admission remain
separate integration work. CPU tests and conditional owned-data probes do not
establish rendered intro playback or Original visual parity.

## Verification

The independent public test suite covers both directions, zero-duration events,
exact boundaries, repeated and unknown events, a binary32 rounding witness,
negative clocks, invalid inputs, callback state observation, exception prefixes,
retry and reentrancy. All 38 CTest executables pass locally, as does a targeted
ASan/UBSan run of this receiver and its tests.

A private probe decodes the owned intro archive, resolves actual command event
names and target references, and conditionally routes the ordered command pass
to three explicitly admitted receiver instances. It checks ordered effects and
transition boundaries using declared test-clock samples. This verifies real
authored commands against the receiver without publishing retail vectors or
pretending that the probe performs runtime component construction.

Separate reviewed data observations establish that the intro picture payload has
an additional component delimiter absent from the restricted startup reader.
Reusing that reader unchanged is unsupported. Authored source flags and runtime
resource flags are distinct domains; their numeric bits must not be equated
without the loader conversion and lifecycle. Neither a decoded picture nor an
unhide effect alone proves that the original draw path accepts it.

## Intro picture source and texture joins

`GmsImage::intro_fade_picture_source` is separate from the startup-only reader.
Callers must establish supported intro provenance; the method also checks the
picture source class, zero class data, exactly one named fade attachment with
numeric zero parameter, and the exact bounded component-delimited block. Full
tags, both closing delimiters, terminal and declared length are required. The
extension field is mandatory; unsupported forms fail without invented defaults.

The reader checks state exponent and alignment, preserves the opaque base-render
selector and full picture resource key, and applies the reviewed unsigned alpha
and extension clamps. It does not infer color from an owner label or copy source
flags into a runtime visibility word. External padding remains outside the block.
The startup reader's accepted grammar is unchanged.

The private owned-data probe now follows the actual command targets through this
reader, the checked PRM picture decoder, manager-key texture bindings and TEX
decoder. It checks all returned source fields, descriptor properties, resource
identities, draw-group/quad counts and every decoded pixel of the bound images.
Equal texture pixels do not erase distinct authored resource identities. No retail
payload or expected pixel vectors are published. These real-data joins establish
the resources to use for later rendering, not the camera, resource lifetime,
material propagation, update admission or final compositing order.
