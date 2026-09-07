# Intro lifecycle recovery inventory

This is the implementation frontier for the supported, user-owned intro. It
records recovered behavior only; it does not make a component runnable. Global
lifecycle remains blocked until every eligible component has its real reader,
callback, owner state, and required services.

| Factory group | Phase-one recovery | Phase-two recovery | Current blocker |
| --- | --- | --- | --- |
| `ZGROUP_RootGroup` | Initializer and phase-one seam | None | Ordinary input and phase-two behavior |
| `ZGEOM_MovieControl` | None | Callback ordering | Full global admission, live services, event 16 and cut dependencies |
| `ZGEOM_Center` | Position/cache seam | None | Reader-backed picture owner and position service |
| `ZWINPIC_FadeToBlack` | Fade and dimension seams | None | Reader, material/color path and event delivery |
| `ZGEOM_MatPosAnim` | KEYS preparation seam | None | Existing-state provider, events and special-owner behavior |
| `ZLIST_CutSequence`, `ZLIST_CutSequenceList`, `ZLIST_CutSequenceCommand` | Timeline/source sub-boundaries | None | Player registration, dispatch and completion timing |
| `ZSNDOBJ_SoundExtend`, `ZSNDOBJ_SoundNotify`, `ZSNDOBJ_SoundSegment`, `ZGEOM_ZSetZDefine` | Typed data and owner-side sub-boundaries | None | Output/channel admission and readiness producer |
| `ZCHAROBJ_CharFader`, `ZWINPIC_LogoFade` | None | None | Reader and event/mode behavior |
| `ZLIST_ExternCutSequenceCommand`, `ZSTDOBJ_VertAnim` | None | None | Reader, update and dispatch behavior |
| `ZGEOM_FilmGrainCamSetup`, `ZWINDOW_LensFlareControl`, `ZWINPIC_LensFlare`, `ZGEOM_ParamAnim`, `ZGEOM_ParticleEmitter`, `ZLIST_LensFlareLights`, `ZSTDOBJ_ScrollTexture` | Constructor-only state | None | Complete reader and runtime callbacks |

All 22 authored factory types are constructed and catalogued. Their generic
lifecycle callbacks intentionally fail closed until their real behavior is
recovered. MovieControl's phase-two callback is the only authored callback body
usable in isolation; it still cannot be called through normal startup.

## Required order

1. Complete the 420 deferred owner/component reader boundaries, including the
   separate MatPos provider route.
2. Implement loader-tail services: named/global data, renderer payload,
   associations, saved flags and live resource state.
3. Complete reader-backed component factories, owner hooks and event/ordinary
   membership.
4. Run the full reverse phase-one pass, then the reverse phase-two pass.
5. Recover ordinary updates, first-cut activation, camera/view admission and
   scene rendering.

The existing Window and MovieControl reader boundaries are deliberately narrow
examples of step 1. Neither is an independent startup path.
