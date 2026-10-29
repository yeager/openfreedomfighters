# Deferred-reader implementation boundary

This page no longer publishes source-derived reader inventories, payload shapes, or population counts. Detailed analysis stays in private research storage.

The runtime should admit a reader only when its owner identity, bounded input, and output state are validated. Reader order and failure behavior must be covered by independently authored synthetic cases. Unsupported forms must remain unavailable rather than being inferred from a similar component.

The controller parser can validate an explicitly bounded block copy and locate
its reference fields. A successful parse does not prove the copy's origin,
owner, reference-service generation, or relocation state. The normal reader
does not yet consume a verified relocated copy, so this API does not enable
intro playback.
