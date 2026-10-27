# Intro startup boundary

The application does not yet play the authored intro. Normal startup can show a labelled static fallback, but it does not advance an intro scene or enter the main menu.

The runtime needs an owning session that connects validated data, lifecycle updates, camera and audio state, draw submission, and teardown. Empty callbacks or diagnostic previews do not satisfy this requirement.

Original executable analysis and installation observations remain private. This page makes no claim about the original startup sequence or its internal services.
