# Menu visual-layout evidence

The current F10 overlay is a functional, project-authored diagnostic surface.
It uses a verified font from the installed startup archive, but it is not yet a
retail-faithful menu. In particular, the startup texture decoder intentionally
has no UI-picture role mapping until that mapping has been recovered; the
runtime therefore uploads no retail UI texture and draws no guessed chrome.

`tools/menu_visual_layout_trace.py` is the private evidence boundary for the
next step. An external observer records only viewport dimensions, panel and
element rectangles, and the small categorical roles accepted by the tool. It
rejects text, image data, resource identifiers, paths, hashes, addresses, and
unknown fields. The input and output must remain outside the repository and the
output is created once without overwriting an existing file.

This evidence can establish placement, rhythm, focus bounds, selector anchors,
and confirmation-panel geometry. It cannot establish artwork, texture-role
mapping, font-role semantics, colors, animations, sounds, input behavior, or
scene activation. Those require their own reviewed evidence. Until then, the
F10 renderer must keep using the labelled project-authored fallback rather than
imitating retail UI with generated assets.
