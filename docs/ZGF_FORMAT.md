# Embedded-resource bundle handling

The bundle reader exposes named embedded resources through bounded views. It
validates the container and each member before consumers can access content; it
does not extract original assets to the repository or host filesystem.

This document intentionally omits game-specific signatures, layouts, tags,
counts, payload sizes, and original resource names. The implementation and
synthetic malformed-input fixtures remain in the source tree.
