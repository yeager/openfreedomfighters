# Picture draw order

`make_picture_order_key` joins current view and picture submission-control bytes
with the selected texture image ID. It accepts the same checked paired-resource
marker as the picture parser; unsupported marker mutations reject.

For that marker, category is 13 and subtype is 2. The key is:

```text
uint32(view << 27 | control << 19 | 13 << 15 | (image_id & 0x7ff) << 4 | 2)
```

Operands are converted to unsigned 32-bit words before shifting. High overflow
bits are discarded. Sorting uses the complete unsigned key, including bit 31;
the consumer's view-transition mask `0x78000000` is a separate operation. View
order 16 must not be rejected or silently clamped to 15.

The image field uses the selected TEX image ID, not catalog position, PRM offset,
resource pointer or decoded-pixel equality. The bound-resource adapter covers
validated static selections. Animated selection and source-header mutation need
their own runtime handling; missing selection must not become an invented ID.

## Rebuilt and retained entries

`merge_picture_draw_order` takes two explicit partitions: already-compacted active
retained entries with existing sorted keys, and eligible entries whose keys were
rebuilt. It does not infer those partitions from loaded source resources.

New entries are sorted by unsigned key. The original comparator returned a
negative result even for equal keys, violating the sorting API's contract.
OpenFreedomFighters uses stable input order for equal new keys as an explicit
native portability policy, not a claim about original equal-key order.

During merging, new entries precede retained entries on equal keys. Retained
relative order is preserved. Every output receives its resulting slot index;
no groups, records or equal-pixel textures are deduplicated. Unsorted retained
input rejects, and neither input partition is modified.

Fresh accepted picture records have a reviewed path from the embedded visitor
to the active rebuild queue. Reused-record maintenance, actual view association
and visibility remain caller responsibilities. Resource-preparation order is not
a substitute for this draw order. The subsequent binding transition is described
in [picture material requests](PICTURE_MATERIAL_STATE.md#resource-binding-requests).
The [ordered dispatch loop](PICTURE_ORDERED_DRAW_LOOP.md) consumes prepared
entries with their retained live associations; it does not reconstruct those
associations from sorted keys.

## Verification

Tests cover checked markers, current control/view bytes, unsigned overflow,
image-field aliases, bit-31 ordering, equal-key policy, retained/new merging,
slot assignment and invalid retained order. All 55 local CTest executables pass;
the ordering test also passes with GCC and ASan/UBSan.

The private probe checks all owned intro picture groups using decoded control
values and validated TEX bindings. Its single-view and fresh-record inputs are
explicit test conditions, not observed first-frame admission. The resulting
order differs from resource-preparation order without losing any group. No
retail resources or expected retail vectors are included in public tests.
