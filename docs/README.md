# Documentation

## Startup and intro

- [Intro bootstrap](INTRO_BOOTSTRAP.md)
- [Intro camera](INTRO_CAMERA.md)
- [Cut timeline](CUT_TIMELINE.md)
- [Native startup splash](STARTUP_SPLASH.md)
- [Picture fades](PICTURE_FADE.md)
- [Position update service](POSITION_UPDATE_SERVICE.md)
- [Picture bounds](PICTURE_BOUNDS.md)
- [Group bounds](GROUP_BOUNDS.md)
- [Bounds propagation](BOUNDS_PROPAGATION.md)
- [Picture expansion](PICTURE_EXPANSION.md)
- [Picture materials](PICTURE_MATERIAL_STATE.md)
- [Picture texture-stage shader](PICTURE_STAGE_SHADER.md)
- [Indexed intro picture renderer](SDL_INTRO_RENDERER.md)
- [Intro initialization presentation](SDL_INTRO_PRESENTATION.md)
- [Picture draw order](PICTURE_DRAW_ORDER.md)
- [Ordered picture dispatch](PICTURE_ORDERED_DRAW_LOOP.md)
- [Ordered drawing coordinator](PICTURE_ORDERED_COORDINATOR.md)
- [Live owner-context preselection](PICTURE_PRESELECTION.md)
- [Picture draw reset](PICTURE_DRAW_RESET.md)
- [Picture view transitions](PICTURE_VIEW_TRANSITION.md)
- [Renderer frame lifecycle](RENDERER_FRAME.md)
- [Viewport-bounded GPU clear](SDL_PICTURE_CLEAR.md)
- [Picture projection](PICTURE_PROJECTION.md)
- [Picture submission cache](PICTURE_SUBMISSION_CACHE.md)
- [Windows reference capture](STARTUP_STATE_CAPTURE.md)

## Project and technical reference

- [Current technical map](TECHNICAL_MAP.md)
- [Resource-format census](FORMAT_CENSUS.md)
- [Windows ABI replacement map](ABI_MAP.md)
- [Code-boundary census](CODE_CENSUS.md)
- [2003 versus digital-build provenance](BUILD_PROVENANCE.md)
- [Private disassembly status](DISASSEMBLY_STATUS.md)
- [Portable data layer](DATA_LAYER.md)
- [Installation hashes and optional soundtrack](INSTALL_MANIFEST.md)
- [Derived cache and parser fuzzing specification](CACHE_AND_FUZZING.md)
- [Campaign compatibility contract](CAMPAIGN_COMPATIBILITY.md)
- [Audio-bank header format](AUDIO_FORMAT.md)
- [Scene-support dependency format](SCENE_SUPPORT_FORMAT.md)
- [Texture-catalog format](TEXTURE_FORMAT.md)
- [Primitive-catalog format](PRIMITIVE_FORMAT.md)
- [PRM-backed UI picture-resource format](PICTURE_RESOURCE.md)
- [Window-picture transform contract](PICTURE_TRANSFORM.md)
- [Packed ZGF/GMS resource envelope](PACKED_RESOURCE_FORMAT.md)
- [ZGF resource-bundle format](ZGF_FORMAT.md)
- [GMS object-source image and runtime handles](GMS_FORMAT.md)
- [RMC/RMI spatial-map format](RENDER_MAP_FORMAT.md)
- [Scene-transform evidence boundary](TRANSFORM_BOUNDARY.md)
- [Camera and projection evidence boundary](CAMERA_EVIDENCE.md)
- [Timing evidence and portable simulation policy](TIMING_EVIDENCE.md)
- [Input and controller runtime specification](INPUT_RUNTIME.md)
- [Audio mixing and positional runtime specification](AUDIO_RUNTIME.md)
- [Deterministic simulation runtime](SIMULATION_RUNTIME.md)
- [Gameplay simulation execution specification](GAMEPLAY_SIMULATION.md)
- [Owning scene render asset](SCENE_RENDER_ASSET.md)
- [Architecture](ARCHITECTURE.md)
- [Modern graphics specification](MODERN_GRAPHICS.md)
- [F10 graphics-settings overlay](GRAPHICS_SETTINGS.md)
- [Retail font runtime contract](RETAIL_FONT_RUNTIME.md)
- [Retail UI texture runtime contract](RETAIL_UI_TEXTURES.md)
- [DLSS 4.5 integration plan](DLSS.md)
- [Roadmap and acceptance gates](ROADMAP.md)
- [Phase execution specifications](PHASES.md)
- [Localization plan](LOCALIZATION.md)
- [Clean-room protocol](../CLEAN_ROOM.md)
- [Continuous integration and releases](CI.md)
- [Release engineering specification](RELEASE_ENGINEERING.md)
- [Third-party dependencies](../THIRD_PARTY.md)

## Local inspection tools

These commands inspect an owned installation without extracting assets:

```sh
python3 tools/inspect_install.py /path/to/FreedomFighters
python3 tools/resource_census.py /path/to/FreedomFighters
python3 tools/code_census.py /path/to/FreedomFighters/Freedom.Exe
```

Run them from the repository root. The code census needs its optional analysis
dependency. Use `--private-paths` with the installation inspector only for
reports kept out of the repository. Full instruction listings belong in private
research storage; `tools/private_disassemble.py` refuses output inside this repo.
