# Intro loader-tail observation contract

`tools/intro_loader_tail_observation.py` reduces a future isolated dynamic
observation to a small, source-free lifecycle trace. It is not a parser, a
loader implementation, or admission to normal startup.

The raw private record contains ordered categorical states only. A successful
trace must show each concrete boundary required by `IntroOuterLoaderTailServices`:
named-global handling, renderer-payload consumption and allocation-state
restoration, typed associations, source-lease release, either camera-zero or
fallback-camera routing, all three scene operations in order, spatial admission,
and the saved-resource service. An omitted source section is represented by an
explicit `not_present` state; an unobserved service is never treated as absent.

The sanitizer rejects reordered stages, callback changes, incomplete successful
records, unknown fields, and free-form material. It stores no game bytes,
identities, paths, executable details, timing, text, images, or hashes.

Run it only with a newly created private input and output outside the checkout:

```sh
python3 tools/intro_loader_tail_observation.py PRIVATE_RAW.json PRIVATE_TRACE.json
```

Keep both files private. A sanitized success trace records evidence for review;
it does not authorize substituting no-op callbacks or guessed service behavior.
