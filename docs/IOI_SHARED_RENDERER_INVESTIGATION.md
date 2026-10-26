# IO Interactive shared renderer investigation

## Question

Can public information about *Hitman 2: Silent Assassin* and the local
*Freedom Fighters* installation help identify a shared IO Interactive
renderer component (including a file referred to as `RenderD3D.dll`), and can
that information support the cross-platform renderer work in this repository?

## Current findings

- Public references supplied for this investigation point to community fixes
  and a compatibility page for *Hitman 2: Silent Assassin*. They make the
  renderer lead worth checking, but do not establish that *Freedom Fighters*
  ships the same DLL, that the games use identical versions, or that any file
  can be substituted between them.
- The new Steam setup guide clarifies the `RenderOpenGL.dll` instruction: it
  is about the `DrawDll` setting in *Hitman 2: Silent Assassin*'s
  `Hitman2.ini`, which the guide says should be `RenderD3D.dll`. It is not an
  instruction to rename an OpenGL binary, and it does not establish that this
  setting or either renderer file belongs to *Freedom Fighters*.
- The public [Hitman: Blood Money configuration reference](https://github.com/HHCHunter/Hitman-BloodMoney/blob/master/docs/config-reference.md)
  documents `DrawDll` under `[GRAPHICS OPTIONS]`, with `RenderD3D.dll` as the
  renderer value. Its README describes the repository as tools and
  documentation for *Blood Money* and its Glacier engine; the reference also
  says its example config targets one specific 4K/debug scenario. This is
  useful evidence that a similarly named renderer-selection setting exists in
  that title, but it does not establish the setting, DLL, ABI, or behavior in
  *Freedom Fighters*. No configuration or code has been copied from it.
- The supplied Steam discussion URL is under app ID `247430`, which is
  *Hitman: Contracts*. Its [discussion thread](https://steamcommunity.com/app/247430/discussions/0/2282708683263007436/)
  describes `HitmanContracts.ini`, selecting `RenderD3D.dll` instead of
  `RenderOpenGL.dll`, and a community workaround using a D3D8-to-D3D9 wrapper
  for a reported laptop-GPU/performance issue. The thread also contains
  conflicting user reports about wrapper and widescreen-fix interaction. This
  is title-specific troubleshooting, not a verified renderer ABI or a
  Freedom Fighters fix. Keep it separate from the earlier Hitman 2: Silent
  Assassin guidance, which concerns `Hitman2.ini` and DxWrapper.
- The supplied [Microsoft download](https://www.microsoft.com/en-us/download/details.aspx?id=8109)
  is the June 2010 DirectX End-User Runtimes package, not the DirectX SDK and
  not a D3D8-to-D3D9 wrapper. Microsoft describes it as installing optional
  legacy side-by-side components such as D3DX, XAudio 2.7, XInput 1.3, XACT,
  and Managed DirectX; it does not replace or upgrade the Windows DirectX
  runtime. This may address a missing legacy dependency on Windows, but it
  does not establish or repair the game's renderer selection, and it has no
  role in the repository's SDL/Vulkan cross-platform renderer. No package was
  downloaded or installed.
- The same guide describes startup/cutscene hangs with a black screen as a
  known *Hitman 2* issue and recommends DxWrapper. Its stated setup places
  `d3d8.dll`, `dxwrapper.dll`, and `dxwrapper.ini` beside `hitman2.exe`, with
  Direct3D 8-to-9 translation enabled. This is community troubleshooting
  guidance for *Hitman 2*; it does not diagnose a *Freedom Fighters* launch or
  confirm whether DxWrapper is installed correctly on another machine.
- The additional [`0danny/re47` project](https://github.com/0danny/re47) is a
  decompilation of *Hitman: Codename 47* (2000), a different title. Its own
  status page marks `renderd3d.dll` as untouched and the project as no longer
  worked on. It therefore provides no D3D-renderer implementation or evidence
  for *Freedom Fighters* parity; at most, its public build/research notes are
  general context. Do not import its code or binary-derived findings into this
  project.
- The locally available *Freedom Fighters* Steam directory was inspected by
  filename. Its top-level executable/library files include `Freedom.Exe`,
  `Launcher.exe`, `steam_api.dll`, and `eax.dll`. No `RenderD3D.dll` or
  similarly named D3D renderer DLL was found in that installation or elsewhere
  under the local Steam directory at the time of inspection.
- This is a filename-level observation only. No binary contents, exports,
  hashes, or ABI compatibility were compared. It does not prove that the
  renderer is absent under another name, embedded in another module, or shared
  in code between the two games.
- A matching name or shared publisher/developer lineage would not by itself
  establish compatible interfaces. Any future comparison should use legally
  obtained local files and private analysis, and report only a concise,
  non-sensitive conclusion here.

## Relevance to this project

The repository's renderer direction is a native cross-platform runtime with
optional graphics features selected through explicit backend capabilities.
The existing DLSS/FSR and modern graphics documents remain the project
requirements. A legacy D3D component from another title would be useful only if
it provides independently verified behavioral or interface evidence; it is
not a runtime dependency for macOS, Linux, Windows, or Steam Deck.

The current normal-scene investigation also has a separate unresolved boundary:
the candidate active-view path has not been joined to a visible device draw.
This Hitman lead does not close that gap or establish scene-rendering parity.

## Evidence and limits

Evidence recorded for this note:

1. User-provided links to Hitman 2: Silent Assassin compatibility/setup
   material and the *Hitman: Contracts* discussion; the latter's app ID and
   title-specific INI/wrapper advice were checked against the public Steam
   thread. Also checked the public [`0danny/re47` README](https://github.com/0danny/re47),
   which identifies the project as *Hitman: Codename 47* and reports its D3D
   renderer as untouched.
2. The public *Blood Money* [configuration reference](https://github.com/HHCHunter/Hitman-BloodMoney/blob/master/docs/config-reference.md)
   and [repository license](https://github.com/HHCHunter/Hitman-BloodMoney/blob/master/LICENSE).
   The license scopes MIT terms to authored documentation/configuration and
   explicitly excludes game assets. This project records only the setting
   name/value and the cross-title limitation; it does not reproduce the
   reference's configuration contents.
3. The official Microsoft DirectX End-User Runtimes [download page](https://www.microsoft.com/en-us/download/details.aspx?id=8109),
   which identifies this as the June 2010 optional legacy-runtime package.
4. A read-only filename inventory of `/Users/bosse/steam/Freedom_Fighters`
   and a search for `RenderD3D.dll` under `/Users/bosse/steam`.

No Hitman executable or DLL was present in the inspected local Steam tree, so
there is no local cross-title file comparison to report. No public web claim
about identical binaries is made here.

## Next useful evidence

1. Obtain a local, legally accessible copy of the relevant Hitman module if
   cross-title file comparison is still useful.
2. Compare module metadata and exported/imported interfaces locally, keeping
   binaries, disassembly, decompilation, raw tool output, and file fingerprints
   out of this repository.
3. Record only whether the evidence supports a shared component or compatible
   interface, together with the scope and confidence of that conclusion.
4. Continue the Freedom Fighters native-renderer trace independently; do not
   infer camera, scene, or draw behavior from a Hitman compatibility fix.

## Publication boundary

This document intentionally contains no retail payload, binary-derived
layouts, addresses, offsets, disassembly, decompilation, screenshots, hashes,
or raw analysis logs. Private executable analysis remains outside Git as
required by [CLEAN_ROOM.md](../CLEAN_ROOM.md).
