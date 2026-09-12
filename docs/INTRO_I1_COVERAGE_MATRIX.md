# I1 intro coverage matrix

Status: source-free implementation frontier. This matrix is the accounting
baseline for I1 in the [intro and main-menu plan](INTRO_AND_MENU_PLAN.md). It
does not establish playback or substitute for the lifecycle contracts.

The population is constructed from the verified, user-owned intro package.
The probe reports only totals and completion states. It never writes source
identities, filenames, paths, offsets, handles, payload bytes, extracted text,
or decoded values.

| Required population | Current covered effects | What coverage means | I1 exit condition |
| --- | ---: | --- | --- |
| Deferred readers (420) | 104 | A bounded reader completed and registered its exact live admission identity. | Every required reader is implemented, invoked once in the production reader bracket, and admitted. |
| Authored components (383) | 0 | A constructed non-synthetic component completed its required global lifecycle effect. | Every required component has its concrete live service and ordered lifecycle effect. |
| Owners, including root (471) | 0 | An owner completed its required owner-level lifecycle effect. | Every required owner has completed its ordered owner hook and remains valid for ordinary update. |

The covered values are deliberately not inferred from construction. A reader
receipt, an allocated owner, an attached component, parsed data, or a retained
source lease does not count as lifecycle coverage. The lifecycle gate remains
fail-closed at the first incomplete class; currently that is reader coverage.

## Reproduce against owned data

Run the cold probe from a build of the current source:

```sh
openfreedomfighters --probe-first-cut-cold --data <owned-data-root>
```

It prints these four aggregate records after the reader-family summary:

```text
lifecycle-coverage=readers required=<count> covered=<count> uncovered=<count>
lifecycle-coverage=components required=<count> covered=<count> uncovered=<count>
lifecycle-coverage=owners required=<count> covered=<count> uncovered=<count>
lifecycle-coverage-status=<state>
```

The command opens no window and does not start audio, schedule events, execute
global lifecycle phases, render a cut, or write game data. The output is an
audit of the implementation boundary, not a completion percentage. Re-run it
after every reader or lifecycle implementation; do not update this document by
copying identities or source-derived content into the repository.

## Use of the matrix

Close the reader row before treating component or owner work as a normal-path
success. A later row may receive isolated unit coverage, but an I1-ready scene
requires all three populations to be complete in one production-owned session.
The probe's status makes that ordering explicit: `reader-coverage`, then
`component-coverage`, then `owner-coverage`, and only then `complete`.
