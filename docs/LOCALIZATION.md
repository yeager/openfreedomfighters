# Localization plan

## Current status

The project has a deliberately small localization foundation for its own F10
graphics-overlay vocabulary. It has complete, independently authored UTF-8
catalog entries for the 20 target locales below, keyed by stable semantic IDs.
It resolves an explicit locale first, a platform locale second, and English
last. The live F10 draw path always reads the system-preferred locale through
SDL. `--locale TAG` is an explicit override (for example, `--locale sv-SE`).
It does not read a retail LOC format or ship retail text. This is not localized
Original presentation, game-wide l10n, or a claim of complete game coverage.

The native startup data-error dialog uses the same catalog. Its concise title,
cause and relaunch instruction are selected from the stable `InstallError`
value; the verifier's raw diagnostic remains a separate technical field and is
included only as labelled diagnostic detail. This keeps user-facing wording
translatable without discarding the exact reason needed for support.

The current catalog has no external catalog loader, plural/select support,
bidirectional layout, shaping, font fallback, or locale-specific layout tests.
The countdown is the single constrained `{seconds}` project-authored pattern;
other formatting contracts remain future work.

The runtime can load bounded font bytes from the user's startup archive for the
diagnostic overlay. That is not evidence that the fonts cover any target locale:
the current SDL_ttf build has HarfBuzz disabled, and no glyph-coverage or
complex-script test exists. See [the retail-font runtime contract](RETAIL_FONT_RUNTIME.md).

The engine will support Unicode, locale-aware formatting, font fallback, right-to-left layout, controller-glyph substitution, and UI expansion testing. Swedish is a launch requirement.

Current F10 target set (20): English, Swedish, Danish, Norwegian Bokmal,
Finnish, German, French, Spanish, Italian, Portuguese (Brazil), Polish, Czech,
Hungarian, Romanian, Turkish, Russian, Ukrainian, Japanese, Korean, and
Simplified Chinese. This is coverage only for the project-authored F10 overlay;
Original and the rest of the game do not yet meet this target set.

The list balances the original market, Nordic coverage, broad PC/Steam audiences, and script/layout diversity. It can change after font licensing and community-maintainer review.

Translation catalogs are keyed by stable semantic IDs. Original retail strings are read at runtime from the user's data where technically possible and are never committed. New Swedish and other translations require independently contributed text with an explicit license grant. CI checks missing keys, placeholders, accelerator collisions, and pseudo-localized UI overflow.

## Clean-room delivery plan

1. Recover only non-expressive format and lookup behavior for retail LOC data.
   Keep retail text in the user installation; never export it, add it to test
   fixtures, or use it as a public catalog seed.
2. Add a locale resolver (explicit setting, platform preference, deterministic
   English fallback) and a UTF-8 message API keyed by stable semantic IDs.
   Its public tests use newly authored strings only.
3. Define a versioned, independently licensed project catalog format with
   placeholder and plural/select contracts. Add a catalog verifier that rejects
   missing IDs, incompatible placeholders, duplicate accelerators, invalid
   UTF-8, and unbounded entries.
4. Use a shaping-capable text stack and a licensed fallback-font chain. Verify
   glyph coverage and layout for Latin, Cyrillic, Japanese, Korean, Simplified
   Chinese, and right-to-left text before claiming locale support.
5. Produce 20 complete, in-context reviewed catalogs of newly authored UI text,
   including Swedish. Retail dialogue and other expressive retail text remain
   user-data-backed until a separately documented lawful source is available.
6. Add pseudo-localization, narrow/wide viewport, controller-glyph, and
   screenshot-free geometry/layout tests for every locale. Run native runtime
   validation on Windows, macOS, Linux, and Steam Deck.
