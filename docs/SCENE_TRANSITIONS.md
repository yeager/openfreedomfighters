# Scene transitions

Scene changes belong to an owning scene manager. A transition must prepare the destination, activate it once, route ordinary input and updates to the active scene, and retire the previous scene only after the handoff succeeds.

A resource load or diagnostic preview does not imply scene activation. Failed preparation must leave the current scene usable and release partially created resources. Intro completion and menu activation remain incomplete in normal startup.

This project-level contract intentionally contains no retail-specific class names, identifiers, offsets, or private executable findings. See [INTRO_AND_MENU_PLAN.md](INTRO_AND_MENU_PLAN.md) and [CLEAN_ROOM.md](../CLEAN_ROOM.md).
