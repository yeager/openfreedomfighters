# Startup coordinator pass-selection observation

Status: private evidence required. `StartupActivePassSnapshotProvider` accepts
one already-selected startup root, enabled camera/view and renderer pass
context. It does not establish the original coordinator that selects them.
Normal startup must remain disconnected from this boundary until that producer
has a reviewed contract.

## Private source-free trace

`tools/startup_coordinator_pass_selection_trace.py` sanitizes a private
observer record into categorical coordinator-entry, root-selection,
camera/view, pass-context and delivery states. The only retained numerics are
local event order and one consistent bounded observer-local callback ordinal.
The schema admits no object or
source identities, names, text, paths, addresses, offsets, bytes, assets,
screenshots or timing values. The utility does not open an installation,
executable, archive or dump.

A successful completion requires all of the following in one trace:

1. entered coordinator;
2. selected root;
3. enabled camera/view;
4. resolved pass context; and
5. delivered selected pass.

Failure is recorded only when one of those structural boundaries explicitly
fails. Selection, camera, pass and delivery dependencies are checked in order;
the sanitizer rejects a claimed later result without its required predecessor.

Run it only on an isolated owned-install observer host, keeping both input and
output outside the checkout:

```sh
python3 tools/startup_coordinator_pass_selection_trace.py PRIVATE_RAW.json PRIVATE_CLEAN.json
```

## Repeat gate

Two independent fresh-process sanitized observations are required. Pass them to
`tools/startup_coordinator_pass_selection_repeat_pair.py`; it revalidates the
strict schema, requires byte-for-byte structural equality and one terminal
completion or failure, and writes a new private record without accepting raw
observer output. Both tools require existing non-symlink private parent
directories, read bounded regular inputs using a no-follow descriptor, and
create a new owner-only (`0600`) output with exclusive creation; they never
create parent directories or overwrite an existing record.

```sh
python3 tools/startup_coordinator_pass_selection_repeat_pair.py \
  PRIVATE_CLEAN_A.json PRIVATE_CLEAN_B.json PRIVATE_PAIR.json
```

The repeat pair is evidence for review only. It creates no runtime wiring,
does not select a menu or intro scene, and does not authorize presentation.
