# Intro startup boundary

The normal application path does not yet play the authored intro. It can present a clearly labelled static fallback, but that is not a scene frame and does not advance a cut or enter the menu.

The intro runtime and startup coordinator contain project-owned loading and lifecycle interfaces. Their presence does not prove that all required scene data, callbacks, camera state, audio, updates, or draw submission are connected. Keep startup fail-closed until one owning session can complete those operations and unwind failures safely.

## Current production-path audit

Normal startup constructs and retains an `IntroRuntime`/`NormalIntroSceneSession`, then continues through SDL startup without constructing or activating a `NormalIntroSceneHost`. The SDL GPU runtime accepts a `NormalIntroSceneHostFrameBridge`, but the normal call site supplies no bridge. It therefore draws only the labelled static fallback when that fallback is available; it does not display an admitted authored intro frame.

The missing work is the production-owned service chain, not a renderer submission call: reader and loader-tail completion, global lifecycle admission, MovieControl phase two and event delivery, requested-camera routing, renderer view admission, legal-picture activation, and frame assembly must all be supplied by their real owners. Static source inspection has not found a production call site that supplies this chain. A bounded executable trace now connects MovieControl's class/selector registration to a global class/method metadata registry. That registry adds and queries descriptors, but the trace does not establish delivery to a live MovieControl instance or activation of event 16. Do not turn diagnostic preview data into an admitted frame or fabricate these callbacks to make the bridge non-null.

This audit is source-only and intentionally contains no executable offsets, retail payload data, or decompiled listings. It verifies why the current build cannot be claimed to play the intro; it does not prove what a future fully connected build will render.

## Remaining project work

- Complete the normal scene's source-backed loading and lifecycle admission.
- Connect the active camera, ordinary update, audio, and ordered renderer submission to the owning scene session.
- Replace the one-frame fallback with a repeatable scene lifecycle and a verified menu transition.
- Preserve the Original path and capability-gate optional Modern features.

This document omits private decompilation, retail payloads, binary offsets, and corpus measurements. See [CLEAN_ROOM.md](../CLEAN_ROOM.md), [INTRO_AND_MENU_PLAN.md](INTRO_AND_MENU_PLAN.md), and [MODERN_GRAPHICS.md](MODERN_GRAPHICS.md).
