# Intro initialization presentation

`SdlIntroPresentation` supplies the renderer services used by the retained
controller's phase-two callback. It binds dimension queries, full-window viewport,
stencil presence, clear and present. The engine's first-renderer lookup remains
external, as do input, global properties, audio, scene time and lifecycle admission.

The adapter owns a linear color backbuffer and an explicitly selected depth
attachment. Clear records the existing viewport-clear renderer into those private
buffers. Present acquires the window swapchain, blits the color buffer, submits
and waits for completion. The controller calls this pair twice. Neither operation
invokes scene traversal or increments `RendererFrameClock`.

## Lifetime and failure rules

The caller supplies an already claimed window and live device. Both must outlive
the adapter and its callbacks. Calls run on the window thread with exclusive
window/command access. Stable dimensions, no pending configuration/reset request,
and applicable profiling services are caller requirements; checking dimensions
and texture format cannot establish those conditions by itself.

Before swapchain acquisition, a pending clear can be cancelled. SDL forbids
cancelling after acquisition, so failure at that point submits the pending
command as terminal cleanup. This may present a buffer but is reported as failure,
not successful initialization. Clear/present failures poison the adapter and
retain completed presentation counts. Missing swapchain images, resize and
device recovery are not silently treated as success.

## Verification and remaining integration

The local Vulkan test uses SDL window and swapchain operations, executes both
presentation pairs, and checks that clear alone does not present. It also checks
invalid identity/viewport requests, cancellation, and failure after a second
unpresented clear. Input/audio/time services in that test are independent fixtures,
not observed original startup state. Pixel and depth/stencil correctness are
covered separately by the existing clear-renderer readback tests.

The normal test passes on Wayland/Vulkan. The targeted ASan/UBSan run passes
with SDL's offscreen video driver and leak
detection enabled. That proves the adapter's command/swapchain path, not visible
desktop output. The Wayland sanitizer run reports a 320-byte leak
with allocation stacks in Fontconfig/Pango/GLib after libdecor/GTK initialization
warnings. That run is not clean; library stack attribution alone does not rule
out a contributing application lifecycle issue. Tests print both video and GPU
drivers so these results remain distinguishable.

CI run 34043926700 at commit `942b2c4` completed this test on Cocoa/Metal and
Windows/Direct3D 12, with two submissions on each backend and no test skip.
The Linux CI presentation test was skipped because its required platform path
was unavailable; it is not Linux presentation evidence. Local Wayland/Vulkan
testing remains separate.

This bridge is linked into the executable but is not automatically invoked by
normal startup. Connecting the real admitted lifecycle and shared scene services
remains necessary; the bridge alone does not start the intro.
