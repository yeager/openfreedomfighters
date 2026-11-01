# Normal startup scene host

The scene host should own one active scene and route input, updates, audio, rendering, and teardown through that lifetime. Startup presentation alone does not establish scene activation.

CPU preparation and GPU work must remain on their appropriate owning threads. Scene replacement must retire the old scene safely, and partial startup failure must release acquired resources. The normal intro-to-menu path remains incomplete until it reaches a live, repeatable scene loop.

## SDL integration status (source review, 2026-10-27)

The normal application path retains a `NormalIntroSceneSession`, but does not
create or activate `NormalIntroSceneHost` or provide its frame bridge. The SDL
session and the application and component-sequence services it borrows now
share a C++ lifetime owner; this alone does not activate the scene or admit a
camera. The SDL
GPU runtime does construct the device, bind window dimensions and display
settings through `CameraDisplayServices`, attach `SdlIntroRenderer` to the
camera renderer list, run the post-construction reader bracket, and prepare a
cold first-cut player. These are preparation and diagnostic boundaries; they
do not admit a live frame.

The SDL active-view host connects the coordinator to registered renderer views
and retains a caller-supplied scene lease. Synthetic GPU and lifetime tests
exercise deferred admission, traversal, retirement, and frame preparation. It
is not connected to normal startup or first-cut camera selection. The remaining
integration must supply `RendererCameraViewAdmissionServices` from live device,
window, and render-pass objects. It must preserve the
`CameraRendererList` lifetime and provide real view ordering, use accounting,
backend-record registration, and frame-traversal identity. No-op callbacks do
not satisfy the admission contract. The source-free behavior requirements for
this seam are recorded in [the active-view host specification](SDL_ACTIVE_VIEW_HOST_SPEC.md).

The SDL renderer now also provides a scene-keyed view registration primitive.
It retains the supplied camera, binds view and traversal tokens to the renderer
instance, and uses the registered camera when preparing a frame. A synthetic
GPU readback test covers this path on macOS. The caller remains responsible for
scene lifetime, retirement, and ending traversal; this primitive does not supply
admission, frame ordering, or the normal startup host.

That host alone is not sufficient to activate normal startup. The loader-tail
path still lacks implementations for its required allocation diagnostics,
resource-association effects, source lease, camera-zero/fallback transform,
outer-scene operations, saved-resource spatial admission, and saved-owner
service. The application also lacks normal providers for event-16 phase
completion, scene clock/preparation/group sends, positive-time member
activation, and ordered picture-record production. These missing services
must be supplied from project-owned behavior before the host can safely run.

This status is a source-only integration review. It contains no retail-derived
observations and makes no claim of intro playback or rendering parity.

## Source-side activation frontier (2026-11-01)

The application creates and retains `OwnedNormalIntroScene`, constructs its
source directory, and passes the resulting `NormalIntroSceneSession` to SDL.
The normal path does not create `MovieControlHostEvidence`, bind
`NormalIntroSceneLifecycleServiceAdapters`, call
`NormalIntroSceneHostFactory`, or construct a `NormalIntroSceneHost`. SDL gets
no `NormalIntroSceneHostFrameBridge`; it therefore keeps presenting the
labelled static fallback.

The existing first-cut player preparation validates its parsed sequence,
camera, legal picture, and source ownership. The existing legal-picture
activation operation still requires live global-lifecycle completion,
positive-time member activation, renderer flags, position state, viewport
dimensions, and an actual position-update service. Its result is only one
receipt: frame assembly also requires an admitted first-cut view, a completed
ordered-picture record, and source-backed picture submissions. Source review
does not provide those values or a normal ordered-picture producer.

`OwnedNormalIntroScene` now exposes a lifetime-bound lease for the prepared
first-cut camera. The normal runtime still does not register that camera with
the live SDL view host or construct an admitted first-cut frame.

The current gate is before host construction and activation. Next, provide
concrete project-owned outer-loader and global-lifecycle services, then connect
the first-cut update, selected camera/view, ordered picture records, audio, and
teardown through that same session. Keep the host and frame bridge fail-closed
until those providers return real operation results and receipts.

## Startup image while activation is pending (2026-11-01)

SDL now builds its labelled static fallback directly from the prepared legal
picture source using the exact-source preview policy. It does not run the
ordinary postconstruction reader bracket or pass a fabricated retained value
to its external-loader hook. The fallback remains a still image: it does not
complete the outer-loader tail, activate MovieControl, select/admit a camera
view, advance the first-cut clock, or present an animated frame. The retained
reader/session stays cold until those services have source-backed providers.
The explicit `--diagnostic-first-cut-picture-step` option follows the selected
command's authored source reference for a still preview; it also does not claim
reader admission or command execution.

## Reader-service source boundary (2026-11-01)

The normal application has no project-owned value provider for the reader
bracket's opaque retained value or its external-loader service. The current
session adapter records those calls but does not implement their production
semantics. Do not pass an invented constant or a no-op service to force reader
completion: the controller evidence and normal scene host must remain
unavailable until those operations have defined project-owned behavior.
