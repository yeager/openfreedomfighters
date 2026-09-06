# Native startup splash

Normal launches begin with original OpenFreedomFighters artwork before any
retail-data result is accepted. The source PNG and its lossless BMP runtime
derivative live in `assets/branding`; their provenance and license are recorded
beside them. CMake copies the BMP beside the built executable and installs it at
`bin/assets/openfreedomfighters-splash.bmp`. The runtime resolves it from
`SDL_GetBasePath`, so launching from another working directory does not change
asset discovery.
The asset-copy target runs on each executable build with `copy_if_different`,
so artwork-only changes and missing staged assets do not require relinking.

The splash uses SDL's CPU window surface and built-in BMP decoder. It therefore
does not require a GPU device, a retail archive, or another image dependency.
Only SDL's video subsystem is initialized in this preflight; gamepad setup stays
with the subsequent runtime.
The image is aspect-fit against black and redrawn on exposure or pixel-size
changes. The three-second deadline begins only after the first successful
`SDL_UpdateWindowSurface`. A monotonic-clock state machine keeps the splash
visible until the first tick at or after that deadline. If CPU startup work is
still running then, the image is replaced by a dark loading surface; it is not
kept beyond its specified duration during loading.

Installation verification runs on a worker while the SDL main thread pumps
events. Only successful verification allows that worker to prepare the decoded
scene, startup images and font bytes. This is CPU-only preparation; SDL window,
GPU and font-rasterizer operations remain on the main thread. Owned prepared
data is consumed only after the worker is joined. An early result remains
behind the full three-second presentation. A
missing, invalid, unsupported, or omitted installation produces a concise
`SDL_ShowSimpleMessageBox` error parented to the startup window, then exits with
the existing data-error status. Before showing this dialog, the artwork is
redrawn even when verification outlasted the timed splash. It remains the error
backdrop until dismissal; this does not restart the three-second timer.
Archive-validation errors identify the installation-relative archive and, when
the failure occurs while parsing a member, its member name. The original
integrity error is retained; no archive payload is included in the message.
Asset-preparation failures likewise restore the artwork and show a parented
diagnostic, but retain runtime-error exit status 4 instead of verification-error
status 3. They do not silently substitute placeholder assets.
Closing the startup window or pressing Escape
hides it immediately;
the process safely joins any in-flight verifier before teardown.
Surface, window, and SDL-session lifetime is guarded in that destruction order.
Thread-launch failures, broken futures, standard exceptions, and non-standard
exceptions are converted to a platform-error result after the guards unwind;
none escape the public preflight boundary.

`--verify-only`, `--help`, and `--version` do not enter this path and remain
usable without a display. Argument syntax and output-path validation also remain
CLI diagnostics.

After successful verification and CPU preparation, a move-only owner transfers
the existing window and SDL lifetime to the main entry point. The GPU runtime
borrows that same window; it does not create a replacement or restart SDL video.
It explicitly destroys the SDL-owned window surface before claiming the window
for GPU use, and checks that transition for failure. GPU resources and the
window claim are released before the final owner destroys the window and quits
SDL. Gamepad initialization has a separate balanced subsystem lifetime.
This removes the deliberate window destruction/recreation transition; it does
not promise uninterrupted pixels while a platform creates its swapchain.
Cancellation and error paths retain their original teardown and popup behavior.

## Verification evidence

The orchestration tests cover verifier failure/exception, skipped preparation,
preparation exceptions, successful owned output and cooperative cancellation
before, between and during stages. A targeted ASan/UBSan run passes.
An SDL dummy-driver test checks move-only ownership, unchanged window identity
through both handoffs, preserved software surface, surface release without
window destruction, and final SDL shutdown. It does not emulate a GPU swapchain.
On the ARM64 Linux development host, a real supported-install launch with
`--frame-limit 1` completed successfully through this worker pipeline and the
SDL GPU Vulkan diagnostic renderer: six startup images uploaded and four
retail fonts loaded. This is a startup smoke test, not a faithful menu or
gameplay pass. A headless verification run with an intentionally unavailable
SDL video driver also succeeded without creating a window.
The supported-install launch was repeated successfully after the single-window
handoff change. Host GTK/libdecor and EGL driver warnings remain; successful
Vulkan completion does not establish a warning-free desktop environment.
