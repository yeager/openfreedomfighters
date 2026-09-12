# Startup-menu scene-request recovery boundary

Status: unsupported. The reviewed startup-menu path can select an opaque
runtime target, update the active startup window, and reach a menu receiver.
It does not establish a scene-manager request, a package target, a mission, or
gameplay construction. The portable runtime must not turn a menu action into a
scene or mission request on that evidence.

## Missing contract

Private observation must establish, for the same selected-menu route:

1. selection delivery and its reader/owner/component preconditions;
2. active-window result relative to the eventual receiver;
3. whether the receiver enters a scene-manager route rather than another menu
   transition;
4. clear/request order, retained-target result, and all failure outcomes; and
5. package-admission timing and validation, if a package is actually prepared.

No object identity, name, source record, target text, package path, archive
membership, executable material, address, offset, screenshot, asset, timing
value, or gameplay payload is needed in the public contract.

## Private structural trace utility

`tools/menu_scene_request_trace.py` accepts only local event order/callback
ordinals, construction and reader receipts, 32-bit status masks, categorical
selection/window/receiver/manager/target/package states, outcome, and external
service entry. It rejects all unrecognised fields and refuses a package result
unless the trace first proves a receiver-to-manager request with a validated
retained target.

The utility does not open or instrument an original executable or game data.
Its input and output must remain outside this repository, and it refuses to
overwrite a previous private output.

```sh
python3 tools/menu_scene_request_trace.py PRIVATE_INPUT.json PRIVATE_OUTPUT.json
```

Only a reviewed source-free result that closes this chain can authorize a
source-backed package-admission boundary. It would still not by itself enable
normal startup, construct a live scene, or establish a playable mission.
