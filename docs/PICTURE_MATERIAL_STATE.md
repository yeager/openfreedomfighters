# Conditional picture material requests

This renderer-neutral model describes state requests from the recovered
picture-resource binding and descriptor-emitter material paths. It does not
choose the final startup material word, execute GPU calls or evaluate pixels.
The [single-stage shader](PICTURE_STAGE_SHADER.md) now evaluates supported
resolved stage operations on the GPU; it is not a complete material executor.
Resource binding and material requests remain separate ordered phases: the
latter can override the former. Missing requests mean leave inherited state
unchanged, never select an implicit default.

## Base-picture property expansion

The base picture expands unsigned authored properties as follows:

| Property | Material word |
| --- | --- |
| 0 | 0x60010 |
| 1 | 0x60012 |
| 2 | 0x60014 |
| 3 | 0x60011 |
| 4 | 0x60018 |
| 5 | 0x60210 |
| 6 | 0x60211 |

Values at least 7 pass through unchanged. The alternate setter mode ORs bit
0x1 into the result; standard loading uses no additional override. This
base-class mapping does not use authored alpha.

Loading writes the expanded word into existing paired resource records, not
per-picture clones. Shared runtime identities observe the last write. The pure
mapping therefore does not establish final draw-time material state: later
refreshes, overrides, aliases and total write order remain separate inputs.

The base-picture alpha setter has a separately modeled material-bit transition:
an incoming integer exactly equal to 255 clears bit 0x1; every other integer
sets it. The comparison uses the complete input, so 511 is not equivalent to
255 despite sharing the low byte. `update_picture_alpha_material` returns the
updated word and whether it changed, preserving all other bits. It does not
model alpha/color storage, resource propagation or when the setter is invoked.

## Resource-binding requests

Effective features are `(~(disable_mask_a | disable_mask_b)) & 3`.
Only an explicit resource transition requests this binding phase:

| Features | Requests |
| --- | --- |
| 0 or 2 | None |
| 1 | Bind texture; RGB and alpha select texture |
| 3 | Bind texture; RGB and alpha use doubled texture/diffuse modulation |

No request binds a fallback texture. Texture identity is supplied by the
separate resource plan. When both features are enabled, the doubled modulation
explains the geometry stage's channel reduction: its ideal component multiplier
is `2*floor(authored_channel/2)/255`. For 255 this is 254/255, not exact unity.
The operation semantics are described by
[Microsoft's texture-operation reference](https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dtextureop);
that reference does not establish the game's runtime feature selection.

For the concrete ordinary draw loop, the outer binding decision compares packed
ordering-key bits `0x7ff0`, starting from an all-ones previous key on each loop
invocation. A changed field only calls resource binding when the entry has a
resource; that resource must also have a backing texture selection before the
binding helper is called. No selection leaves the previous binding unchanged.
The model's explicit transition input represents that admitted helper call, not
resource-pointer or catalog-index inequality. Complete key production, view and
subtype transitions remain outside this material helper.

## Material requests and cache

The caller supplies the resolved runtime material word. The picture path uses
secondary word `0xffffffff` and alpha threshold zero. Nonzero suppression or
an exact cached triple produces no material requests and leaves the cache
unchanged. Otherwise the cache is replaced, even when effective features are
zero. Features are not part of the cache key.
The active special-material branch subsequently changes the cached material
word to `0xffffffff`, retaining the secondary word and threshold. The returned
cache replacement records that post-request value, not merely the input triple.

The concrete ordinary pass resets its material triple to `(0xffffffff, 0, 0)`
on every loop invocation, including an empty loop or a resume after a barrier.
It does not reset between groups. This initial secondary word ensures that the
first unsuppressed picture request misses even if its material word is all ones.
The entry operation also resets tracked backend state; the optional material
requests here are not a representation of that complete reset.

With features zero, only mode selector 1 requests texture factor `0xffffffff`;
other selectors request nothing. With any nonzero features:

- Blending is enabled exactly when `material & 0x402607` is nonzero. When
  disabled, blend factors are not rewritten.
- Ordinary enabled blending requests source alpha and inverse source alpha,
  except bit 0x2 with bit 0x400 clear requests destination factor one.
- Bit 0x2000 requests the special override: source zero, destination source
  color, RGB texture-plus-diffuse addition and disabled alpha operation.
  These are requests only, not a defined final pixel equation.
- Alpha testing is disabled for this zero-threshold picture path.
- Fog color uses the current renderer's additive color when bit `0x2` is set,
  otherwise its special color for bit `0x2000`, otherwise its base color.
  This selection also runs after the special cache-word replacement. An
  unblended material selects base color. A request is emitted only when the
  selected packed word differs from the renderer's tracked fog color.
- Bit 0x40000 disables depth writes; otherwise they are enabled. Bit 0x20000
  chooses always depth comparison; otherwise less-or-equal.
- Bit 0x80000 disables culling; otherwise clockwise culling is requested.
- Bits 0x4000 and 0x8000 independently select U and V clamp when set, wrap
  when clear.

These material requests can occur with feature 2 even though texture binding
was omitted. Suppression/cache hits do not undo preceding resource binding.
Blend operation, depth-test enable, filtering, later texture stages, clipping,
projection and output transfer are not established here and are not defaulted.

## Fog context and ordering

`resolve_picture_material_state` requires `PictureRendererFogState`: current
base, additive, special and tracked color words. None is a per-material default.
The optional fog-color request follows blend/stage/alpha handling and precedes
depth, culling and addressing. Applying it replaces the tracked word before
submitting the backend color. A packed zero request is distinct from no request.
Suppression, a cache hit or zero effective features skip fog entirely.

Tracked fog color is not necessarily actual GPU fog color. The separate fog
configuration setter submits its opaque base color without updating that tracked
word. Collapsing them into one effective-state field would change the original
comparison behavior. Likewise, a camera disabling fog does not erase the
material's fog-color requests.

The normal loop's reset initializes base/additive/special/tracked colors to
`0`, `0xff000000`, `0xffffffff`, `0` and submits fog color zero. Those values are
established at that reset boundary, not inferred for arbitrary material calls.
Fog enable and vertex/table modes are separate states; this request model does
not select their final values.

## Remaining integration evidence

Final startup pass masks, later material changes and alias/write order still
need proof. Neither this model nor the authored record copies select those
values. Original rendering remains gated until the full input and raster
contract is established.
The required private observation is documented in
[Original startup state observation](STARTUP_STATE_CAPTURE.md).

## Validation

Unit tests cover the complete small property table, unsigned passthrough and
bitwise override, all four effective feature combinations, ordered suppression
and cache guards, special sentinel hits, zero-feature cache replacement,
blend selection, and depth/cull/address requests. The optional local startup
asset test checks the observed authored property domain and its load-time
mapping against the user's data, without treating it as final resource state.

Fog tests cover all feature combinations, additive-over-special precedence,
suppression, cache hits, packed-zero requests and sequential tracked-color
updates. The private probe uses real fade material/color updates with an
explicit conditional renderer context; it does not claim observed first-frame
fog history. All 54 local CTest executables pass with this correction.
