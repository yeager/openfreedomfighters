# Scene controller lifecycle boundary

This page no longer publishes source-derived callback order, flags, values, or private analysis findings.

The controller must retain one canonical scene state and accept each lifecycle transition at most once. Phase entry, external services, event dispatch, failure, and cleanup require a source-free reviewed contract before normal startup can depend on them. A completed isolated callback does not establish whole-scene readiness.

Observer output should use local synthetic identifiers and generic categories only. Raw captures, addresses, offsets, symbols, memory dumps, retail data, and decompilation must remain private.
