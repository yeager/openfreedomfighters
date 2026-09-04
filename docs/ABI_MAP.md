# Windows ABI replacement map

The supported executable imports 15 functions by ordinal. All 15 are now resolved to interoperability names; no ordinal remains unknown.

| DLL | Ordinal | Name | Native replacement direction |
|---|---:|---|---|
| `EAX.DLL` | 6 | `EAXDirectSoundCreate8` | portable audio device/mixer; optional environmental effects |
| `DSOUND.dll` | 2 | `DirectSoundEnumerateA` | enumerate portable audio outputs |
| `OLEAUT32.dll` | 2 | `SysAllocString` | isolated COM compatibility helper or remove at clean boundary |
| `OLEAUT32.dll` | 6 | `SysFreeString` | isolated COM compatibility helper or remove at clean boundary |
| `WS2_32.dll` | 3 | `closesocket` | portable sockets |
| `WS2_32.dll` | 4 | `connect` | portable sockets |
| `WS2_32.dll` | 9 | `htons` | portable sockets |
| `WS2_32.dll` | 11 | `inet_addr` | portable address parsing |
| `WS2_32.dll` | 19 | `send` | portable sockets |
| `WS2_32.dll` | 23 | `socket` | portable sockets |
| `WS2_32.dll` | 51 | `gethostbyaddr` | portable name resolution; verify whether needed by gameplay |
| `WS2_32.dll` | 52 | `gethostbyname` | portable name resolution; verify whether needed by gameplay |
| `WS2_32.dll` | 57 | `gethostname` | portable platform identity or remove |
| `WS2_32.dll` | 115 | `WSAStartup` | no-op/init in socket adapter |
| `WS2_32.dll` | 116 | `WSACleanup` | no-op/shutdown in socket adapter |

The EAX mapping is read from the user-owned `EAX.DLL` export table by the local inspector. System mappings were cross-checked against Wine's primary ABI specifications: [ws2_32.spec](https://github.com/wine-mirror/wine/blob/master/dlls/ws2_32/ws2_32.spec), [dsound.spec](https://github.com/wine-mirror/wine/blob/master/dlls/dsound/dsound.spec), and [oleaut32.spec](https://github.com/wine-mirror/wine/blob/master/dlls/oleaut32/oleaut32.spec).

`tools/inspect_install.py` also records every import-address-table RVA. This permits a disassembly probe to count call sites and identify which compatibility paths are actually reachable, without storing or publishing instruction listings.

