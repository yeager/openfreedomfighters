# 2003 retail build versus the digital Steam build

OpenFreedomFighters currently treats the digitally re-released Steam executable with SHA-256 `b05ca73c44474320e1b7321c24be66270f1de2a0686b063b6cb18e9ed21de9c9` as its canonical compatibility baseline.

## What is established

- Valve's store page says the game was made digitally available "in its original form" and lists only Windows 10, single-player, Steam Cloud, and English. It does not describe a remaster or enhanced gameplay release. See the [official Steam store page](https://store.steampowered.com/app/1347780/Freedom_Fighters/).
- The installed campaign archive members retain August 2003 timestamps and the original Glacier resource families.
- The game executable still imports `Direct3DCreate8`, `DirectInput8Create`, DirectSound/EAX, and the original-style Win32 window/input surface. The DirectX 10-compatible GPU requirement on Steam is therefore a host compatibility requirement, not evidence of a Direct3D 10 renderer.
- The digital executable was rebuilt: its PE timestamp is `2021-01-28 15:54:51 UTC`, its embedded build path identifies a Visual Studio 2015 Win32/Steam GameRelease configuration, and it imports the Steam lifecycle API. The installation also has a separate modern launcher and supports Steam Cloud.

## Working conclusion

The strongest evidence supports original 2003 game content and behavior packaged with a later Windows/Steam executable, launcher, save-path integration, and compatibility build. This is an inference from the official description plus local binary/resource evidence; it is not a byte-for-byte comparison.

## What remains unproven

Exact code, data, timing, bug-fix, and asset differences cannot be established without a lawfully acquired 2003 Windows installation. OpenFreedomFighters will not download or accept an unauthorized copy. If an original disc installation becomes available, the comparison procedure is:

1. hash both installations and record file-level additions/removals without publishing files;
2. compare archive member names, sizes, CRCs, timestamps, and format headers;
3. compare exported registries, imports, PE layout, and behavior traces;
4. classify Steam integration/launcher changes separately from engine or content changes;
5. preserve the 2020/2021 Steam build as the required public baseline unless another legally owned build is explicitly added to the compatibility manifest.
