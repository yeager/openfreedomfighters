# Private first-mission evidence contract

`first_mission_observation_repeat_bundle.py` emits one private aggregate file.
After independent review, copy it outside the repository into a real local
directory as `reviewed-first-mission-evidence.json`. The native reader accepts
only that one regular, non-symlink file and rejects empty, oversized, malformed,
trailing, or schema-extended input.

The fixed v1 schema is the aggregate emitted by the private tool:

- a format and nonzero method version;
- an opaque verified-data-manifest fingerprint and declared platform metadata;
- the fixed 2 baseline / 1 experiment counts and bounded event counts;
- one reviewed non-idle input-probe category;
- one category-only visible outcome; and
- one later mission `loading`, `failed`, or `completed` category.

The loader validates all of those fields but retains only the probe, visible
boundary/state, and terminal-state categories. It never retains the fingerprint,
platform metadata, event counts, traces, paths, retail identifiers, source data,
or raw observer material. Loading a contract is not runtime wiring and does not
authorize a scene, player, camera, input mapping, or gameplay behavior.

Keep the original traces and reviewed aggregate private. The checked-in test
fixture is independently authored structural JSON only.
