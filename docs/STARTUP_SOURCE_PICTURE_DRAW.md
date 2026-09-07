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
