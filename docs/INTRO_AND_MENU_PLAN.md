# Intro and main-menu implementation plan

## Target

A normal launch should present the project splash, play the authored intro, and enter a responsive main menu. The menu should support navigation, settings, localization, and a verified new-game request. Original mode must preserve compatibility behavior; Modern mode may improve rendering without changing simulation results. The game must retain usable fallbacks across Windows, Linux, Steam Deck, and macOS.

## Current boundary

The production path does not yet play the timed intro or establish the complete main menu. Existing diagnostic scenes and static previews are not evidence of normal scene activation, camera selection, playback, or gameplay rendering.

## Delivery sequence

- Complete the normal scene-loading and lifecycle path, including
  [construction from local directory records](SCENE_DIRECTORY_LOADING.md).
- Connect an owning scene session to update, input, rendering, audio, and teardown.
- Implement intro timing, transitions, and the transition into the menu from reviewed behavior contracts.
- Complete menu navigation, localization, and settings persistence.
- Add Modern rendering features and capability-gated temporal/vendor upscalers only when their real frame inputs and submission paths exist.
- Verify behavior on each supported platform with independently authored test data and user-owned installations.

Do not publish retail assets, payloads, private observations, executable analysis, or source-derived inventory data. See [CLEAN_ROOM.md](../CLEAN_ROOM.md).
