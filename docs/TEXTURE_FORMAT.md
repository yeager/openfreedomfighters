# Texture-resource handling

The texture reader exposes validated image records, mip chains, palettes, and
sequence relationships to the portable CPU and GPU paths. It checks dimensions,
indices, and byte ranges before decoding or upload.

Decoded images and original names remain local to the user's installation. This
document intentionally omits game-specific layouts, encoding values, corpus
measurements, and pixel data.
