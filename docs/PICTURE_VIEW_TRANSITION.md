# Picture view transitions

View changes must be handled by the renderer's owning view service and must invalidate any dependent cached or temporal state. This page omits source-derived masks, camera flags, and transition values.

The draw path must not infer an active gameplay camera from a diagnostic view. Synthetic fixtures should cover view replacement, missing views, and invalidation behavior.
