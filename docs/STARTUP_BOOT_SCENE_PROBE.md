# Synthetic startup boot-scene probe

`SyntheticStartupBootSceneProbeHost` is an isolated observation instrument.
It accepts only a checked `FF-StartUp` package and a directory source derived
from that package's exact parsed GMS object. It exercises the already bounded
BootMenu construction call sequence and returns a trace.

Every generated runtime value is explicitly named `synthetic_*`. The trace
also records the BootMenu owner's GMS directory ordinal, but that ordinal is
source evidence, not a runtime or allocator identity. The instrument is useful
for designing capture comparisons against the original executable: a future
observation can establish whether a real allocator or registry follows the
same call order and which values are externally visible.

The probe has no startup-loader integration and returns no controller token,
scene, registry, allocator, input target, renderer state, or transition. It
must not be included by normal startup code. It cannot establish allocator
pool size, object lifetime, destruction behavior, or real object identities.

## Command-line diagnostic

`openfreedomfighters --probe-startup-boot` runs the instrument against a
hash-verified owned installation. It is opt-in and does not initialize SDL,
open a window, start normal loading, construct a scene, or write game data.
Pass `--data PATH` when the installation is not in the default location:

```sh
./build/openfreedomfighters --probe-startup-boot --data /path/to/FreedomFighters
```

The command prints only a structural trace: the checked GMS BootMenu owner
directory ordinal, the call count, and each call's ordinal. It never prints
synthetic IDs, archive contents, strings from game data, or an allocator
identity. Runtime flags such as `--mode`, `--locale`, `--frame-limit`,
`--screenshot`, and `--diagnostic-scene` are rejected for this diagnostic.
