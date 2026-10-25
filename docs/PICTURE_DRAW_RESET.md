# Picture draw-state reset

The renderer should reset its project-owned cached state at a documented frame boundary and only after submission outcome is known. This page omits source-derived cache keys and material values.

If submission fails, preserve enough state to recover or recreate the renderer safely. Synthetic tests should cover success, cancellation, and reset ordering.
