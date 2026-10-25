# Ordered drawing coordinator

The coordinator separates record selection, per-view association, resource preparation, and backend submission. This page intentionally omits retail-derived selection rules and values.

The owning renderer must provide a stable ordered set of validated records. Each stage must either complete or report failure; a partially prepared list must not be presented as a complete frame. Synthetic fixtures should cover empty input, reordered records, missing resources, and cancellation.
