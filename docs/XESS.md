# Intel XeSS-SR integration plan

## Scope

Intel Xe Super Sampling Super Resolution (XeSS-SR) is the optional Intel
counterpart to DLSS Super Resolution for Modern+. It is never required to run
the game. The F10 graphics menu stores a XeSS request, but the current SDL-GPU
renderer cannot activate it and resolves the request to the available
spatial/native path instead. It does not expose a temporal fallback.

## Required renderer boundary

Intel's SDK consumes jittered input color, depth, motion vectors, exposure,
history-reset state, and an output texture, then records work through a native
graphics command object. OpenFreedomFighters must first own those temporal
inputs and the native backend objects. UI composition remains after upscaling,
and the selected presentation path cannot affect simulation, input, saves, or
replays.

XeSS-SR has documented D3D12 and Vulkan 1.1 integrations. A Windows D3D12
adapter is the first candidate. Vulkan support requires the SDK's queried
extensions and device features. No platform is enabled merely because headers
or a GPU vendor string are present.

## Packaging and build policy

The Intel SDK is not vendored or fetched by the build. A future opt-in build
switch will require a developer-supplied SDK root, validate the selected native
backend, and link only the API-specific library. Redistribution is subject to
Intel's current license. The adapter must verify the runtime, driver, GPU, and
feature support before activation; otherwise it reports the fallback and keeps
the game running.

XeSS Frame Generation and Xe Low Latency are separate future decisions. They
are not enabled by the XeSS-SR selection.

## Sources

- [Intel XeSS SDK](https://github.com/intel/xess)
- [XeSS-SR developer guide](https://github.com/intel/xess/blob/main/doc/xess_sr_developer_guide_english.md)
