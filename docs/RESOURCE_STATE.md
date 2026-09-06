# Live resource flags

`IntroRuntime` retains one optional complete resource flag word and a separate
typed resource-context handle per owner. Unknown state stays unknown. Authored
flags, camera-owner flags and owner aggregate flags are different values.
Picture flag queries and PreviewCamera resource views use this same storage,
including after a dynamic camera grows the resource collection.

`assign_resource_state` publishes state supplied by a real producer. It is not
the game's flag setter and performs no attachment or registration work.

## Mutations

`mutate_resource_low_byte` replaces the target's low byte with
`((flags & ~clear_mask) | set_mask) & 0xff`, preserving higher bits. Requested
low set bits propagate to every resource ancestor. Clears do not propagate;
clearing the root does not change its children. Context, pose and owner flags
are untouched.

`set_resource_flags_no_maintenance` implements the construction-time setter
branch that needs no registration or renderer maintenance:

- Resource bit `0x2000` must not change.
- When either mask touches `0x8000`, actual allocation mode must be off or
  actual renderer-resource maintenance must be suppressed.

It applies the low-byte operation first, then the remaining masks to the current
target word. Set wins over clear. Full replacement therefore still propagates
requested low bits to ancestors; it is not a plain assignment.

Unsupported maintenance and unknown required words fail before mutation.
Unknown ancestors are irrelevant to a clear-only request. This preflight is a
native safety policy, not a claim about original failure handling.

## Loader boundary

Normal startup calls `construct_root` before admitting authored resources. It
executes single allocation, the ZROOM owner factory, RootGroup enrollment and
its immediate initializer. At this boundary the root's actual flags are
`0x09000000`, with identity pose, no parent/context and an empty live child list.
Prepared authored links are detached; later attachment must build the real list.
An existing completed root is reused without resetting its state.

RootGroup retains its `DisplayName` floating-point descriptor and `RootControl`
input-map registration. Its ordinary membership remains pending, not merged.
The root's separate enabled marker does not enable a camera or set a resource
initialized bit. Global phase one repeats the same initializer and map reference
operation. Its ordinary input processor is still missing and fails by name.

These operations require an established live parent chain. The prepared GMS
hierarchy alone does not prove that original attachment services have run.
The real source path must preserve its first setter, attachment, then second
setter using the post-attachment word. Remaining source readers and loader
services are not replaced by constructor constants or empty callbacks.

Tests use independently constructed states to check ancestor propagation,
maintenance gates, picture views and subsequent DefaultCam hide inheritance.
They do not establish the retail root's post-load flags. The explicit `root_ready`
stage blocks DefaultCam fallback until actual authored loading is implemented;
the fresh construction result is not accepted as the later loader result.
