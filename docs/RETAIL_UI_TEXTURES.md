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
the recovered consumer associates each texture-resource reference with its
corresponding draw-group record; the descriptor supplies presentation geometry
rather than texture identity. The bounded
[picture-resource parser](PICTURE_RESOURCE.md) models this association without
converting runtime indexes into pointers. This association does not establish
GPU draw scheduling.

The identity join is now recovered. Each draw-group texture reference resolves a typed
PRM record whose manager key selects one of 2,048 scene-local slots. The startup
path uses the upper key bank, normalized by subtracting 2,048; the resulting ID
must exist exactly once in the paired TEX catalog, and the reverse operation must
reproduce the original key. All 1,144 startup draw groups resolve to 334 distinct
images. The recovered consumer supplies selection zero, but startup contains no
texture sequences, so sequence redirection cannot be validated against this
corpus. The startup-scoped join therefore rejects sequence-bearing slots rather
than claiming behavior for other scenes.

The startup graphics-row role is now bound by window hierarchy, authored local
coordinates, class nesting, and picture composition shape. Exactly one subtree
must match. It yields 24 picture instances, 88 one-quad groups, and six distinct
TEX images: eight persistent one-group backgrounds and sixteen five-group chrome
instances. The two chrome children carry the same authored exponent zero and
runtime mask `0x01`; the recovered state dispatcher therefore reveals or hides
them together. This is a visibility/co-gating fact, not a claim that the GPU
schedules both for drawing. The extractor retains inclusive root-to-instance
construction and transform chains without composing them. It does not label
chrome as focused/normal, distinguish
the two duplicate-Y rows, or map the same-anchor action controls. No role binding
uses names, image dimensions, catalog order, or pixel similarity.

This decoder is startup-scoped: callers must first establish that the GMS, raw
PRM allocation, and TEX catalog are paired members of the verified supported
startup archive. Locator X/Y values are compared as exact finite binary32
values. IEEE negative zero is equivalent to positive zero; an adjacent
`nextafter` value is a mismatch. Z is retained in each raw transform but is not
used for selection because no Z-role predicate has been established. Every
returned texture image index is an identity relative to the supplied paired TEX
catalog, not a global or cross-archive image identifier.

Public unit tests supply explicit project-authored bindings and generated pixel
buffers. A retail smoke test must obtain bindings from the recovered picture
resource resolver and read the player's archive in memory; it must not write
decoded textures to disk or commit runtime evidence containing retail identifiers.
The composition factory is tested separately with owned project-authored row
values and draw plans, including group shape, canonical signatures, construction
chains, slot identity, rejection cases, and independence from caller storage.
Malformed picture-resource parsing and texture-catalog joins remain covered by
their lower-layer unit tests; the composition tests do not duplicate those
parser and resolver contracts.

The renderer-neutral visibility evaluator preserves both duplicate-slot row
identities and every picture-instance identity. Initial state `0x01` yields
seven visible rows, each with its persistent background and both chrome
instances. The other duplicate-slot row is excluded by its authored hide flag.
Recovered states `0x08`, `0x10`, and `0x20` hide both chrome instances together
and retain the persistent backgrounds. A requested mask containing no recovered
allowed-state bit retains its requested value but evaluates child visibility
with the recovered `0x01` fallback. A mixed mask containing an allowed bit does
not fall back. The returned sequence is an identity-bearing visibility result,
not a claim about inter-picture, inter-row, container, or GPU submission order.
The hardcoded allowed-state mask is scoped only to this recovered startup
graphics control family and must not be reused as a general window-state mask.

## Owning startup graphics asset

After whole-install verification, the runtime can open the supported startup
archive once and build an owning `StartupGraphicsAsset`. The loader requires one
GMS, BUF, PRM, and TEX member, validates the BUF against the paired GMS, builds
the canonical startup row composition, and collects its six distinct
catalog-local image identities. It decodes mip zero for those six images only;
the other startup catalog images are not decoded by this boundary. The result
owns the composition and decoded RGBA bytes after the ZIP, packed resources,
raw PRM allocation, and TEX catalog have left scope.

Decoded startup graphics are capped at an aggregate 64 MiB. This is a
project-authored CPU/GPU upload policy, not a claim about a retail format or
corpus maximum. Size accumulation uses checked arithmetic before allocation.
The asset preserves both the catalog-local image index and normalized TEX ID,
but assigns no `RetailUiTextureRole` and makes no claim about transform
composition, draw order, chrome scheduling, focus states, or action behavior.

The SDL runtime now accepts this asset and validates the six unique catalog
indexes and TEX IDs again at the CPU/GPU boundary. It checks nonzero extents,
exact RGBA byte counts, 32-bit transfer sizes, and the aggregate budget before
creating six RGBA8 sampler textures and uploading them in one copy submission.
These textures are deliberately not bound or drawn. They are a data-to-GPU
startup gate only; visible use remains blocked on the virtual-window transform
producer and recovered submission order. Partial initialization and every
normal runtime exit release all created startup textures before device
destruction.
