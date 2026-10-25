# Ordinary scene and renderer traversal

The native scene and renderer coordinators define explicit callback boundaries.
Scene update, component work, position propagation, maintenance, view admission,
backend traversal, and presentation remain separate services supplied by the
application.

## Coordinator contract

Each coordinator must document callback order, admission conditions, bounded
work, identity lifetime, and failure behavior. A failed callback must not be
reported as a successful frame. Renderer readiness and scene readiness are
separate; callback completion alone does not prove geometry submission, GPU
execution, or swapchain presentation.

## Verification

Use synthetic callback traces to verify ordering, skipped work, repeat calls,
reentrancy guards, and failure prefixes. Safety limits are project policy and
must not be presented as original runtime capacities. Keep installation-derived
observations and executable analysis out of this document.
