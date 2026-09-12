# Startup-menu scene-request recovery boundary

Status: unsupported. The reviewed startup-menu path can select an opaque
runtime target, update the active startup window, and reach a menu receiver.
It does not establish a scene-manager request, a package target, a mission, or
gameplay construction. The portable runtime must not turn a menu action into a
scene or mission request on that evidence.

## Current source-backed boundary

The startup factory now has a scene-owned registry that materializes the
checked `FF-StartUp` hierarchy with fresh native handles, exact parent/child
links and the verified BootMenu attachment owner. It retains no selected root,
node family, visibility, input, camera, draw record or transition state. The
registry therefore closes the previous synthetic-only construction gap without
authorizing a live menu, reader/initialization pass, or scene-manager request.

## Missing contract

Private observation must establish, for the same selected-menu route:

1. selection delivery and its reader/owner/component preconditions;
2. delivery of that selection to the active-window update and its result;
3. a receiver-to-manager edge, rather than merely a menu transition;
4. clear/request order, retained-target result, and all failure outcomes; and
5. package-admission timing and validation, if a package is actually prepared.

No object identity, name, source record, target text, package path, archive
membership, executable material, address, offset, screenshot, asset, timing
value, or gameplay payload is needed in the public contract.

## Private structural trace utility

`tools/menu_scene_request_trace.py` accepts only local event order/callback
ordinals, construction and reader receipts, 32-bit status masks, categorical
selection/window delivery/receiver-to-manager/manager/target/package states,
outcome, and external service entry. A v2 trace makes the active-window
selection delivery explicit and refuses a manager request unless it also records
the corresponding receiver-to-manager edge. It rejects all unrecognised fields
and refuses a package result unless the trace first proves that edge with a
validated retained target.

The utility does not open or instrument an original executable or game data.
Its input and output must remain outside this repository, and it refuses to
overwrite a previous private output.

```sh
python3 tools/menu_scene_request_trace.py PRIVATE_INPUT.json PRIVATE_OUTPUT.json
```

Only a reviewed source-free result that closes this chain can authorize a
source-backed package-admission boundary. It would still not by itself enable
normal startup, construct a live scene, or establish a playable mission.
