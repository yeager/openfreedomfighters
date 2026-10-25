# Bounds propagation

Bounds updates belong to the owning scene and spatial service. This page no longer publishes source-derived flags, masks, or arithmetic values.

A producer must validate finite input, propagate only through established relationships, and report whether an update was accepted. Unknown parentage or stale identities must not be treated as a valid world-space result. Synthetic fixtures should cover invalid input, cancellation, and hierarchy changes.
