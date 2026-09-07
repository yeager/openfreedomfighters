#include "off/data/install.hpp"
#include "off/graphics/intro_runtime.hpp"
#include "off/graphics/scene_gpu_plan.hpp"
#include "off/graphics/scene_render.hpp"
#include "off/graphics/startup_graphics_asset.hpp"
#include "off/mode.hpp"
#include "off/platform/sdl_gpu_runtime.hpp"
#include "off/platform/sdl_startup.hpp"
#include "off/ui/retail_ui_fonts.hpp"
#include "off/ui/retail_ui_textures.hpp"

#include <charconv>
#include <cstddef>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace {

void usage(std::ostream &output) {
  output << "Usage: openfreedomfighters [--data PATH] [--mode original|modern] "
            "[--verify-only] [--frame-limit COUNT] [--show-graphics-menu] "
            "[--screenshot FILE.bmp] [--locale TAG] "
            "[--diagnostic-scene [RELATIVE_ARCHIVE.ZIP]]\n";
}

[[nodiscard]] std::filesystem::path default_game_data_path() {
  // An explicit environment value is useful for portable installs and test
  // systems. It never overrides --data.
  if (const char *value = std::getenv("OPENFREEDOMFIGHTERS_DATA");
      value != nullptr && *value != '\0')
    return std::filesystem::path{value};
#if defined(_WIN32)
  const char *home = std::getenv("USERPROFILE");
#else
  const char *home = std::getenv("HOME");
#endif
  if (home == nullptr || *home == '\0')
    return {};
  return std::filesystem::path{home} / ".openfreedomfighters";
}

} // namespace

