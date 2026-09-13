# Private intro-camera lifecycle contract

`intro_camera_lifecycle_trace.py` accepts a record from an externally maintained,
fresh isolated observer. It does not inspect game data, executables, memory,
screenshots, logs, process state, or a target map.

The only retained facts are ordered categorical relations: sequence entry,
controller readiness, camera-owner resolution, transform composition, view
admission, frame delivery, and a terminal result. The input schema excludes
identities, paths, addresses, offsets, matrices, vectors, scalar values, timing,
images, text, bytes and asset references. Private input and output are bounded,
owner-only, no-follow files outside the repository.

`intro_camera_lifecycle_contract_bundle.py` accepts two identical successful
observations plus a separate failure observation. It emits an inert receipt. It
does not activate the intro, construct a projection, select a camera, or connect
to MovieControl. Those runtime connections remain fail-closed until the receipt
and their independent lifecycle contracts have been reviewed.

`intro_camera_observation_runner.py --execute` is the sole runner boundary. It
accepts only a supplied executable observer and a new private workspace, starts
it with `--mode fresh-isolated`, suppresses all observer streams, requires exactly
three bounded structural records, deletes them, and leaves only the receipt.
