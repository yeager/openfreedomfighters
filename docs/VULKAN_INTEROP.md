# SDL Vulkan interop boundary

The optional `OFF_ENABLE_SDL_VULKAN_INTEROP` build pins SDL 3.4.10 and applies
the repository's scoped external-pass patch. The `off_sdl_vulkan_interop`
adapter validates the command buffer, callback, and unique texture-use list,
then forwards the callback to SDL. It does not retain Vulkan handles or texture
metadata beyond the call.

The callback runs synchronously while recording an SDL-owned command buffer.
It must not submit work, retain callback-scoped handles, or change the listed
image layouts. SDL owns transitions and final submission. Treat a failed call
as command-buffer failure and cancel that buffer.

This is an interop primitive, not an upscaler. No application pass currently
uses it to resolve a normal gameplay frame. Vendor capabilities remain
disabled until a normal scene producer supplies validated color, depth, motion,
exposure, camera, instance, and history inputs and an adapter consumes them.
See [Modern graphics](MODERN_GRAPHICS.md), [DLSS](DLSS.md), and [FSR](FSR.md).
