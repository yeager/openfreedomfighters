# Game-data policy

The runtime must locate a user-owned PC installation and verify a supported manifest before starting. It may read assets directly or create a local derived cache. That cache stays on the user's machine and must not be uploaded.

The repository and release packages must never contain:

- files from the retail installation;
- extracted textures, geometry, animation, audio, video, fonts, or text;
- checks designed to bypass Steam or another ownership mechanism;
- proprietary DLLs.

CI uses only synthetic fixtures. `.gitignore` blocks known retail extensions as a first guard; release CI will add content/signature and size checks before public binaries are produced.

Localization files contributed to the project must be newly authored translations of contributor-owned source text or distributed separately with documented permission. The initial 20-language framework does not authorize copying the game's existing strings.

