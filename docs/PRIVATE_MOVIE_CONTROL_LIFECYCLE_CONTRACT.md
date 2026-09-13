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

## Installing a reviewed receipt

`tools/import_reviewed_movie_control_lifecycle_receipt.py` is the only
operator-side bridge from a final lifecycle bundle to the filename above. It
accepts only the final source-free bundle from
`movie_control_cutscene_lifecycle_contract_bundle.py`, repeats the complete
structural validation, rejects duplicate JSON fields and symlinks, and creates
one owner-only file without replacing an existing entry. It does not start or
attach to an observer or game process. Independent review is explicit:

```sh
python3 tools/import_reviewed_movie_control_lifecycle_receipt.py --reviewed \
  PRIVATE_LIFECYCLE_BUNDLE.json \
  PRIVATE_PREFERENCES/reviewed-movie-control-lifecycle/reviewed-movie-control-lifecycle.json
```

Installing this receipt proves only local structural admission. Normal intro
playback remains fail-closed until a separate reviewed behavior specification
is implemented and tested.
