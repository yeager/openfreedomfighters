# User-owned interface textures

Interface artwork is read from the user's installation at runtime and is not included in this repository. Retail texture inventories, identifiers, layouts, and measurements are intentionally omitted.

The UI renderer should resolve textures through an owned registry, validate decoded images before upload, and provide a clear error when a required resource is missing. Project-authored fallback UI must not be labeled as original artwork.
