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

The current external-pass callback is too late to initialize NVIDIA
Streamline. It runs while recording a command buffer, after SDL has already
created the Vulkan instance, device, queues, and swapchain. The SDL patch's
extension-discovery callback can add instance and per-device extension names.
SDL 3.4.10's `SDL_GPUVulkanOptions` also accepts Vulkan 1.0 features and
recognized Vulkan 1.1-1.3 feature-chain entries. SDL does not expose arbitrary
extension feature structures or a way to request additional queue families or
queue counts; its Vulkan backend creates one graphics/present queue.

SDL's Vulkan loader honors `SDL_HINT_VULKAN_LIBRARY`. That gives a Windows
Streamline adapter a possible proxy-loader route, but this project does not yet
initialize Streamline before SDL creates the GPU device, select or validate
`sl.interposer.dll`, or verify that SDL's resolved creation and presentation
calls use the required Streamline proxies. The current SDL runtime passes a
zero-initialized `SDL_GPUVulkanOptions`, so it requests no extra features or
extensions through those options. The external-pass API itself has no swapchain
or presentation callback.

Streamline's Vulkan manual-hooking guide requires `slInit` before Vulkan
creation. Its creation proxies can supply feature requirements and extra
queues. A native-creation path must satisfy the queried requirements and call
`slSetVulkanInfo` immediately after device creation. Manual hooking also
requires routing the relevant swapchain/present operations and calling
`presentCommon()` exactly once per presented frame. SDL resolves Vulkan
entrypoints through the selected loader, but a Streamline proxy-loader path
has not yet been integrated or validated here.

The next bridge work is to evaluate the supported Windows proxy-loader route:
initialize Streamline before SDL's first Vulkan call, choose the interposer
through `SDL_HINT_VULKAN_LIBRARY`, and verify that adapter requirements,
device selection, swapchain lifecycle, and presentation are handled by the
proxies. If proxy loading cannot meet SDL's lifetime and platform requirements,
extend SDL's device options for additional queues and unsupported feature
structures, and add device-created and present callbacks. Any implementation
must retain SDL's ownership of submission and teardown, remain optional, and
fail closed when Streamline or a feature is unavailable. This Vulkan boundary
alone would still not enable DLSS: a real scene producer, correctly tagged
temporal inputs, feature/runtime checks, permitted packaging, and image
validation are also required. See NVIDIA's [Streamline 2.14.16 programming
guide](https://github.com/NVIDIA-RTX/Streamline/blob/v2.14.16/docs/ProgrammingGuide.md)
and [manual-hooking guide](https://github.com/NVIDIA-RTX/Streamline/blob/v2.14.16/docs/ProgrammingGuideManualHooking.md),
and the [SDL 3.4.10 Vulkan loader](https://github.com/libsdl-org/SDL/blob/release-3.4.10/src/video/SDL_video.c).
