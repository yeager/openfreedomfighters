# Portable data layer

The data layer reads the user's installed game resources through bounded archive
and stream interfaces. Its parsers reject malformed ranges and unsupported
encodings before exposing typed records to runtime systems. Original assets and
payloads remain in the user's installation.

## Responsibilities

- Validate archive structure, member ranges, and decoded sizes.
- Provide read-only overlays and bounded random access for mounted user data.
- Parse resource records into portable types with checked references.
- Keep parsing, runtime ownership, rendering, and playback as separate stages.
- Use independently authored synthetic fixtures for malformed-input handling.

## Boundaries

This documentation intentionally omits game-specific file layouts, signatures,
identifiers, values, corpus inventories, measurements, original strings, and
payload details. It also omits conclusions drawn from private executable
analysis. Detailed parser behavior is represented by the maintained
implementation and synthetic tests, not by published retail-derived data.
