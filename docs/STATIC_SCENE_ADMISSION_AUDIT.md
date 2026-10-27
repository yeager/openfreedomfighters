# Static scene admission boundary

The current startup image is a static fallback. It is not an admitted scene frame, does not advance an intro timeline, and does not prove a transition to a menu or gameplay.

Normal scene admission requires one owning runtime session to validate its inputs, activate lifecycle services, produce ordered draw records, submit them through the active view, and unwind failures safely. The current application has not verified that end-to-end path.

This public document omits original scene names, source records, owner/component relationships, timing values, and executable-derived observations.
