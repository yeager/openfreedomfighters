# MatPosAnim recovery boundary

`MatPosAnim` is a retained intro component family. Its constructor state and
owner-local data are prepared during scene construction, but that is not an
animation player and does not activate the intro.

## Deferred readers are not its input

The supported intro contains 227 deferred records associated with MatPosAnim
owners. Each reaches the deferred-record terminator before attachment dispatch.
Consequently, the normal deferred-reader bracket supplies no MatPosAnim
component cursor. A deferred owner tail, attachment name, or neighbouring record
must not be used as a substitute input.

The corresponding component reader and writer are a separate persistence and
restore virtual route. Their recovered boundary is a fixed object stream; it is
not the deferred reader/writer grammar and it is not phase one. Phase one takes
no object-stream argument: it initializes runtime behavior from existing
component state and its runtime providers. The persistence/restore producer and
the runtime provider behavior have not yet been recovered, so the native
runtime deliberately does not invoke either route or claim its fields are
initialized.

## Prepared `KEYS` state

The currently supported owner-local profile recognizes a bounded BUF container
with exactly one `KEYS` child of the reviewed shape. Scene preparation validates
the complete profile, copies the canonical data, and publishes an immutable
owner-local registry only after every entry is valid. Lookups are scoped to the
live owner identity; they do not fall back to global names, adjacent BUF data, or
invented handles.

This is preparation for future recovered persistence/restore and runtime
provider routes. It does not decode a general BUF language, execute a
MatPosAnim reader, register animation events, advance animation time, or modify
transforms.

## Detached pose evaluation

The bounded numeric KEYS sample can be converted into a detached local pose.
Its first group is linearly interpolated before this boundary, scaled from
signed quantized components by `1 / 32512`, and normalized as `x, y, z, w`.
The evaluator also produces the recovered logical row-major quaternion basis;
it is an inert value and is not composed with an owner-local transform or sent
to a renderer. The three-float translation group is returned unchanged. This
does not attach the pose to an owner, a resource, or a renderer. It cannot
activate scene animation or make the intro visible.

## Recovered owner application seam

After sampling, the native path calls a virtual operation on the same live
owner/provider that supplies the exact `KEYS` child. It passes the transient
local translation and basis values and ignores the operation's return value.
This is not a generic scene-transform write, resource update, or renderer
submission.

`MatPosOwnerTransformApplyBinding` models only that narrow seam. It requires a
currently enrolled typed provider, performs a fresh exact `KEYS` lookup on
every application, and rejects non-finite basis or translation values. Its
receiver has no result channel and the binding retains none of the sampled
values. Provider teardown and explicit invalidation both disable later calls.

The shared provider bridge writes a typed owner-local transform state only when
the complete basis or translation changes. A basis comparison preserves float
bit identity; translation uses numeric equality. On change, the whole local
state is committed, marked dirty, then synchronously notifies a typed
invalidation service. This state has no resource, hierarchy, cache, or
renderer identity, and rejects non-finite samples before any mutation.

Parent-world composition and any relation from the invalidation service to
picture, camera, view, or render-pass state are still unrecovered.
Consequently no current runtime binds this seam to an intro owner or uses it to
claim animation or presentation.

## Lifecycle status

MatPosAnim phase one remains unavailable until its existing-state/provider
initialization behavior, `KEYS` object semantics, event declaration behavior,
and the complete scene lifecycle can be recovered and run together. The fixed
object stream belongs to the separate persistence/restore route. The global
lifecycle must cover the whole eligible scene; a MatPosAnim-only pass would not
establish original ordering or readiness.

No current path renders, updates, or plays the original intro. The prepared
registry and its tests are data-integrity boundaries, not evidence of visible
cutscene playback.
