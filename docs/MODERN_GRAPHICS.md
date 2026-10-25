# Modern graphics specification

OpenFreedomFighters has an Original compatibility profile and a Modern profile. Rendering choices must not change simulation, input, saves, or replay results. Modern+ may later add separately distributed replacement assets under a distinct packaging policy.

Modern rendering should improve resolution handling, filtering, anti-aliasing, lighting, shadows, effects, and color output while preserving clear fallbacks. UI and subtitles should be composed at output resolution.

The current normal scene path is incomplete. It does not yet supply a complete live scene frame to a temporal resolver, so the runtime must not advertise temporal upscaling or DLSS/FSR/XeSS merely because a setting, library, or SDK is present. Vendor support requires real inputs, native submission, runtime/version checks, history invalidation, and image-quality validation. See [DLSS.md](DLSS.md), [FSR.md](FSR.md), and [XESS.md](XESS.md).

Any diagnostic rendering must remain labeled diagnostic. Do not publish retail assets, private captures, executable analysis, or source-derived corpus data.
