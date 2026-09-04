# Code-boundary census

`tools/code_census.py` performs a Capstone linear sweep over the PE32 `.text` section. It emits counts and API boundary names only: no instructions, operands, bytes, function bodies, or disassembly listings are written to its report.

## Supported-build measurements

| Measurement | Result |
|---|---:|
| Decoded instruction candidates | 856,087 |
| Undecodable/skipped bytes | 80 |
| Direct internal call sites | 30,080 |
| Unique direct internal targets | 4,375 |

The unique target count is a useful upper estimate for function discovery, not a proven function count. Linear sweep can decode embedded data and misses indirect-only functions. A recursive traversal seeded from the entry point, 341 exports, and relocation/IAT references is required before generating a lifting manifest.

## Dynamic platform boundaries

Six direct IAT references to `GetProcAddress` and two to `LoadLibraryA` were observed. Local callsite review validates these nearby names:

- `dbghelp.dll`: optional Windows crash/debug support; replace with native crash diagnostics or omit.
- `InitializeConditionVariable`, `SleepConditionVariableCS`, and `WakeAllConditionVariable`: runtime-selected synchronization APIs with an older-Windows fallback; replace with C++ synchronization primitives.
- `NotifyDestroy`: dynamically resolved engine/plugin interface function.

One `LoadLibraryA` call receives its module name from a runtime argument, so the renderer/plugin module selection must still be traced. One `GetProcAddress` wrapper likewise receives a runtime symbol argument. Nearby-string counts are hints; the names above were manually checked at the associated call boundaries before being promoted to this specification.

## High-value direct boundaries

The census sees one reference each to the Direct3D factory, DirectInput factory, DirectSound enumeration, EAX device creation, XInput state, and the three Steam lifecycle imports. The Winsock surface is small (one or two references per imported function). This supports isolated platform adapters rather than a broad Win32 emulation layer.

## Reproduce

```sh
python3 -m pip install -r requirements-analysis.txt
python3 tools/code_census.py /path/to/FreedomFighters/Freedom.Exe
```

Reports must be reviewed before publication. Do not add disassembly, original code bytes, or unrelated extracted strings.

