# Ordered picture dispatch

This page no longer publishes source-derived keys, masks, or resource identities.

A draw dispatcher should preserve the order supplied by its producer, validate each record before use, and keep view changes, resource changes, and material updates as distinct operations. Missing resources and malformed records must not be converted into plausible draws. Synthetic fixtures should cover ordering and state transitions.
