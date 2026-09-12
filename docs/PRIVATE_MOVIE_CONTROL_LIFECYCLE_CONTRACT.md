# Private MovieControl lifecycle contract

The private lifecycle review tool produces the source-free JSON record
`off.movie-control-cutscene-lifecycle-contract-bundle/v1`. After an independent
review, an operator may place that one record at
`reviewed-movie-control-lifecycle/reviewed-movie-control-lifecycle.json` below
the application preferences directory.

`ReviewedMovieControlLifecycleContract::load_local` reads only that direct,
regular, non-symlink child. It accepts a bounded file with the exact v1 schema:
one completed phase-one route, one matching failed phase-one route, one
completed synchronous dispatch, one matching failed dispatch, four ordered
player lifecycle phases, and one matching failed activation. It rejects extra
fields, duplicate keys, unsupported JSON features, mismatched callback
ordinals, malformed categories, trailing data, symlinks, and oversized files.

The admitted object is intentionally inert. It retains no observer identity or
retail content and is not passed to MovieControl, the cutscene player, a scene,
a camera, a renderer, or normal startup. A separate reviewed native behavior
specification remains required before any runtime connection is made.
