# Picture resource integration

This page no longer documents retail-specific resource layouts, keys, or measurements.

Picture resources are loaded from the user's installation through bounded parsers and retained by the owning scene. GPU upload is separate from scene admission and draw submission. The renderer must validate dimensions, decoded data, and ownership before creating GPU resources, then release them on the device-owning thread.
