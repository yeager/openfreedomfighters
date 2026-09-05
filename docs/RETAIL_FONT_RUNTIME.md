# Retail font runtime contract

OpenFreedomFighters reads UI fonts from the user's installed
`FF-StartUp.ZIP`. Font bytes remain in memory and are never extracted into the
source tree, build tree, install tree, screenshots, logs, or test artifacts.
The public repository contains only the loader, renderer, and synthetic parser
fixtures.

## Ownership and shutdown order

An SDL IO stream opened over a memory span does not own that span. Therefore,
the `RetailUiFontSet` that owns each sfnt byte vector must remain alive until
all `TTF_Font` objects opened from those vectors have been closed. The current
surface-rasterization path uses the following strict reverse-construction
shutdown order:

1. destroy each temporary rendered text surface after copying its pixels;
2. close every `TTF_Font`;
3. close its non-owning SDL IO stream explicitly, including failed font-open
   paths;
4. call `TTF_Quit`;
5. release the overlay's GPU resources, then the GPU device, window, and SDL.

Every partial-construction failure follows the same ordering for the objects
that were successfully created. If this path later adopts SDL_ttf's GPU text
engine, every `TTF_Text` must be destroyed before that engine, and borrowed
draw sequences must be consumed before their text object changes or is
destroyed.

## Text and glyph behavior

UI strings are UTF-8 and lengths passed to SDL_ttf are byte lengths, not
Unicode code-point counts. An invalid UTF-8 string, a missing required glyph,
an unbounded draw sequence, or an atlas/upload allocation failure is a normal
rendering failure: the runtime must keep running safely and report a generic
source-only error without including retail resource names or contents.

The first integration renders the English F10 diagnostic labels with a
verified retail font. It deterministically selects the first valid embedded
font in bundle order as a provisional role assignment; recovered font-role
semantics must replace that selection before retail-accurate UI acceptance.
It does **not** yet prove locale-wide glyph coverage,
font fallback, bidirectional layout, or complex-script shaping. Those remain
acceptance requirements for the 20-language localization phase. In
particular, a build with SDL_ttf's HarfBuzz integration disabled must not be
described as providing complete Unicode shaping.

## Bounds

The archive parser bounds font count, per-font byte size, sfnt table count, and
every sfnt table range before SDL_ttf sees the data. Runtime text generation
also retains the draw-list text-count and byte-count limits. GPU glyph atlas
dimensions, copied vertex/index totals, and upload byte calculations must be
bounded and overflow-checked before allocation. No atlas or draw sequence may
be trusted merely because it came from a structurally valid sfnt file.

## Verification

Automated tests use generated sfnt/ZIP fixtures only. They verify owned bytes,
archive removal after loading, exact extension matching, malformed table-range
rejection, and the absence of extracted files. A runtime smoke test using the
user's retail installation may verify that an actual embedded font opens and
draws, but must never copy that font into a persistent test fixture or upload
it as a CI artifact.
