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

## Streamline initialization boundary

The current callback is too late and too narrow to initialize NVIDIA
Streamline. It runs while recording a command buffer, after SDL has already
created the Vulkan instance, device, queues, and swapchain. The SDL extension
discovery callback can add instance and per-device extension names, but cannot
provide Streamline's requested Vulkan feature chains, extra queues, or
creation-function proxies. The external-pass API also has no swapchain or
presentation hook.

Streamline's Vulkan manual-hooking guide requires `slInit` before Vulkan
creation. Its creation proxies can supply feature requirements and extra
queues; a native-creation path must instead satisfy those requirements
explicitly and call `slSetVulkanInfo` immediately after device creation. Manual
hooking also requires routing the relevant swapchain/present operations and
calling `presentCommon()` exactly once per presented frame. The current SDL
patch does none of that, so it cannot yet host a working Streamline adapter.

The next bridge work is therefore an SDL-owned Vulkan lifecycle and
presentation hook surface: initialization before instance creation,
Streamline-compatible instance/device creation (or a validated native
requirements path), device information immediately after creation, and one
per-frame presentation notification. Any implementation must retain SDL's
ownership of submission and teardown, remain optional, and fail closed when
Streamline or a feature is unavailable. This lifecycle hook alone would still
not enable DLSS: a real scene producer, correctly tagged temporal inputs,
feature/runtime checks, permitted packaging, and image validation are also
required. See NVIDIA's [Streamline 2.14.16 programming guide](https://github.com/NVIDIA-RTX/Streamline/blob/v2.14.16/docs/ProgrammingGuide.md)
and [manual-hooking guide](https://github.com/NVIDIA-RTX/Streamline/blob/v2.14.16/docs/ProgrammingGuideManualHooking.md).
