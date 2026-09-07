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

An empty group list or an empty group is rejected. The lower-level descriptor
expander and SDL renderer validate geometry, transforms and GPU state. The
public test uses independently authored descriptor data and verifies the
preserved image identities and geometry; it contains no retail content.
