# Normal startup scene host

The scene host should own one active scene and route input, updates, audio, rendering, and teardown through that lifetime. Startup presentation alone does not establish scene activation.

CPU preparation and GPU work must remain on their appropriate owning threads. Scene replacement must retire the old scene safely, and partial startup failure must release acquired resources. The normal intro-to-menu path remains incomplete until it reaches a live, repeatable scene loop.

## SDL integration status (source review, 2026-10-27)

The normal application path retains a `NormalIntroSceneSession`, but does not
create or activate `NormalIntroSceneHost` or provide its frame bridge. The SDL
GPU runtime does construct the device, bind window dimensions and display
settings through `CameraDisplayServices`, attach `SdlIntroRenderer` to the
camera renderer list, run the post-construction reader bracket, and prepare a
cold first-cut player. These are preparation and diagnostic boundaries; they
do not admit a live frame.

The implemented SDL display-camera binding is not an active-view backend. The
next code-only integration seam is a concrete SDL active-view host that owns
state-zero/view lifetimes and supplies `RendererCameraViewAdmissionServices`
from live device, window, and render-pass objects. It must preserve the
`CameraRendererList` lifetime and provide real view ordering, use accounting,
backend-record registration, and frame-traversal identity. No-op callbacks do
not satisfy the admission contract. The source-free behavior requirements for
this seam are recorded in [the active-view host specification](SDL_ACTIVE_VIEW_HOST_SPEC.md).

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
