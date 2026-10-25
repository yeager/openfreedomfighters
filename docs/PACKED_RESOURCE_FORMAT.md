# Packed-resource handling

The data layer supports bounded decompression for selected resources in the
user's installed game data. It checks declared input and output ranges and
rejects unsupported or inconsistent encodings before a decoded view is exposed.

This document intentionally omits game-specific envelope layouts, tags, sizes,
checksums, resource inventories, and executable-analysis results. Implementation
details remain in the source tree; no original packed or decoded payloads belong
in this repository.
