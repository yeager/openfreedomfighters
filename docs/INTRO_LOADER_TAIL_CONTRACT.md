# Repeat-gated loader-tail contract

`tools/intro_loader_tail_contract.py` turns three private sanitized loader-tail
traces into a small, source-free receipt. It is evidence tooling only: it does
not invoke the loader tail, create callbacks, or permit normal-startup
admission.

The gate requires two byte-identical complete successful traces and one
distinct terminal-failure trace from the same observer-local callback. The
failure trace must retain the successful trace's observed prefix order and may
not report success. Successful traces are revalidated as complete concrete
loader-tail traces, so a missing or unobserved service cannot be represented as
a completed contract.

The receipt contains only the format and categorical gate results. It retains
no game bytes, identities, paths, executable details, callback ordinals,
timing, text, images, or hashes.

Run only with newly created private files outside the checkout:

```sh
python3 tools/intro_loader_tail_contract.py SUCCESS_A.json SUCCESS_B.json FAILURE.json RECEIPT.json
```

Every path rejects parent traversal and symlink components. The receipt is
created once with private permissions; existing outputs are never overwritten.
Passing this gate supports review of the observed boundary only. It does not
authorize guessed callbacks, no-op services, or loader-tail integration.
