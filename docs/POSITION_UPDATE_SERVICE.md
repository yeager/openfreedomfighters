# Position update service

Position changes should be queued through a scene-owned service and applied only to validated live identities. This page omits source-derived queue sizes, flags, masks, and callback order.

The service must define cancellation, stale-handle rejection, and bounds propagation. It must not create world transforms from unrelated source records. Synthetic fixtures should cover queue failure and scene teardown.
