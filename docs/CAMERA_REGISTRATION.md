# Camera membership and audio listener

Camera loading, renderer registration, view admission, and audio-listener
selection are separate runtime responsibilities. A camera becomes a render view
only through an explicit backend admission interface. Audio uses an explicit
listener when configured and otherwise follows the application's documented
fallback policy.

Registries must define duplicate handling, stale-owner cleanup, identity lifetime,
and callback failure behavior. Resource context and parent relationships remain
separate typed values. A synthesized camera must be marked as project-created
rather than represented as an authored resource.

## Normal startup boundary

Startup may create a fallback camera only after the scene host establishes the
required resource state and services. Construction failures must not be treated
as successful or silently retried over partially published state. Synthetic
integration tests can verify ordering and lifetime; they do not establish parity
with an external application or prove that a view was rendered.
