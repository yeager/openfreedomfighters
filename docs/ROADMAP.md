# Roadmap and acceptance gates

The roadmap is organized around a playable, compatible application rather than isolated subsystems.

## Core delivery

- Load user-owned installation data through bounded, validated parsers.
- Complete normal scene activation, ownership, update, input, and teardown.
- Reproduce intro playback and transition into a navigable main menu.
- Render scene geometry through a verified object, transform, camera, and draw-record path.
- Preserve Original compatibility while adding independently selectable Modern rendering features.
- Keep temporal and vendor upscalers unavailable until real frame inputs and native submission are connected.
- Verify Windows, Linux, Steam Deck, and macOS behavior with synthetic fixtures and user-owned installations.

## Release gate

A feature is complete only when its end-to-end path works in the normal application, its failure and fallback behavior is understood, and its platform requirements are checked. Diagnostic scenes and isolated component probes do not satisfy a playable milestone.

Retail assets, private observations, executable analysis, corpus measurements, and credentials must remain outside version control. See [CLEAN_ROOM.md](../CLEAN_ROOM.md).
