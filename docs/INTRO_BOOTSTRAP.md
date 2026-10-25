# Intro startup boundary

The normal application path does not yet play the authored intro. It can present a clearly labelled static fallback, but that is not a scene frame and does not advance a cut or enter the menu.

The intro runtime and startup coordinator contain project-owned loading and lifecycle interfaces. Their presence does not prove that all required scene data, callbacks, camera state, audio, updates, or draw submission are connected. Keep startup fail-closed until one owning session can complete those operations and unwind failures safely.

## Remaining project work

- Complete the normal scene's source-backed loading and lifecycle admission.
- Connect the active camera, ordinary update, audio, and ordered renderer submission to the owning scene session.
- Replace the one-frame fallback with a repeatable scene lifecycle and a verified menu transition.
- Preserve the Original path and capability-gate optional Modern features.

This document omits private decompilation, retail payloads, binary offsets, and corpus measurements. See [CLEAN_ROOM.md](../CLEAN_ROOM.md), [INTRO_AND_MENU_PLAN.md](INTRO_AND_MENU_PLAN.md), and [MODERN_GRAPHICS.md](MODERN_GRAPHICS.md).
