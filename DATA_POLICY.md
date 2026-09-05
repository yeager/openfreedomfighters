# Game-data policy

The runtime must locate a user-owned PC installation and verify a supported manifest before starting. It may read assets directly or create a local derived cache. That cache stays on the user's machine and must not be uploaded.

## Retail-first runtime rule

When the supported installation contains data needed by a user-visible runtime
path, the engine must read that data from the installation. It must not replace
available retail fonts, interface art, strings, scene data, geometry, textures,
animation, or audio with invented placeholder content and then present the result
as compatible gameplay. Unsupported or undecoded retail data produces an explicit
diagnostic and a disabled/incomplete feature; it is not silently synthesized.

Project-authored synthetic data is limited to automated tests, fuzzing, security
regressions, and clearly labelled developer diagnostics that cannot be mistaken
for game content. Hosted CI necessarily uses such fixtures because retail data
must never be uploaded. Tests against a user-owned installation are separate,
local compatibility gates and must not emit retail-derived artifacts into the
source tree. Independently licensed optional Modern+ assets are not synthetic
fallbacks and remain subject to the replacement-asset contract.

The repository and release packages must never contain:

- files from the retail installation;
- extracted textures, geometry, animation, audio, video, fonts, or text;
- checks designed to bypass Steam or another ownership mechanism;
- proprietary DLLs.
- screenshots, frame captures, or thumbnails containing retail-derived imagery.

CI uses only project-authored synthetic fixtures. `.gitignore` blocks known retail extensions as a first guard; release CI will add content/signature and size checks before public binaries are produced.

Localization files contributed to the project must be newly authored translations of contributor-owned source text or distributed separately with documented permission. The initial 20-language framework does not authorize copying the game's existing strings.
