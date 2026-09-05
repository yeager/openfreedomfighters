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

`load_startup_scene_render_asset` applies this contract to the startup scene. It
requires the paired PRM, TEX, and GMS resources plus both RMC and RMI layers,
preserves RMC before RMI in the map-layer order, and returns an asset with no
references into archive or parser storage.

## Validation and limits

Construction reuses `RenderAssetBindings` for cross-catalog identity and topology
validation, then rejects invalid vertex indexes and non-finite vertex attributes,
source transforms, map transforms, or extents. Checked cumulative budgets cap a
scene at 16 map layers, 131,072 resolution records, 16,000,000 deduplicated
vertices, 32,000,000 deduplicated indexes, 4,000,000 deduplicated draw ranges,
and 1 GiB of decoded mip-zero RGBA data. These are defensive implementation
limits, not claims about the file format.

`validate_scene_render_asset` independently repeats the GPU-boundary invariants
for an already owned asset. It checks all mesh, texture, resolution, and instance
references; exact RGBA8 storage; finite attributes and transforms; topology-aware
contiguous draw ranges; vertex indexes; provenance links; and the same cumulative
budgets. Upload code must call this validator even when the asset originally came
from the builder, because future cache and tooling paths may construct or mutate
the public aggregate directly.

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
The current source-only diagnostic transform is a presentation convention, not a
recovered world-space formula. An aggregate audit found that only four of 2,801
map positions fall inside their associated decoded spatial bounds under the
currently documented quantization inverse, so naively treating those fields as
world-space placement is specifically unsupported. The exact proven
relationships, prohibited interpretations, and evidence gates are documented in
[TRANSFORM_BOUNDARY.md](TRANSFORM_BOUNDARY.md).

## SDL-free GPU plan

`prepare_scene_gpu_plan` converts a validated asset into a deterministic command
schedule without depending on SDL or a graphics device. It computes one global
fit over every indexed vertex of every materialized instance, owns normalized GPU
vertex data and one copy of each deduplicated mesh/texture resource, and emits one
command per preserved draw range. Opaque
commands remain in stable instance order and use depth test/write; non-opaque
commands follow in stable order with blending and depth test but no depth writes.
Untextured commands retain an empty texture index so the backend can bind one
explicit shared white fallback. Every GPU instance also retains its expected
mesh index, and independent validation rejects draws that pair an otherwise
valid instance with the wrong mesh resource.

The diagnostic projection preserves a real third coordinate in SDL GPU's
`[0, 1]` clip-depth convention rather than flattening every object to one plane.
It reserves `[0.05, 0.95]` for geometry and handles planar depth with a stable
midpoint. Physical viewport dimensions produce an aspect-correct uniform without
rebuilding geometry resources, and zero-sized viewports are rejected. The fit
uses only the explicitly named source-only diagnostic transform;
RMC/RMI transform values cannot influence it. This CPU-only boundary makes global
bounds, instance ordering, topology, alpha/depth policy, and eventual resize-aware
uniform generation testable identically on Linux, macOS, and Windows.
