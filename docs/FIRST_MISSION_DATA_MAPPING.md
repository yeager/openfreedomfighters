# First-mission data and routing boundary

Status: the owned-data corpus identifies a structurally valid candidate for the
first playable scene, but it does not establish that candidate's runtime route.
The native project must not create a player, load a scene, or start gameplay
from static package membership alone.

## Private evidence map

Private static review establishes these source-free facts:

| Boundary | Evidence | Native status |
|---|---|---|
| Startup menu | A menu-side component has a route toward a single-player selection and a retained start-zone choice. | Selection, scene-manager request, and failure behavior remain unimplemented. |
| Loading route | The executable contains a loader-oriented handoff and a scene-package naming convention. | The portable loader can retain validated packages but cannot select or activate one. |
| Candidate scene | One scene package family is referenced by the static campaign/start-zone material and has the complete archive, audio-bank, and loader shape. | Its identity remains private; static evidence is not a transition contract. |
| Scene contents | The candidate obeys the same validated resource-family rules as the corpus: object-source, spatial, primitive, texture, audio, support, and parameter resources are present. | No component schema, player binding, camera, physics, scripting, or objective semantics are inferred. |
| Gameplay arrival | Black-box observation identifies a third-person viewport, HUD regions, and a movement tutorial after the opening sequence. | This is insufficient to assign inputs, movement constants, collision, weapons, AI, or mission state. |

The source-free package facts are deliberately not a file-name inventory,
archive hash, object list, text extract, byte dump, address map, or screenshot.
They are enough to choose an observation target, not enough to author runtime
behavior.

## High-value next target

The next legal implementation target is the **observed handoff-to-visible-state
contract**, not a synthetic mission loader. It joins the already separate
startup-menu request and opening-cut boundaries to the first externally visible
gameplay state. It can establish, in order:

1. the visible handoff category;
2. one input probe;
3. one visible control/camera/player outcome; and
4. one reset or terminal outcome.

This is the smallest contract that can safely admit a future
presentation-independent input-intent boundary. It still cannot admit
locomotion, collision, combat, AI, or mission scripting.

## Private structural trace

`tools/first_mission_observation_trace.py` validates one private observation
record. It accepts only an opaque verified-data-manifest fingerprint, bounded
platform/input metadata, and ordered categorical observations. It rejects all
other fields, including identifiers, strings, paths, addresses, offsets,
symbols, executable material, screenshots, assets, payloads, and bytes. It
does not open game data or the executable.

The first record is always a launch handoff. Later records may use only the
documented probe classes and visible states from
[the first-mission observation contract](FIRST_MISSION_OBSERVATION_CONTRACT.md).
Both input and output must be outside this repository; output must be a new
file.

```sh
python3 tools/first_mission_observation_trace.py PRIVATE_INPUT.json PRIVATE_OUTPUT.json
```

Collect two clean baseline launches with the same private configuration and a
third launch with exactly one input experiment. Review repeat agreement before
adding any native behavior. The sanitized traces remain private; only an
authored behavior specification and source-free test fixture may enter the
repository.
