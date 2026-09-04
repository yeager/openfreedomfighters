# Localization plan

The engine will support Unicode, locale-aware formatting, font fallback, right-to-left layout, controller-glyph substitution, and UI expansion testing. Swedish is a launch requirement.

Initial target set (20): English, Swedish, Danish, Norwegian Bokmal, Finnish, German, French, Spanish, Italian, Portuguese (Brazil), Polish, Czech, Hungarian, Romanian, Turkish, Russian, Ukrainian, Japanese, Korean, and Simplified Chinese.

The list balances the original market, Nordic coverage, broad PC/Steam audiences, and script/layout diversity. It can change after font licensing and community-maintainer review.

Translation catalogs are keyed by stable semantic IDs. Original retail strings are read at runtime from the user's data where technically possible and are never committed. New Swedish and other translations require independently contributed text with an explicit license grant. CI checks missing keys, placeholders, accelerator collisions, and pseudo-localized UI overflow.

