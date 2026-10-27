# SDL intro renderer boundary

The SDL renderer can present validated GPU resources, but renderer initialization alone does not establish intro playback. This page omits resource inventories and source-derived image details.

Normal presentation requires an admitted scene frame from the owning scene session. A static fallback or diagnostic image must remain clearly labeled and must not advance timeline, audio, or scene lifecycle state. GPU creation, submission, and release must remain on the SDL device's owning thread.

## Current status

As of 2026-10-27, the only verified normal-startup visual remains the static intro fallback. No animated intro frame has been verified through scene admission, draw submission, and presentation. Diagnostic pictures, geometry previews, and synthetic renderer checks remain separate from normal playback.

The latest private static-analysis pass narrowed one intro-resource reader boundary, but did not connect its output to the active scene, view, or submitted draw. That result does not change the playback status and is intentionally kept out of this public repository.

The playback gate is end-to-end: the active scene must supply its camera and view state, admitted geometry must reach the renderer, and the resulting frame must be presented. Intro audio and lifecycle completion must follow the same admitted scene session before this milestone can be called playable.
