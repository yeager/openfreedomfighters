# Startup source-picture draw admission

`StartupSourcePictureDrawAdmission` is a narrow source-backed CPU-to-backend
submission boundary. It does not render the normal startup menu by itself.

It accepts only a verified `StartupGraphicsAsset`, an explicit stable live
window hierarchy snapshot, current admitted owner/view associations, explicit
picture transforms and a caller-owned backend service set. The snapshot is
checked as depth-first preorder with the recovered construction rule: container
siblings appear first in reverse source order and leaf siblings follow in
source order. Its accepted picture leaves must exactly match the verified
source plan for the requested state.

After those checks, backend stages run in this order: device-scene admission,
matched-state admission, record preparation for every source picture,
preselection, texture residency, ordered-draw admission, then one submission
for every expanded source group. Any absent service or failed stage submits no
later work.

The boundary deliberately does not select a startup root, infer visibility,
create a camera or viewport, infer material/sampler/blend state, bind input, or
claim presentation. A portable 640x480 viewport remains a separate project
policy and is not evidence of the original startup viewport.

`StartupPicturePassAdmission` is a stricter disconnected caller for that
boundary. It requires one matching live scene lease, an externally selected
window-traversal root, an enabled admitted camera/view, the renderer state's
retained positive pass rectangle, a source runtime snapshot, and an explicitly
initialized finite Y-basis policy value. All picture owner/view associations
must refer to that admitted view, and the snapshot's root and pass-context IDs
must match the supplied live services. Only then does it delegate to
`StartupSourcePictureDrawAdmission`.

This does not choose the first startup window or camera, derive the rectangle
from the host output, create a renderer state, or make a visible-menu claim.

`StartupActivePassSnapshotProvider` is the stricter coordinator handoff. One
coordinator read supplies the selected root, admitted enabled camera/view,
renderer-state rectangle, projection and Y-basis together. The provider checks
that the root remains in the complete live hierarchy and binds the result to
scene, factory, hierarchy and coordinator epochs. The renderer boundary can
consume this move-only snapshot instead of mixing fields from separate reads.

The next private disassembly pass followed the ordinary state visitor and
window-pass consumers. They consume an already selected root/state, but do not
recover the FF-StartUp coordinator assignment, initial visibility or pass
mutation, camera registration/view creation, pass rectangle producer, or
presenter completion. The common virtual traversal wrapper occurs in multiple
derived tables, so a table match cannot identify a live FF-StartUp receiver.
The normal startup path remains deliberately disconnected until those producer
boundaries are recovered.
It is disconnected from normal startup until the original coordinator producer
is recovered.
