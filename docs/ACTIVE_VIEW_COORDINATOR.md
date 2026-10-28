# Active-view coordinator primitive

`off::graphics::ActiveViewCoordinator` implements the ownership and state
transitions described by `SDL_ACTIVE_VIEW_HOST_SPEC.md`. It is a coordination
primitive, not a production SDL host: it does not create or own an SDL GPU
device or window, construct a renderer pass, populate a camera renderer list,
or derive scene/camera inputs from normal startup. A future integration must
provide those real inputs and renderer operations.

The coordinator captures its construction thread as the device-owning thread.
Call admission, traversal, scene changes, grant operations, retirement, and
GPU callbacks on that thread. Destroy the coordinator there after all traversal
tokens have ended. `retire` immediately prevents new traversals, clears the
scene's deferred queue, and delays release of registered and partially
registered resources until existing traversals end. Renderer resources created
before a later admission failure remain recorded for that scene's cleanup.

Views are traversed in ascending priority order, with equal priorities kept in
admission order. Use accounting is invoked once per accepted view at traversal
begin and receives that traversal's identity. If accounting fails or throws,
the attempted traversal is rejected and its scene is retired; no later
traversal retries that scene. View access goes through
`traversal_views` while a token is active; ended or retired tokens are rejected.
A deferred-request read returns a value snapshot on the device thread outside
mutating boundaries. The `create` operation must not retain or acquire a
resource when it returns null or throws; a non-null returned resource is
recorded immediately and remains under scene cleanup even if later registration
fails. A failed registration is still unregistered during retirement because
the callback may have partially changed renderer state. Accounting callbacks
already completed in a traversal that later fails are not replayed or rolled
back.

## Current production integration boundary

The coordinator is not connected to the production SDL loop yet. The window is
created in `src/platform/sdl_startup.cpp`; `run_sdl_gpu_runtime` in
`src/platform/sdl_gpu_runtime.cpp` claims it for the GPU device, owns GPU
resources, and owns the frame loop. That loop currently has no active-view
traversal. Its `scene` input is a diagnostic `SceneGpuPlan`, while ordinary
startup passes no scene and no admitted intro frame.

`SdlIntroRenderer::attach_camera_display` currently selects one camera node,
and `prepare` uses that selected camera for every submitted draw. To render
multiple admitted views, the renderer needs per-view camera input and the SDL
frame loop must traverse immutable admitted views on the device thread. The
coordinator's create/register/unregister/release callbacks must be backed by
actual SDL renderer resources and retired before the GPU device is released.

The normal intro session retained by the SDL runtime receives the supported
post-construction reader bracket, but startup does not yet execute the complete
normal scene lifecycle, event-16 activation, requested-camera admission, or
camera registration into the SDL renderer. `NormalIntroSceneHost` now exposes
`dispatch_first_cut_event16` to route the typed first-cut lifecycle handoff
through the controller owned by that host. The method is still an unbound
boundary: startup does not construct the host or supply the manager gates,
sequence-preparation/group-state operations, and player phase callbacks it
requires. World startup likewise lacks a
live active-root producer; the existing world frame input is diagnostic and
is not a normal scene-manager result. Intro and world frame inputs are also
currently mutually exclusive in `run_sdl_gpu_runtime`.

These are required integration inputs for parity, not claims of completed
rendering. Keep the incomplete fallback visible until real lifecycle,
camera/view, and frame data are connected; do not turn diagnostic plans into
the ordinary scene source.