int main(int argc, char **argv) {
  std::filesystem::path data_path;
  auto mode = off::Mode::original;
  bool verify_only = false;
  std::size_t frame_limit = 0;
  bool show_graphics_menu = false;
  bool diagnostic_scene = false;
  std::optional<std::filesystem::path> diagnostic_scene_archive;
  std::filesystem::path screenshot_path;
  std::string locale;
  for (int index = 1; index < argc; ++index) {
    const std::string_view argument{argv[index]};
    if (argument == "--data" && index + 1 < argc) {
      data_path = argv[++index];
    } else if (argument == "--mode" && index + 1 < argc) {
      const auto parsed = off::parse_mode(argv[++index]);
      if (!parsed) {
        std::cerr << "Unknown mode. Expected original or modern.\n";
        return 2;
      }
      mode = *parsed;
    } else if (argument == "--verify-only") {
      verify_only = true;
    } else if (argument == "--frame-limit" && index + 1 < argc) {
      const std::string_view value{argv[++index]};
      const auto [end, error] = std::from_chars(
          value.data(), value.data() + value.size(), frame_limit);
      if (error != std::errc{} || end != value.data() + value.size() ||
          frame_limit == 0) {
        std::cerr << "Frame limit must be a positive integer.\n";
        return 2;
      }
    } else if (argument == "--show-graphics-menu") {
      show_graphics_menu = true;
    } else if (argument == "--diagnostic-scene") {
      diagnostic_scene = true;
      if (index + 1 < argc && std::string_view{argv[index + 1]}.front() != '-')
        diagnostic_scene_archive = argv[++index];
    } else if (argument == "--screenshot" && index + 1 < argc) {
      screenshot_path = argv[++index];
    } else if (argument == "--locale" && index + 1 < argc) {
      locale = argv[++index];
      if (locale.empty() || locale.size() > 35U) {
        std::cerr << "Locale tag must contain 1 to 35 characters.\n";
        return 2;
      }
    } else if (argument == "--help" || argument == "-h") {
      usage(std::cout);
      return 0;
    } else if (argument == "--version") {
      std::cout << "OpenFreedomFighters 0.1.0\n";
      return 0;
    } else {
      std::cerr << "Unknown or incomplete argument: " << argument << '\n';
      usage(std::cerr);
      return 2;
    }
  }
  if (data_path.empty())
    data_path = default_game_data_path();
  if (data_path.empty() && verify_only) {
    std::cerr
        << "A legally purchased Freedom Fighters installation is required; "
           "pass --data PATH or set OPENFREEDOMFIGHTERS_DATA.\n";
    usage(std::cerr);
    return 2;
  }
  if (!screenshot_path.empty() && screenshot_path.extension() != ".bmp") {
    std::cerr << "Screenshot output must use the .bmp extension.\n";
    return 2;
  }
  if (verify_only && !screenshot_path.empty()) {
    std::cerr << "A screenshot cannot be captured in verify-only mode.\n";
    return 2;
  }
  if (!screenshot_path.empty()) {
    auto temporary = screenshot_path;
    temporary += ".part";
    if (std::filesystem::exists(screenshot_path) ||
        std::filesystem::exists(temporary)) {
      std::cerr
          << "Screenshot output already exists; refusing to overwrite it.\n";
      return 2;
    }
    const auto parent = screenshot_path.has_parent_path()
                            ? screenshot_path.parent_path()
                            : std::filesystem::current_path();
    if (!std::filesystem::is_directory(parent)) {
      std::cerr << "Screenshot output directory does not exist.\n";
      return 2;
    }
  }

  std::optional<off::data::InstallVerification> verification;
  // One application owner retains logical sound records and category state.
  // No output device/channel service exists yet; no start event is fabricated.
  off::runtime::ApplicationServices application(
      off::runtime::ClockExecutionPolicy::no_recording_or_replay,
      off::runtime::make_monotonic_clock_samples());
  // Native registration of the currently implemented concrete factory;
  // not the original complete class-list/base-class preparation.
  if(!verify_only && !diagnostic_scene) {
    application.initialize_native_group_registration();
    application.initialize_native_window_language_registration();
    application.initialize_native_picture_registration();
    application.initialize_native_camera_registration();
    application.initialize_native_second_window_scope_registration();
    application.initialize_native_visual_registration();
    application.initialize_native_room_animation_scope_registration();
    application.initialize_native_lens_flare_animation_scope_registration();
    application.initialize_native_remaining_intro_scope_registration();
  }
  std::optional<off::graphics::SceneGpuPlan> scene;
  std::optional<off::graphics::SceneRenderResolutionSummary> scene_summary;
  // Scene-manager identity lifetime, independent of source archive catalogs.
  off::runtime::SceneComponentSequence component_sequence{[&application] {
    const auto time=application.component_dispatch_time();
    if(!time) throw std::runtime_error("Live application component dispatch time has not been produced");
    return *time;
  }};
  std::optional<off::graphics::SceneRenderAsset> startup_ui_scene_resources;
  std::optional<off::graphics::StartupGraphicsAsset> startup_graphics;
  std::unique_ptr<off::graphics::IntroRuntime> intro;
  off::ui::RetailUiFontSet ui_fonts;
  off::ui::RetailUiTextureSet ui_textures;
  off::platform::StartupWindow startup_window;
  if (!verify_only) {
    auto preflight = off::platform::run_sdl_startup_preflight(data_path, [&] {
      if (diagnostic_scene) {
        const auto asset = diagnostic_scene_archive
                               ? off::graphics::load_owned_diagnostic_scene_render_asset(
                                     data_path, *diagnostic_scene_archive)
                               : off::graphics::load_diagnostic_scene_render_asset(data_path);
        scene_summary.emplace(off::graphics::summarize_scene_render_resolutions(asset));
        scene.emplace(off::graphics::prepare_scene_gpu_plan(asset));
      } else {
        // Supported normal (non-restore) cold-load boundary, before resources.
        // Native monotonic samples are an explicit CRT portability policy.
        application.reset_clock();
        // Retain exact UI-archive resources, not an original first-scene
        // selection or a guessed camera/world draw plan.
        startup_ui_scene_resources.emplace(
            off::graphics::load_startup_scene_render_asset(data_path));
        // Prepare authored first-cut resources without admitting a scene or
        // manufacturing lifecycle state. Keep ownership through the runtime.
        intro = std::make_unique<off::graphics::IntroRuntime>(
            off::graphics::load_intro_prepared_resources(
                data_path / "Scenes" / "FF-Intro.ZIP"), application, component_sequence,
            "FF-Intro.gms", off::graphics::IntroSoundLoadPolicy::directory_construction);
        // Execute the actual fresh root stage. Authored source construction and
        // its loader tail are still required before fallback/view admission.
        intro->construct_root();
        // The engine GPU runtime is created below, after CPU preflight. The
        // startup splash is a separate renderer. Reset load progress once under
        // the native staging policy, then construct the reviewed directory prefix.
        intro->begin_source_loading_without_engine_renderer();
        intro->construct_first_authored_group();
        intro->construct_window_language_groups_without_engine_renderer();
        intro->construct_picture_component_prefix_without_engine_renderer();
        intro->construct_authored_camera_without_engine_renderer();
        intro->construct_second_window_picture_without_engine_renderer();
        intro->construct_second_window_scope_without_engine_renderer();
        intro->construct_following_visual_scope_without_engine_renderer();
        intro->construct_room_animation_scope_without_engine_renderer();
        intro->construct_lens_flare_animation_scope_without_engine_renderer();
        intro->construct_remaining_directory_without_engine_renderer();
      }
      startup_graphics.emplace(off::graphics::load_startup_graphics_asset(
          data_path / "Scenes" / "FF-StartUp.ZIP"));
      ui_fonts = off::ui::load_retail_ui_fonts(
          data_path / "Scenes" / "FF-StartUp.ZIP");
    }, locale);
    if (preflight.outcome ==
        off::platform::StartupPreflightOutcome::quit_requested)
      return 0;
    if (preflight.outcome ==
        off::platform::StartupPreflightOutcome::data_error) {
      std::cerr << "Game-data verification failed: "
                << preflight.verification.message << '\n';
      return 3;
    }
    if (preflight.outcome ==
        off::platform::StartupPreflightOutcome::platform_error) {
      std::cerr << "Native startup failed: " << preflight.message << '\n';
      return 4;
    }
    verification = preflight.verification;
    startup_window = std::move(preflight.window);
  } else {
    verification = off::data::verify_install(
        data_path, {}, {.deep_audit_cache_root =
                            off::platform::application_deep_audit_cache_root()});
  }

  if (!*verification) {
    std::cerr << "Game-data verification failed: " << verification->message
              << '\n';
    return 3;
  }
  std::cout << verification->message << '\n'
            << "Mode: " << off::mode_name(mode) << '\n';
  for (const auto& warning : verification->optional_file_warnings)
    std::cerr << "Optional file skipped: " << warning << '\n';
  std::cout << "Optional soundtrack: " << verification->soundtrack_candidates.size()
            << " hash-verified files; cue mapping and playback not implemented.\n";
  if (verify_only) {
    return 0;
  }
  if (scene_summary) {
    const auto &summary = *scene_summary;
    std::cout << "Diagnostic scene geometry: "
              << summary.local_primitive << " local, "
              << summary.no_local_source << " external, "
              << summary.source_without_primitive << " source-without-primitive, "
              << summary.missing_primitive << " missing-primitive, "
              << summary.unresolved_primitive_alias << " unresolved-alias.\n";
  }
  if (!diagnostic_scene)
    std::cout << "Authored startup resources loaded; world rendering pending. "
                 "This is not gameplay or a faithful rendered startup menu.\n";
  if (intro)
    std::cout << "Source-backed intro runtime retained: "
              << intro->pictures().size() << " picture definitions, "
              << intro->resources().images().size()
              << " images; automatic scene activation remains pending.\n";
  if (intro)
    std::cout << "Retained component catalog: " << intro->components().size()
              << " entries; " << intro->components().construction_order().size()
              << " constructed. The full authored construction directory is retained;"
                 " readers, loader tail, activation and rendering remain pending.\n";
  if (intro && !intro->source_resource_scopes().empty()) {
    std::size_t allocated=0;
    for(const auto& scope:intro->source_resource_scopes()) allocated+=scope.resources.size();
    std::cout << "Source loading: " << intro->source_resource_scopes().size() << " scopes, " << allocated
              << " resources allocated; " << intro->loaded_resource_handles().size()
              << " authored owners constructed and attached; deferred readers pending.\n";
  }
  if (intro)
    std::cout << "Source-bound intro sound definitions: " << intro->resources().sounds().size()
              << "; retained authored metadata, no playback or readiness event.\n";
  if (intro)
    std::cout << "Canonical intro sound records: " << intro->sounds().size()
              << "; logical backend retained, owner preparation and playback not activated.\n";
  const auto runtime = off::platform::run_sdl_gpu_runtime(
      startup_window, mode, scene ? &*scene : nullptr, *startup_graphics,
      ui_fonts, ui_textures, intro.get(),
      frame_limit, show_graphics_menu, screenshot_path, locale);
  if (!runtime.success) {
    std::cerr << "Native runtime failed: " << runtime.message << '\n';
    return 4;
  }
  std::cout << runtime.message << '\n';
  return 0;
}
