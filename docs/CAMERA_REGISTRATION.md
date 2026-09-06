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

The complete DefaultCam child-attachment/resource-flag path, actual PreviewCamera
component enrollment, ordinary input dispatch, backend view creation and full
global component initialization remain unconnected. Known constructor flags are
not proof of a resource's flags after attachment to the current root. This work
does not skip those gates or signal that audio playback is ready.
