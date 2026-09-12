# Text layout boundary

`make_text_layout_boundary` is a bounded, source-free admission step for
project-authored UI text. It validates scalar UTF-8, limits input to 4 MiB and
returns byte spans that must remain indivisible until a text shaper consumes
them. Allocation failure is an admission failure.

The current segmenter intentionally implements only a documented grapheme
subset: CRLF, combining marks, a small set of spacing marks and prepend
characters, Hangul syllables, regional-indicator pairs, and common emoji ZWJ
sequences. It is not a complete Unicode UAX #29 implementation and must not
be presented as one.

It does not shape glyphs, choose fallback fonts, measure text, or resolve bidi
runs. Those steps require a deterministic cross-platform integration of
HarfBuzz, FreeType, and FriBidi (or generated, pinned Unicode property tables
for the applicable standard version). Until then, the boundary is intended
only to prevent the renderer from splitting recognized user-perceived clusters.
