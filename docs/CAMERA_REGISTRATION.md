# Camera membership and audio listener

`IntroRuntime` connects its retained camera to a renderer-owned handle collection,
dimension notification and the application's sound listener selector. Registration
is explicit: loading camera data alone does not insert it or admit a render view.

The collection retains a last-inserted cursor and finite binary32 keys. Equal
keys use that cursor, not stable sorting; consecutive zero-key registrations are
newest first. Duplicate handles keep their existing key and cause no dimension
or view callbacks. Indexed lookup prunes stale owners in order, clearing the
cursor only when its own entry is removed.

New registration stores renderer width before querying/storing height. It then
checks the actual backend-ready service before admitting a view. These writes
affect the same camera used elsewhere in the scene; they do not enable it or
change its normalized viewport. A ready backend requires a real view-admission
implementation. An absent/unready backend retains camera membership without
pretending a view exists. Post-insertion failures preserve their prefix and
poison the native registry rather than allowing duplicate retries to hide failure.

The sound backend first resolves its explicit listener; otherwise it uses the
first renderer's pruned camera index zero. Disabled rendering does not exclude a
camera. A valid explicit setter clears listener offsets; invalid requests preserve
state. Camera room/context and resource parent remain separate. Null context uses
the same synthesized scene root, never an authored source-number substitute.

Native owner IDs now come from the application's non-reusing identity sequence.
Scenes retain their own source/hierarchy mapping, so an expired listener cannot
alias the same directory index in a later scene. This identity policy is separate
from the original component construction serial and is not its replacement.

`FreshIntroCamera` also has a genuine source-free constructor for synthesized
cameras. It retains the reviewed projection defaults and marks authored source
state absent; it does not fabricate a GMS record. Camera priority and renderer
registration key are separate values.

Tests cover cursor ordering, stale/duplicate handles, callback order/failures,
listener/context selection and cross-scene identity. A private owned-data probe
registered the actual intro camera with dimensions queried from a native SDL
window, then repeated the load to check stale-listener rejection. It did not
activate a cut or render a view. No game assets were saved or published.

## Still needed for normal startup

`IntroRuntime::ensure_default_camera` now performs the fallback when the caller
has established the root's actual resource state: query camera zero, create the
named DefaultCam/ZCAMERA child, attach it to the same hierarchy, set its loader
flag/transform, attach PreviewCamera, then set priority and register with key zero.
It uses application-allocated dynamic identities rather than extending a source
index range. Existing children retain their order. Resource context association,
parent and camera-owner room remain separate typed domains.

Fresh resource flags inherit only the reviewed root hide and context markers;
the transform queue sees the same dirty resource once. Unknown root state blocks
creation. `assign_resource_state` is a native publication boundary for an actual
producer, not the original flag setter and not permission to substitute authored
flags or a constructor constant. Failed construction keeps its completed prefix
and cannot be resumed as though it had succeeded.

The real Preview payload is admitted through the shared [ordinary component
manager](ORDINARY_COMPONENTS.md). Its resource view refers directly to the host
hierarchy, so keyboard/mouse writes, renderer traversal and listener selection
share the same owner and pose. The owned-data probe still rejects fallback while
the real root's post-load state is unavailable; it creates no substitute camera.

Normal startup still needs the preceding root/resource loader operations, the
remaining concrete component population, real platform input/frame dispatch and
backend view creation. Synthetic integration tests exercise the complete fallback
ordering and subsequent Preview callback but do not prove retail intro playback.
