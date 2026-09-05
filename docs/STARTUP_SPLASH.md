# Native startup splash

Normal launches begin with original OpenFreedomFighters artwork before any
retail-data result is accepted. The source PNG and its lossless BMP runtime
derivative live in `assets/branding`; their provenance and license are recorded
beside them. CMake copies the BMP beside the built executable and installs it at
`bin/assets/openfreedomfighters-splash.bmp`. The runtime resolves it from
`SDL_GetBasePath`, so launching from another working directory does not change
asset discovery.

The splash uses SDL's CPU window surface and built-in BMP decoder. It therefore
does not require a GPU device, a retail archive, or another image dependency.
Only SDL's video subsystem is initialized in this preflight; gamepad setup stays
with the subsequent runtime.
The image is aspect-fit against black and redrawn on exposure or pixel-size
changes. The three-second deadline begins only after the first successful
`SDL_UpdateWindowSurface`. A monotonic-clock state machine keeps the splash
visible until the first tick at or after that deadline. If verification is
still running then, the image is replaced by a dark loading surface; it is not
kept beyond its specified duration.

Installation verification runs on a worker while the SDL main thread pumps
events. An early result remains behind the full three-second presentation. A
missing, invalid, unsupported, or omitted installation produces a concise
`SDL_ShowSimpleMessageBox` error parented to the startup window, then exits with
the existing data-error status. Closing the startup window or pressing Escape
hides it immediately;
the process safely joins any in-flight verifier before teardown.
Surface, window, and SDL-session lifetime is guarded in that destruction order.
Thread-launch failures, broken futures, standard exceptions, and non-standard
exceptions are converted to a platform-error result after the guards unwind;
none escape the public preflight boundary.

`--verify-only`, `--help`, and `--version` do not enter this path and remain
usable without a display. Argument syntax and output-path validation also remain
CLI diagnostics.

The current safe integration destroys the startup window after successful
verification and lets the existing SDL GPU runtime create its own window. This
can produce a short platform-dependent transition. A later ownership refactor
will reuse one window after destroying its CPU surface, and will move subsequent
retail asset preparation behind the responsive startup host. Neither limitation
changes the splash deadline or the missing-data dialog path.
