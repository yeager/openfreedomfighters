# Camera display tuple and renderer-list behavior

Behavior contract for an independent implementation. The camera tuple alone
is not proof of an admitted frame or a completed renderer pipeline.

## Renderer list and metadata lifecycle

Renderer instances form a doubly linked list. Construction inserts each new node at the head; the previous head becomes its next node. Destruction unlinks the node and repairs neighboring links or the head pointer. The head is therefore the most recently constructed renderer.

Each renderer node snapshots a boolean from the configuration object at construction. Startup sets that configuration boolean from the display dimensions using the signed-integer predicate 3 * pixelWidth > 4 * pixelHeight. For valid positive dimensions without overflow, this means a display aspect ratio greater than 4:3. The camera projection code later reads the head node's snapshot, independently of the authored renderer-list selector. This value is a display aspect predicate, not a backend capability and not the separate WideScreen setting. The configuration object's storage is not initialized by its constructor; the startup setter must run before renderer-node construction for a meaningful value.

Camera setup also uses an authored one-based renderer-list selector. Starting at the head, walk next links until the requested entry. Selector zero or an absent entry skips the camera-to-renderer callback. For a selected entry, invoke its callback with the camera and authored priority. This lookup does not choose which metadata byte controls projection. Camera color is sent through a separate renderer color callback.

## Mode dispatch and exact computations

The aspect-mode dispatch recognizes cases 1, 2, and 3. Other values do not take these mode-specific projection paths. Preserve upstream behavior for unrecognized values; do not guess a fallback mode.

The algorithm constants below are binary32 values. Keep the stated computation order and avoid algebraic rewrites when matching behavior.

### Mode 1

Start with e = 0.125.

- If the fullscreen/window-state boolean is false, keep e unchanged and do not read the renderer-head metadata.
- If it is true, replace e with 0.01 when the list is nonempty and the head metadata is true; otherwise retain 0.125.
- Still inside the true branch, evaluate aspect = float(pixelWidth) / float(pixelHeight), then scale = 1.3333334 / aspect, then e = e * scale.

Pass (0, e, 1, 1 - (e + e)) to the camera's four-float setter, in that order. After the setter, write camera marker value 4.

### Mode 2

Choose e as follows:

- If the list is nonempty and its head metadata is true, use 0.122 without querying the setting.
- Otherwise query WideScreen: true selects 0.122; false selects 0.216.

Pass (0, e, 1, 1 - (e + e)) to the setter, then write camera marker 4.

### Mode 3

Choose e as follows:

- If the list is nonempty and its head metadata is true, use 0.125 without querying the setting.
- Otherwise query WideScreen: true selects 0.125; false selects 0.

Pass (e, 0, 1 - (e + e), 1) to the setter. This branch does not write marker 4.

### Setter interpretation

The downstream pixel-viewport mapping is specified in `CAMERA_VIEWPORT_SPEC.md`.

The four inputs are stored as camera state and the setter also derives another field as input-four divided by input-three. Treat them as the native camera tuple, not as proven literal left/right/top/bottom clip planes. Mode 3 uses a distinct tuple layout and must not be normalized into the mode-1/2 form.

## Validation plan for an independent implementation

Use synthetic inputs and a mock renderer list/configuration provider; do not depend on retail-derived resource data.

- Verify head insertion, traversal from newest to oldest, unlinking head/middle/tail, selector zero, valid selectors, and out-of-range selectors. Confirm callback selection is independent of which node supplies projection metadata.
- Test the startup predicate on values below, equal to, and above a 4:3 display ratio. Ensure it is assigned before renderer-node metadata is snapshotted.
- Exercise mode 1 for fullscreen false/true; head absent/present; metadata false/true; several width/height ratios. Assert exact tuple values, evaluation order, and marker behavior.
- Exercise modes 2 and 3 across head absent/present and WideScreen true/false. Confirm the setting is queried only when the head condition is false, and check mode 3's distinct tuple and lack of marker write.
- Check unknown aspect-mode values cause no mode-specific setter or marker effect.
- Confirm the selected renderer receives the authored camera and priority only when its one-based entry exists; color propagation is separate.

## Host integration constraints

A renderer-list entry must belong to a real window/rendering backend lifetime.
Its camera callback updates that backend's camera state. The existing
priority-sorted camera registry is a different abstraction and must not be
used as the renderer list. Head metadata is a snapshot at node construction;
changing display dimensions later does not silently rewrite that snapshot.

The host must establish valid positive pixel dimensions and the startup aspect
predicate before creating entries. Reject invalid dimensions and values outside
the signed comparison's supported non-overflow range. This is an explicit host
validation policy, not a claim about invalid native inputs. Fullscreen and
WideScreen are distinct inputs. Obtain actual window dimensions/fullscreen state
from the window provider; supply the WideScreen setting explicitly.

Current CPU startup preflight precedes creation of the engine GPU device.
Implementation must address that ordering explicitly so a callback cannot claim
to bind an absent renderer. Tests of pure tuple computation or a mock callback
do not establish production renderer registration, camera activation, or playback.
