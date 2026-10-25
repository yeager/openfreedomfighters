# SDL intro renderer boundary

The SDL renderer can present validated GPU resources, but renderer initialization alone does not establish intro playback. This page omits resource inventories and source-derived image details.

Normal presentation requires an admitted scene frame from the owning scene session. A static fallback or diagnostic image must remain clearly labeled and must not advance timeline, audio, or scene lifecycle state. GPU creation, submission, and release must remain on the SDL device's owning thread.
