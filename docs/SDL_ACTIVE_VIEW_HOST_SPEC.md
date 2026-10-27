# Active-view host behavior specification

## Scope

This specification defines the observable behavior of a host that accepts a
camera for use by the current scene and makes an accepted view available to
frame traversal. It does not define scene startup, asset loading, camera
decoding, rendering algorithms, or pixel-level output.

Binding a camera to a display or preparing a renderer does not by itself mean
that a view has been accepted or is available to traversal.

## Terms

- **Scene:** the currently active owner of views and deferred camera requests.
- **View:** a camera-associated unit that the active scene makes available to
  frame traversal.
- **Deferred request:** a camera identity and ordering priority retained for a
  scene that cannot yet accept a view.
- **Traversal identity:** an identity for one active frame traversal, used to
  reject stale work from another traversal.

These terms describe externally observable roles; they do not require a
particular object layout or storage strategy.

## Inputs

An admission attempt receives a camera identity and an ordering priority. It
observes whether the rendering service is available, whether the active scene
can accept views, and the current application dimensions. It must have a way
to associate the camera with an accepted view, register that view for the
renderer, place it in the requested ordering, and update the view's use
accounting.

When the active scene cannot yet accept views, it may retain a bounded
deferred request containing the camera identity and priority. A deferred
request belongs only to the scene that accepted it. Later processing must be
authorized by a one-use grant issued by that same scene's deferred-request
service; a generic success acknowledgment is insufficient.

Frame traversal receives the admitted view ordering and the identity of the
traversal in progress. Camera owners and scene data supplied to admission must
remain valid for as long as the host may inspect or traverse them. Data needed
to use an accepted view, including its camera association and renderer-facing
references, remains valid for as long as that view may be traversed or used.

## Observable outcomes and transitions

An admission attempt has one of these outcomes:

| Outcome | Observable result |
| --- | --- |
| Rendering service unavailable | No scene, deferred-request, or view state changes. |
| Rendering service not ready | No scene, deferred-request, or view state changes. |
| Deferred request accepted | Camera identity and priority are retained for that scene; no view is available to traversal. |
| View accepted | The scene owns an associated view; the renderer can use it; it appears in the requested ordering; use accounting is updated once. |
| Failure | The host reports failure and does not report a new view as available. Any mutations already completed remain owned for cleanup and are not implicitly retried. |

A deferred request becomes an accepted view only after the scene is ready and
the request passes through the same acceptance behavior as a direct request.
Until then, traversal must not observe it as a view. Processing multiple
deferred requests must report which requests were accepted; a failure must not
imply success for requests not yet processed.

Accepted-view ordering and use accounting must stay consistent with the
renderer's observable behavior. Retiring a scene must prevent new traversals
from using its views and allow any traversal already using them to finish or
stop before the associated data is released.

## State, lifetime, and threading

The host owns the active scene's accepted views and any renderer resources
created for them. The scene and camera owners must remain valid throughout
admission and every traversal that can use their data.

Operations that create, submit, or release GPU state run on the SDL device's
owning thread. CPU-only preparation may run on its appropriate owner thread.
Cross-thread work must cross an explicit synchronization boundary; work must
not be exposed as ready until the owning thread has completed the relevant
operation.

Only one scene is active at a time. On replacement or teardown, views,
deferred requests, and renderer resources belonging to the retiring scene
become unusable before their backing owners are released. Any traversal
already in progress must finish or stop using the retiring scene before its
view data is released. A failed or partial startup leaves no view eligible for
traversal. Teardown is valid after any prefix of setup, releases each acquired
resource at most once, and prevents later use through the retired host.

Admission and deferred-request processing are non-reentrant. An attempt to
enter either boundary while it is already running fails without interleaving
state changes or duplicating accounting. A one-use grant is bound to the
scene and request that issued it, expires when consumed or when that scene is
retired, and cannot authorize another request or a later processing attempt.

## Edge cases and failures

- If the rendering service is absent or not ready, return its corresponding
  outcome before changing scene, deferred-request, ordering, or accounting
  state.
- If the service is available but a required input or operation is missing,
  report failure. Do not invent dimensions, scene state, a view, or a
  successful no-op result.
- Invalid dimensions or a scene that is not ready cannot yield an accepted
  view. A bounded deferred request may be retained only if the scene accepts
  it. Rejection or capacity exhaustion reports failure and creates no view.
- A grant for another scene, another request, an expired request, or a grant
  already consumed cannot authorize processing.
- If scene initialization fails while deferred requests exist, none of those
  requests are reported as accepted views. If processing one request fails,
  later requests are not reported as accepted by that processing attempt.
- If acceptance fails after a mutation has occurred, report failure and make
  the view unavailable to traversal. Keep completed mutations under the
  retiring scene's cleanup responsibility; do not silently replay the
  attempt.
- A stale camera owner or traversal identity cannot be used for new admission
  or traversal. The view collection must not be changed while it is being
  traversed.
- Teardown after full or partial setup releases owned resources at most once
  and makes subsequent traversal through the retired host fail.

## Synthetic verification cases

Use synthetic cameras, identities, dimensions, scene objects, and mock
rendering services only.

1. **Service unavailable:** try absent and not-ready service states. Confirm
   distinct outcomes and no change to scene state, deferred requests, view
   ordering, registration, or use accounting.
2. **Direct acceptance:** provide a ready scene. Confirm one associated view
   becomes traversable, is placed in the requested ordering, is registered
   for renderer use, and updates accounting once for the active traversal.
3. **Defer and process:** provide a not-ready scene that accepts a request.
   Confirm no view is traversable. Make the scene ready, use the scene-issued
   grant once, and confirm ordinary acceptance occurs exactly once.
4. **Deferred request rejected or full:** confirm failure and no accepted
   view, renderer registration, ordering entry, or accounting update.
5. **Missing required input or operation:** omit each required item in turn
   after the service is available. Confirm failure and no invented success.
6. **Failure during acceptance:** fail each mutation step in turn. Confirm
   failure, no traversable view, and cleanup of any acquired resources during
   teardown.
7. **Grant misuse:** try a grant from another scene, a consumed grant, an
   expired grant, and a grant for a different request. Confirm rejection.
8. **Reentry:** trigger nested admission or deferred processing from a mock
   callback. Confirm rejection without duplicate ordering or accounting.
9. **Retirement and thread ownership:** replace a synthetic scene and confirm
   its views cannot be traversed afterward. Confirm GPU create, submit, and
   release operations occur on the device-owning thread, including partial
   setup cleanup.
10. **Traversal identity:** run successive traversals and confirm a retired
    scene or stale traversal identity cannot expose an old view.

## Clean-room constraint

This specification states behavior and observable constraints only. It
contains no retail values, payloads, fingerprints, copied text, or
implementation-derived observations. Verification data must be synthetic.
