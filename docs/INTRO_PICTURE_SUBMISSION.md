# Intro picture submission boundary

`IntroPictureSubmission` is the explicit bridge from a retained authored
picture draw plan to `SdlIntroRenderer` draw requests. It takes externally
admitted groups, the completed picture-cache transform, projection, viewport,
scissor, stage state, texture factor and full SDL draw state. It expands each
real descriptor group independently and preserves its texture catalog image
identity and input order.

It does not select a picture, resolve a camera, decide visibility, admit a
scene, derive a transform, choose GPU state, submit commands, or present. The
caller retains those responsibilities. In particular, this boundary is not
wired into normal startup: the original first-cut lifecycle, camera/view and
renderer-admission evidence are still incomplete.

Ordered picture entries retain the opaque owner-context identity of an accepted
queued record. It is deliberately distinct from the record, view and generic
renderer-resource identities, and is not an `IntroRuntime` owner/resource
binding. A future first-cut frame bridge must obtain that typed binding from the
actual admission producer rather than infer it from an ordered entry.

`IntroAcceptedPictureRecordRegistry` is the frame-local post-accept bridge for
that future producer. It retains full generic record provenance alongside
separate typed owner/resource handles and expires all bindings at exact frame
end. It does not claim that a generic owner-context or renderer-resource value
equals either typed handle, and normal startup does not open this registry.

An empty group list or an empty group is rejected. The lower-level descriptor
expander and SDL renderer validate geometry, transforms and GPU state. The
public test uses independently authored descriptor data and verifies the
preserved image identities and geometry; it contains no retail content.

`FirstCutPictureFrame` is the next gate above this boundary. It accepts no
picture unless the existing view-admission result is `view_admitted`, the
first member became active at positive time, its activation prefix completed,
and ordered traversal accepted the record. It retains assembled submissions
for the caller's same-command-buffer GPU preparation. Normal startup does not
yet produce these prerequisites, so this remains disconnected from its frame
loop.
