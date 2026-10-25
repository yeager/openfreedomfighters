# Installed font runtime contract

The application may read fonts from a user-selected installation at runtime.
Font bytes remain in memory and must not be copied into source, build, install,
test, log, screenshot, or release artifacts. Public tests use generated font
fixtures only.

## Ownership and shutdown order

An SDL IO stream opened over a memory span does not own that span. The object
that owns the font bytes must outlive every font opened from them. Destroy
rendered surfaces, close fonts and streams, shut down SDL_ttf, then release GPU
resources and SDL objects in reverse construction order. Partial failures follow
the same rule for successfully created objects.

## Text and bounds

Treat input as UTF-8 and pass byte lengths to SDL_ttf. Invalid input, missing
glyphs, unbounded draw sequences, or allocation failure must produce a generic
error that contains no installation resource names or contents. Do not claim
complete Unicode shaping, fallback, or locale coverage until the selected text
backend has been validated for those features.

Bound font/archive parsing, text generation, glyph atlas allocation, and upload
arithmetic before allocating or passing data to the renderer. Structural
validation alone does not make font data trusted.

## Verification

Automated tests use generated font and archive fixtures. A local runtime smoke
test may use the user's installation, but its font data and captures must not
become persistent fixtures or CI artifacts.
