# Retail UI texture boundary

The graphics overlay may decode UI images only from the user's verified startup
archive at runtime. The repository contains no retail image, identifier,
inventory, fingerprint, or extracted payload.

`RetailUiTextureBinding` is the handoff from the recovered UI-picture resource
join.
It associates a semantic role with an ephemeral image index in the currently
parsed texture catalog. The decoder requires every role exactly once, validates
the recovered dimensions, rejects out-of-range and unintended shared bindings,
and then decodes mip zero through the bounded texture decoder. It never guesses
from a resource name, catalog order, or pixel similarity.

The current role contract covers the separate top and bottom scanline images,
top and bottom fill uses, eight ordered border fragments, and four arrow states.
Every role currently requires a distinct recovered image binding. Fragment
numbers describe recovered sequence slots, not spatial corner claims.

Large menu backgrounds are intentionally absent from this contract. Retail
evidence shows that they are heterogeneous multi-tile compositions. They must be
added only after the picture-resource draw-group/descriptor join proves the exact
tile family, order, transforms, UVs, state selection, and compositing behavior.
Selecting one plausible-looking catalog image would not reproduce the game.

## Recovered picture-resource path

The retail window-picture class does not use the ordinary geometry path from GMS
class data through a PRM primitive and its texture selector. It first loads its
base window-object state, reads a class-specific scalar picture asset reference
through the engine's tagged serializer, resolves that reference through the
texture-resource manager, and receives an ordered draw-group count plus draw-group records. A
descriptor index addresses a 40-byte record within that resolved resource.

The startup picture block has a bounded 24-bit byte size and a proven sequence of
four base scalars, an optional extension scalar, a structural delimiter, one
picture asset reference, another delimiter, and a terminal marker. The public GMS
model exposes an on-demand, fail-closed decoder for this exact path. It validates
the complete block, tag types, the shape-based optional branch selected exactly
when the next tag has the integer type, stream order, and exact end before
returning the scalar reference. Private corpus validation passes all 124 startup
picture sources.

The texture manager interprets that scalar as a byte displacement from a runtime
allocation base. Disassembly proves that this allocation begins with the raw PRM
member: the renderer allocates its byte size plus a private working reserve, then
reads exactly the raw PRM bytes at the allocation base without repacking them.
The reference is therefore a PRM-base-relative byte displacement, but it is not
a packed primitive index, TEX image ID, catalog ordinal, or raw TEX offset.

The returned resource has a descriptor count, 40-byte presentation descriptors,
a draw-group count, a parallel array of draw-group texture-resource references,
and eight-byte draw-group records. Each record contains a descriptor span count
and first descriptor index. The
renderer submits each texture-resource reference together with its corresponding
frame record; the descriptor supplies presentation geometry rather than texture
identity. The bounded [picture-resource parser](PICTURE_RESOURCE.md) models this
association without converting runtime indexes into pointers.

The final identity join is now recovered. Each frame reference resolves a typed
PRM record whose manager key selects one of 2,048 scene-local slots. The startup
path uses the upper key bank, normalized by subtracting 2,048; the resulting ID
must exist exactly once in the paired TEX catalog, and the reverse operation must
reproduce the original key. All 1,144 startup draw groups resolve to 334 distinct
images. The recovered consumer supplies selection zero, but startup contains no
texture sequences, so sequence redirection cannot be validated against this
corpus. The startup-scoped join therefore rejects sequence-bearing slots rather
than claiming behavior for other scenes.

This proves image identity but does not assign semantic menu roles. Role binding
still requires window hierarchy, state, frame, transform, and composition
evidence and may not use names, dimensions, catalog order, or pixel similarity.

Public unit tests supply explicit project-authored bindings and generated pixel
buffers. A retail smoke test must obtain bindings from the recovered picture
resource resolver and read the player's archive in memory; it must not write
decoded textures to disk or commit runtime evidence containing retail identifiers.
