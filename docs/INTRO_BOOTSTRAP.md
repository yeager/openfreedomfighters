# Intro startup boundary

The normal application path does not yet play the authored intro. It can present a clearly labelled static fallback, but that is not a scene frame and does not advance a cut or enter the menu.

The intro runtime and startup coordinator contain project-owned loading and lifecycle interfaces. Their presence does not prove that all required scene data, callbacks, camera state, audio, updates, or draw submission are connected. Keep startup fail-closed until one owning session can complete those operations and unwind failures safely.

## Current production-path audit

Normal startup constructs and retains an `IntroRuntime`/`NormalIntroSceneSession`, then continues through SDL startup without constructing or activating a `NormalIntroSceneHost`. The SDL GPU runtime accepts a `NormalIntroSceneHostFrameBridge`, but the normal call site supplies no bridge. It therefore draws only the labelled static fallback when that fallback is available; it does not display an admitted authored intro frame.

The missing work is in the normal application path: create the scene host from the supported intro session, bind it to the SDL frame bridge, and advance its lifecycle from the normal frame loop. The repository has diagnostic preview and readiness paths, but they do not activate an authored intro scene. Keep those paths separate from normal playback and fail startup cleanly when required scene services are unavailable.

This is a source-level application status. It does not claim that a future connected build has been verified to render the intro.

## Remaining project work

- Construct and activate `NormalIntroSceneHost` in normal startup after the supported intro session is ready.
- Bind an admitted frame bridge to SDL and advance the scene from the normal frame loop.
- Connect camera updates, audio, ordered renderer submission, and the menu transition to the owning session.
- Preserve the Original path and capability-gate optional Modern features.

This document omits private decompilation, retail payloads, binary offsets, and corpus measurements. See [CLEAN_ROOM.md](../CLEAN_ROOM.md), [INTRO_AND_MENU_PLAN.md](INTRO_AND_MENU_PLAN.md), and [MODERN_GRAPHICS.md](MODERN_GRAPHICS.md).
