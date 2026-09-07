# Ordinary scene and renderer traversal

This document records the recovered ordering boundaries represented by the
native scene/renderer models. It does not describe an active retail intro or a
complete graphics backend.

## Ordinary scene update

Every modeled ordinary update calls these services in order:

1. Scheduled events.
2. Ordinary components.
3. Position and bounds propagation.
4. Queued maintenance.

When rendering is disabled, the update stops there. Rendering enabled means
that the live renderer chain is traversed and then the scene post-render service
runs. A renderer callback executes before its node's next link is read, so a
callback can affect the link followed by the same traversal. The bounded native
model requires that chain to remain finite and alive for the call.

The update does not itself admit a camera, renderer state, device, draw, or
presentation. `OrdinarySceneUpdate` is deliberately a caller-wired boundary;
it does not make normal startup render merely because its callbacks return.

## Renderer-frame gates

The engine owns one unsigned 32-bit renderer frame word, initially 1. It is
shared by the engine rather than reconstructed per scene, renderer, view, or
application update. A view's last-clear word is separate. Drawing sees the
pre-increment word.

The eligible frame operation has this order:

1. If the engine is not running or the renderer is not initialized, return
   without resetting local counters, running hooks, or advancing the shared word.
2. Reset the two renderer-local counters and clear the in-scene marker.
3. Apply device suppression, backend readiness, and scene-begin admission.
4. If admitted, run backend traversal, admitted post-render work, and scene end,
   in that order.
5. Run renderer completion whether admission succeeded or failed, then advance
   the shared word with unsigned wrapping arithmetic.

Consequently, a failed device admission after the outer gates still runs
completion and advances the word, but performs no backend traversal or scene
end. Multiple qualifying renderer calls in one scene update advance the same
word independently. Callback failures retain their already completed prefix and
abort the operation; they are not interpreted as ordinary admission failures.

`RendererFrameClock`, `RendererFrame`, and `EligibleRendererFrame` provide this
separation. Recursive use of either a frame coordinator or its shared clock is
rejected. These guards are not synchronization: callers must serialize use and
keep the callback and clock storage alive for the operation.

## Admitted backend traversal

`BackendTraversal` receives a caller-provided snapshot of states already matched
to one renderer. It retains only those matching states, then performs the
following sequence:

1. Run each retained state's frame-begin service.
2. Visit its indexed views in their supplied order. A view without a camera, or
   with a disabled camera, is skipped. Each enabled view runs transform setup,
   begin-view, common-view, and end-view services.
3. Run maintenance for every retained state.
4. Run backend preparation, then preselect the retained state identities.
5. Run whole-state drawing rounds until no state requests another round, then
   restore selection.

View storage is consumed in place: it is not sorted, copied, or re-resolved by
the model. The bounded limits on views and drawing rounds are native safety
limits, not a statement about original ownership or capacity.

Successful callbacks do not prove source-payload decoding, state matching, view
allocation, camera admission, geometry submission, GPU work, swapchain
presentation, or an intro-to-menu path. Those services must be supplied and
verified independently. The existing picture clear and ordered-draw facilities
remain separate from this traversal until normal scene activation wires them
together.
