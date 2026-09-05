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
added only after the picture-resource and frame-descriptor join proves the exact
tile family, order, transforms, UVs, state selection, and compositing behavior.
Selecting one plausible-looking catalog image would not reproduce the game.

## Recovered picture-resource path

The retail window-picture class does not use the ordinary geometry path from GMS
class data through a PRM primitive and its texture selector. It first loads its
base window-object state, reads a class-specific scalar picture asset reference
through the engine's tagged serializer, resolves that reference through the
texture-resource manager, and receives a frame count plus frame descriptors. A
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
allocation base. It is not a PRM index, TEX image ID, catalog ordinal, or offset
from the raw TEX file. The returned resource has a descriptor count, 40-byte
descriptors, a frame count, a four-byte frame array, and eight-byte frame records;
each frame record contains a descriptor index. The producer of that runtime
allocation, the descriptor field that selects final image storage, the TEX-image
mapping, UVs, and compositing order remain unresolved. Candidate scalar values
must not be interpreted as retail texture identifiers.

Public unit tests supply explicit project-authored bindings and generated pixel
buffers. A retail smoke test must obtain bindings from the recovered picture
resource resolver and read the player's archive in memory; it must not write
decoded textures to disk or commit runtime evidence containing retail identifiers.
