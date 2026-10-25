# Startup menu boundary

The startup coordinator handles project-owned startup decisions and failure reporting. It does not currently establish a complete menu scene or authorize intro playback.

A menu scene may become active only after its own data, lifecycle, input, rendering, and teardown paths are connected through the owning scene manager. A diagnostic or static preview is not evidence of menu activation.

Public documentation intentionally omits private executable traces, runtime identifiers, offsets, and retail data. See [CLEAN_ROOM.md](../CLEAN_ROOM.md) and [INTRO_AND_MENU_PLAN.md](INTRO_AND_MENU_PLAN.md).
