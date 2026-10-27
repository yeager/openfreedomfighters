# Picture submission cache

The renderer may cache project-owned preparation results only when the cache key includes every project-defined input that affects the result. Resource generations and submission outcomes must be handled explicitly, and failed submissions must not publish stale state.

This page omits original cache keys, identity mappings, layout details, and source-derived behavior. Synthetic renderer fixtures are the only public validation inputs.
