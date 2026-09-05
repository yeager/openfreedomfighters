# Scene render asset

`SceneRenderAsset` is the first owning, renderer-facing representation of all
directly materializable geometry in a scene. It bridges the parsed `RMC`/`RMI`,
`GMS`, `PRM`, and `TEX` models without retaining spans or pointers into parser
storage.

## Construction contract

`build_scene_render_asset` accepts the paired scene primitive and texture
catalogs, the scene GMS object-source directory, and an ordered list of RMC/RMI
map views. It:

- visits map layers in caller order, map entries in source order, and each
  required primary reference before its present optional secondary reference;
- retains a resolution record for every handle, including unresolved and
  non-primitive outcomes;
- creates an instance only for a handle that resolves to a direct local ordinary
  PRM;
- deduplicates mesh storage by PRM catalog entry and texture storage by TEX
  catalog entry while never deduplicating instances;
- preserves RMC/RMI kind, layer, descriptor, role, handle, GMS source identity,
  topology, alpha evidence, and map/GMS transforms; and
- keeps the GMS and map transforms separate because their composition has not
  yet been established from evidence.

The asset includes line lists, untextured meshes, and transparent geometry. The
diagnostic single-mesh preview's selection rules do not apply to scene assets.
Textures are decoded to owned RGBA8 mip-zero images once per referenced TEX
entry. Mesh vertices, indexes, and draw ranges are copied into owned storage.

## Validation and limits

Construction reuses `RenderAssetBindings` for cross-catalog identity and topology
validation, then rejects invalid vertex indexes and non-finite vertex attributes,
source transforms, map transforms, or extents. Checked cumulative budgets cap a
scene at 16 map layers, 131,072 resolution records, 16,000,000 deduplicated
vertices, 32,000,000 deduplicated indexes, 4,000,000 deduplicated draw ranges,
and 1 GiB of decoded mip-zero RGBA data. These are defensive implementation
limits, not claims about the file format.

A read-only audit of all 90 supported scenes observed 220 directly
materializable instances in total and a per-scene maximum of 106. Independent
per-scene maxima were 97 deduplicated PRMs, two deduplicated textures, 1,923
vertices, 2,714 indexes, 97 primitive draw ranges, 106 instance-expanded draws,
and 262,144 decoded RGBA bytes. These figures exclude external, unresolved, and
local non-primitive references; independent maxima need not occur in one scene.

## Current boundary

This model prepares the complete set of currently understood direct local scene
primitives, but the SDL GPU runtime still draws a single diagnostic preview.
Uploading the scene asset, applying evidenced transform composition, reconstructing
materials, selecting the scene camera, adding depth, and resolving indirect or
non-primitive GMS sources are later milestones. Modern and Modern+ render paths
must consume the same instance identity and gameplay snapshot as Original mode.

