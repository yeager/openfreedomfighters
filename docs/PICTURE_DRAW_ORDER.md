# Picture draw ordering

The rendering path must preserve the ordering supplied by its live producer and validate records before binding resources or issuing draws. This page omits retail-derived ordering fields and values.

Missing or malformed records must not be converted into guessed draws. Keep resource binding, view changes, and material state as separate operations with explicit ownership.
