#include "off/data/install.hpp"
#include "off/data/deferred_attachment_dispatch_shape.hpp"
#include "off/data/deferred_compact_block_profile.hpp"
#include "off/data/first_cut_component_payload_session.hpp"
#include "off/data/first_cut_owner_reader.hpp"
#include "off/data/first_cut_list_component_reader.hpp"
#include "off/data/first_cut_command_component_reader.hpp"
#include "off/graphics/intro_preview_builder.hpp"
#include "off/graphics/intro_outer_loader_tail_readiness.hpp"
#include "off/graphics/intro_runtime.hpp"
#include "off/graphics/normal_intro_scene_session.hpp"
#include "off/graphics/scene_gpu_plan.hpp"
#include "off/graphics/scene_render.hpp"
#include "off/graphics/startup_graphics_asset.hpp"
#include "off/graphics/startup_graphics_diagnostic_plan.hpp"
#include "off/graphics/startup_graphics_expanded_plan.hpp"
#include "off/mode.hpp"
#include "off/platform/sdl_gpu_runtime.hpp"
#include "off/platform/sdl_startup.hpp"
#include "off/runtime/startup_boot_scene_directory_source.hpp"
#include "off/runtime/startup_boot_scene_probe_host.hpp"
#include "off/runtime/startup_scene_package_source.hpp"
#include "off/ui/retail_ui_fonts.hpp"
#include "off/ui/retail_ui_textures.hpp"

#include <charconv>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>

#ifndef OFF_VERSION
#error "OFF_VERSION must be supplied by the CMake release version"
#endif

namespace {

void usage(std::ostream &output) {
  output << "Usage: openfreedomfighters [--data PATH] [--mode original|modern] "
            "[--verify-only] [--frame-limit COUNT] [--show-graphics-menu] "
            "[--screenshot FILE.bmp] [--locale TAG] "
            "[--diagnostic-scene [RELATIVE_ARCHIVE.ZIP]] [--diagnostic-startup-graphics] "
            "[--probe-startup-boot] [--probe-first-cut-cold]\n";
}

[[nodiscard]] std::string_view startup_boot_probe_call_name(
    off::runtime::SyntheticStartupBootSceneProbeCall call) noexcept {
  using Call = off::runtime::SyntheticStartupBootSceneProbeCall;
  switch (call) {
  case Call::registry_live:
    return "registry-live";
  case Call::allocate_ordinary_window:
    return "allocate-ordinary-window";
  case Call::canonical_live_window_owner:
    return "canonical-live-window-owner";
  case Call::attach_boot_menu_component:
    return "attach-boot-menu-component";
  case Call::live_boot_menu_component:
    return "live-boot-menu-component";
  }
  return "unknown";
}

void write_startup_boot_probe_trace(
    std::ostream &output,
    const off::runtime::SyntheticStartupBootSceneProbeResult &result) {
  // Do not emit synthetic IDs or any game-data payload. The output is limited
  // to stable diagnostic structure that can be compared with future captures.
  output << "Startup BootMenu probe (synthetic diagnostic; no scene created)\n"
         << "source-boot-owner-directory-ordinal="
         << result.source_boot_owner_directory_ordinal() << '\n'
         << "source-hierarchy-retained-node-count="
         << result.hierarchy_summary().retained_node_count << '\n'
         << "source-boot-owner-depth="
         << result.hierarchy_summary().boot_owner_depth << '\n'
         << "source-boot-owner-direct-child-count="
         << result.hierarchy_summary().boot_owner_direct_child_count << '\n'
         << "source-hierarchy-maximum-depth="
         << result.hierarchy_summary().maximum_depth << '\n'
         << "source-hierarchy-topology-digest="
         << result.hierarchy_summary().topology_digest << '\n'
         << "trace-call-count=" << result.trace().size() << '\n';
  for (std::size_t index = 0; index < result.trace().size(); ++index) {
    const auto &entry = result.trace()[index];
    output << "trace[" << index
           << "]=" << startup_boot_probe_call_name(entry.call)
           << " source-owner-ordinal="
           << entry.source_boot_owner_directory_ordinal << '\n';
  }
}

[[nodiscard]] std::string_view reader_family_label(
    off::graphics::IntroDeferredReaderFamily family) noexcept {
  using Family=off::graphics::IntroDeferredReaderFamily;
  switch(family) {
  case Family::unclassified: return "unclassified";
  case Family::sound_owner: return "sound-owner";
  case Family::window_owner: return "window-owner";
  case Family::movie_controller: return "movie-controller";
  case Family::first_cut_sequence: return "first-cut-sequence";
  case Family::first_cut_list: return "first-cut-list";
  case Family::first_cut_legal_picture: return "first-cut-legal-picture";
  case Family::external_cut_commands: return "external-cut-commands";
  case Family::first_cut_fade_picture: return "first-cut-fade-picture";
  case Family::first_cut_camera: return "first-cut-camera";
  case Family::basic_group_owner: return "basic-group-owner";
  case Family::following_visual_owner: return "following-visual-owner";
  }
  return "unknown";
}

[[nodiscard]] std::string_view reader_state_label(
    off::graphics::IntroDeferredReaderImplementationState state) noexcept {
  using State=off::graphics::IntroDeferredReaderImplementationState;
  switch(state) {
  case State::unimplemented: return "unimplemented";
  case State::implemented_not_applied: return "recognized-not-admitted";
  case State::applied: return "applied";
  }
  return "unknown";
}

void write_reader_coverage_probe(std::ostream& output,
    const off::graphics::IntroDeferredReaderCoverageInventory& coverage) {
  constexpr std::size_t family_count=12U;
  constexpr std::size_t state_count=3U;
  std::array<std::array<std::size_t,state_count>,family_count> counts{};
  for(const auto& entry:coverage.entries) {
    const auto family=static_cast<std::size_t>(entry.family);
    const auto state=static_cast<std::size_t>(entry.state);
    if(family>=family_count || state>=state_count)
      throw std::runtime_error("reader coverage probe has an unknown enum value");
    counts[family][state]+=entry.count;
  }
  if(coverage.total_applied>coverage.total_discovered)
    throw std::runtime_error("reader coverage probe has inconsistent totals");
  output << "reader-coverage-discovered=" << coverage.total_discovered << '\n'
         << "reader-coverage-recognized=" << coverage.total_supported << '\n'
         << "reader-coverage-applied=" << coverage.total_applied << '\n'
         << "reader-coverage-unapplied=" << coverage.total_discovered-coverage.total_applied << '\n'
         << "reader-unclassified-no-attachments=" << coverage.unclassified_without_attachments << '\n'
         << "reader-unclassified-with-attachments=" << coverage.unclassified_with_attachments << '\n'
         << "reader-unclassified-terminal-before-attachment=" << coverage.unclassified_terminal_before_attachment << '\n'
         << "reader-unclassified-attachment-before-terminal=" << coverage.unclassified_attachment_before_terminal << '\n'
         << "reader-unclassified-unknown-dispatch-shape=" << coverage.unclassified_unknown_dispatch_shape << '\n'
         << "reader-unclassified-attachment-delimiters=" << coverage.unclassified_attachment_delimiters << '\n'
         << "reader-unclassified-max-attachment-delimiters=" << coverage.unclassified_max_attachment_delimiters << '\n';
  for(std::size_t family=0;family<family_count;++family) {
    for(std::size_t state=0;state<state_count;++state) {
      if(counts[family][state]==0U) continue;
      output << "reader-family=" << reader_family_label(
                    static_cast<off::graphics::IntroDeferredReaderFamily>(family))
             << " state=" << reader_state_label(
                    static_cast<off::graphics::IntroDeferredReaderImplementationState>(state))
             << " count=" << counts[family][state] << '\n';
    }
  }
  for(const auto& entry:coverage.entries) {
    if(entry.family!=off::graphics::IntroDeferredReaderFamily::unclassified ||
       entry.state!=off::graphics::IntroDeferredReaderImplementationState::unimplemented)
      continue;
    output << "reader-unimplemented-source-type=0x" << std::hex << entry.source_type
           << std::dec << " count=" << entry.count << '\n';
  }
}

int run_first_cut_cold_probe(const std::filesystem::path &data_path) {
  off::runtime::ApplicationServices application(
      off::runtime::ClockExecutionPolicy::no_recording_or_replay,
      off::runtime::make_monotonic_clock_samples());
  application.initialize_native_group_registration();
  application.initialize_native_window_language_registration();
  application.initialize_native_picture_registration();
  application.initialize_native_camera_registration();
  application.initialize_native_second_window_scope_registration();
  application.initialize_native_visual_registration();
  application.initialize_native_room_animation_scope_registration();
  application.initialize_native_lens_flare_animation_scope_registration();
  application.initialize_native_remaining_intro_scope_registration();
  off::runtime::SceneComponentSequence components{[&application] {
    const auto time=application.component_dispatch_time();
    if(!time) throw std::runtime_error("first-cut cold probe has no component dispatch time");
    return *time;
  }};
  auto runtime=std::make_unique<off::graphics::IntroRuntime>(
      off::graphics::load_intro_prepared_resources(data_path / "Scenes" / "FF-Intro.ZIP"),
      application,components,"FF-Intro.gms",
      off::graphics::IntroSoundLoadPolicy::directory_construction);
  runtime->construct_root();
  runtime->begin_source_loading_without_engine_renderer();
  runtime->construct_first_authored_group();
  runtime->construct_window_language_groups_without_engine_renderer();
  runtime->construct_picture_component_prefix_without_engine_renderer();
  runtime->construct_authored_camera_without_engine_renderer();
  runtime->construct_second_window_picture_without_engine_renderer();
  runtime->construct_second_window_scope_without_engine_renderer();
  runtime->construct_following_visual_scope_without_engine_renderer();
  runtime->construct_room_animation_scope_without_engine_renderer();
  runtime->construct_lens_flare_animation_scope_without_engine_renderer();
  runtime->construct_remaining_directory_without_engine_renderer();
  auto session=off::graphics::make_normal_intro_scene_session(std::move(runtime));
  session->complete_postconstruction_reader_bracket(0U);
  auto& intro=session->runtime();
  const auto reader_coverage=intro.reader_coverage_inventory();
  const auto matpos_dispatch=intro.matpos_deferred_dispatch_inventory();
  if(reader_coverage.stage!=off::graphics::IntroReaderBracketStage::ordinary_reader_boundary_complete ||
      reader_coverage.total_discovered!=intro.deferred_reader_work().size())
    throw std::runtime_error("first-cut cold probe found incomplete reader coverage");
  const auto first_cut_source=intro.resources().first_cut_index();
  const auto* first_cut_work=static_cast<const off::graphics::IntroDeferredReaderWork*>(nullptr);
  for(const auto& work:intro.deferred_reader_work()) {
    if(work.source_directory_index==first_cut_source) {
      if(first_cut_work)
        throw std::runtime_error("first-cut cold probe found duplicate deferred work");
      first_cut_work=&work;
    }
  }
  if(!first_cut_work)
    throw std::runtime_error("first-cut cold probe found no deferred work");
  const auto* list_reader=intro.first_cut_list_reader_state();
  const auto* component_reader=intro.first_cut_component_reader_state();
  if(!list_reader || !component_reader || component_reader->owner!=intro.source_handle(first_cut_source) ||
      component_reader->resource!=first_cut_work->resource ||
      component_reader->component_indices!=list_reader->component_indices)
    throw std::runtime_error("first-cut cold probe found no matching component reader state");
  const auto& directory=intro.resources().sources().directory();
  const auto mapping=intro.directory_resource_mapping();
  if(first_cut_work->resource.value==0U || first_cut_work->source_offset==0U ||
      first_cut_work->source_directory_index!=first_cut_source ||
      first_cut_source>=directory.size() || first_cut_source>=mapping.size() ||
      directory[first_cut_source].deferred_source_offset!=first_cut_work->source_offset ||
      mapping[first_cut_source]!=first_cut_work->resource)
    throw std::runtime_error("first-cut cold probe found an unbound deferred identity");
  const auto block=intro.resources().sources().deferred_source_block(first_cut_source);
  const auto suffix=off::data::FirstCutOwnerReader::read(block);
  if(suffix.component_suffix.data()!=block.data()+14U || suffix.component_suffix.size()!=157U ||
      suffix.component_extent!=157U)
    throw std::runtime_error("first-cut cold probe found an unbounded component suffix");
  const auto parsed_components=off::data::FirstCutComponentPayloadSession::read(
      suffix);
  const auto& first_cut_source_data=intro.resources().first_cut();
  if(parsed_components.list.controls!=first_cut_source_data.settings_words ||
      std::bit_cast<std::uint32_t>(parsed_components.list.final_value)!=
          std::bit_cast<std::uint32_t>(first_cut_source_data.final_value))
    throw std::runtime_error("first-cut cold probe found a component-parser disagreement");
  for(std::size_t index=0;index<first_cut_source_data.commands.size();++index) {
    const auto& command_record=parsed_components.commands[index];
    const auto& command=first_cut_source_data.commands[index];
    if(command_record.timeline_position!=command.timeline_position ||
        command_record.event_reference!=command.event_reference ||
        command_record.target_reference!=command.target_reference ||
        command_record.event_argument!=command.event_argument ||
        command_record.target_name!=command.target_name)
      throw std::runtime_error("first-cut cold probe found a command-parser disagreement");
  }
  const off::data::DeferredReaderWorkIdentity identity{
      first_cut_work->resource.value,first_cut_work->source_offset,first_cut_work->source_directory_index};
  off::data::DeferredReaderSession owner_session(identity,block);
  if(owner_session.state()!=off::data::DeferredReaderSessionState::created ||
      owner_session.owner_block().data()==block.data())
    throw std::runtime_error("first-cut cold probe did not copy the owner block");
  owner_session.prepare(identity);
  if(owner_session.state()!=off::data::DeferredReaderSessionState::prepared)
    throw std::runtime_error("first-cut cold probe did not prepare the owner reader");
  owner_session.read_owner(off::data::FirstCutOwnerReader::read);
  if(owner_session.state()!=off::data::DeferredReaderSessionState::owner_read ||
      owner_session.identity()!=identity || owner_session.owner_block().size()!=block.size())
    throw std::runtime_error("first-cut cold probe did not retain the owner-reader boundary");
  owner_session.deactivate();
  if(owner_session.state()!=off::data::DeferredReaderSessionState::deactivated ||
      !owner_session.owner_block().empty())
    throw std::runtime_error("first-cut cold probe did not release the owner-reader boundary");
  session->prepare_supported_first_cut_player();
  const auto* first_cut=session->first_cut_player();
  if(!first_cut || first_cut->initialization().phase_one_complete() ||
      first_cut->initialization().phase_two_complete() || first_cut->receiver().open() ||
      first_cut->receiver().closed())
    throw std::runtime_error("first-cut cold probe observed an unexpected lifecycle transition");
  const auto tail_readiness=session->outer_loader_tail_readiness();
  if(tail_readiness.ready_to_run())
    throw std::runtime_error("first-cut cold probe unexpectedly considers the loader tail runnable");
  std::cout << "First-cut cold probe verified\n"
            << "reader-states=2\n"
            << "component-payloads=6\n"
            << "owner-envelope=verified\n"
            << "first-component-payload=verified\n"
            << "command-component-payloads=5-verified\n"
            ;
  write_reader_coverage_probe(std::cout,reader_coverage);
  std::cout << "matpos-deferred-records=" << matpos_dispatch.associated_records << '\n'
            << "matpos-terminal-first=" << matpos_dispatch.terminal_before_first_attachment_delimiter << '\n'
            << "matpos-attachment-first=" << matpos_dispatch.attachment_delimiter_precedes_terminal << '\n'
            << "matpos-attachment-delimiters=" << matpos_dispatch.attachment_delimiters << '\n';
  std::set<std::uint32_t> unimplemented_source_types;
  for(const auto& entry:reader_coverage.entries) {
    if(entry.family==off::graphics::IntroDeferredReaderFamily::unclassified &&
       entry.state==off::graphics::IntroDeferredReaderImplementationState::unimplemented)
      unimplemented_source_types.insert(entry.source_type);
  }
  std::map<std::string,std::size_t> deferred_signatures;
  for(const auto& work:intro.deferred_reader_work()) {
    const auto& source=intro.resources().sources().directory().at(work.source_directory_index);
    if(!unimplemented_source_types.contains(source.source_type)) continue;
    const auto block=intro.resources().sources().deferred_source_block(work.source_directory_index);
    std::string signature="bytes="+std::to_string(block.size())+
        " attachment-count="+std::to_string(source.attachments.size());
    try {
      const auto observation=off::data::DeferredAttachmentDispatchClassifier::observe(
          block.subspan(sizeof(std::uint32_t)));
      signature+=" delimiters="+std::to_string(observation.delimiter_count);
      signature+=observation.shape==off::data::DeferredAttachmentDispatchShape::terminal_before_first_attachment_delimiter ?
          " terminal-first" : " attachment-first";
      const auto profile=off::data::DeferredCompactBlockProfiler::profile(
          block.subspan(sizeof(std::uint32_t)));
      signature+=" values="+std::to_string(profile.encoded_values)+
          " continued="+std::to_string(profile.continuation_values)+
          " framing="+std::to_string(profile.framing_digest)+
          " notation="+profile.framing_notation+
          " kinds="+std::to_string(profile.value_kinds[1U])+","+
          std::to_string(profile.value_kinds[2U])+","+
          std::to_string(profile.value_kinds[3U])+","+
          std::to_string(profile.value_kinds[4U])+","+
          std::to_string(profile.value_kinds[5U]);
    } catch(const std::exception&) { signature+=" malformed-dispatch"; }
    ++deferred_signatures[std::move(signature)];
  }
  for(const auto& [signature,count]:deferred_signatures)
    std::cout << "reader-signature=" << signature << " count=" << count << '\n';
  std::cout << "outer-loader-tail=native-services-required\n"
            << "outer-loader-named-global-bytes=" << tail_readiness.named_global_bytes << '\n'
            << "outer-loader-renderer-bytes=" << tail_readiness.renderer_resource_bytes << '\n'
            << "outer-loader-associations=" << tail_readiness.resource_association_count << '\n'
            << "outer-loader-allocation-sizing-rows=" << tail_readiness.allocation_sizing_row_count << '\n';
  for(const auto boundary:tail_readiness.required_boundaries)
    std::cout << "outer-loader-pending="
              << off::graphics::intro_outer_loader_tail_boundary_label(boundary) << '\n';
  std::cout
            << "phase-one=not-run\n"
            << "phase-two=not-run\n"
            << "renderer=not-admitted\n"
            << "audio=not-started\n";
  return 0;
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
  bool diagnostic_startup_graphics = false;
  bool probe_startup_boot = false;
  bool probe_first_cut_cold = false;
  bool mode_specified = false;
  std::optional<std::filesystem::path> diagnostic_scene_archive;
  std::filesystem::path screenshot_path;
  std::string locale;
  for (int index = 1; index < argc; ++index) {
    const std::string_view argument{argv[index]};
    if (argument == "--data" && index + 1 < argc) {
      data_path = argv[++index];
    } else if (argument == "--mode" && index + 1 < argc) {
      mode_specified = true;
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
    } else if (argument == "--diagnostic-startup-graphics") {
      diagnostic_startup_graphics = true;
    } else if (argument == "--probe-startup-boot") {
      probe_startup_boot = true;
    } else if (argument == "--probe-first-cut-cold") {
      probe_first_cut_cold = true;
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
      std::cout << "OpenFreedomFighters " OFF_VERSION "\n";
      return 0;
    } else {
      std::cerr << "Unknown or incomplete argument: " << argument << '\n';
      usage(std::cerr);
      return 2;
    }
  }
  if (data_path.empty())
    data_path = default_game_data_path();
  if (data_path.empty() && (verify_only || probe_startup_boot || probe_first_cut_cold)) {
    std::cerr
        << "A legally purchased Freedom Fighters installation is required; "
           "pass --data PATH or set OPENFREEDOMFIGHTERS_DATA.\n";
    usage(std::cerr);
    return 2;
  }
  if (diagnostic_scene && diagnostic_startup_graphics) {
    std::cerr << "Select only one diagnostic renderer.\n";
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
  if (probe_startup_boot &&
      (verify_only || diagnostic_scene || frame_limit != 0U ||
       show_graphics_menu || !screenshot_path.empty() || !locale.empty() ||
       mode_specified)) {
    std::cerr
        << "--probe-startup-boot cannot be combined with runtime options.\n";
    usage(std::cerr);
    return 2;
  }
  if (probe_first_cut_cold &&
      (verify_only || probe_startup_boot || diagnostic_scene || frame_limit != 0U ||
       show_graphics_menu || !screenshot_path.empty() || !locale.empty() || mode_specified)) {
    std::cerr << "--probe-first-cut-cold cannot be combined with runtime options.\n";
    usage(std::cerr);
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

  if (probe_startup_boot) {
    const auto verification = off::data::verify_install(
        data_path, {},
        {.deep_audit_cache_root =
             off::platform::application_deep_audit_cache_root()});
    if (!verification) {
      std::cerr << "Game-data verification failed: " << verification.message
                << '\n';
      return 3;
    }
    try {
      auto package =
          std::make_shared<const off::runtime::StartupSceneLoadPackage>(
              off::runtime::StartupScenePackageSource::prepare_checked(
                  "FF-Startup", data_path / "Scenes" / "FF-StartUp.ZIP"));
      const auto &inputs = *package->factory_inputs();
      const auto directory =
          off::runtime::StartupBootSceneDirectorySource::from_checked_gms(
              inputs.gms());
      const auto result =
          off::runtime::SyntheticStartupBootSceneProbeHost::observe(
              std::move(package), directory,
              {.synthetic_factory_generation = 1U,
               .synthetic_owner_identity = 1U,
               .synthetic_component_identity = 1U});
      write_startup_boot_probe_trace(std::cout, result);
      return 0;
    } catch (const std::exception &error) {
      std::cerr << "Startup BootMenu probe failed: " << error.what() << '\n';
      return 3;
    }
  }

  if (probe_first_cut_cold) {
    const auto verification=off::data::verify_install(
        data_path,{}, {.deep_audit_cache_root=off::platform::application_deep_audit_cache_root()});
    if(!verification) {
      std::cerr << "Game-data verification failed: " << verification.message << '\n';
      return 3;
    }
    try { return run_first_cut_cold_probe(data_path); }
    catch(const std::exception& error) {
      std::cerr << "First-cut cold probe failed: " << error.what() << '\n';
      return 3;
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
  if (!verify_only && !diagnostic_scene) {
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
    const auto time = application.component_dispatch_time();
    if (!time)
      throw std::runtime_error(
          "Live application component dispatch time has not been produced");
    return *time;
  }};
  std::optional<off::graphics::SceneRenderAsset> startup_ui_scene_resources;
  std::optional<off::graphics::StartupGraphicsAsset> startup_graphics;
  std::optional<off::graphics::StartupGraphicsExpandedPlan>
      startup_graphics_cpu_plan;
  std::unique_ptr<off::graphics::NormalIntroSceneSession> intro_session;
  off::graphics::IntroRuntime *intro{};
  std::optional<off::graphics::IntroPreviewSnapshot>
      intro_legal_picture_preflight;
  off::ui::RetailUiFontSet ui_fonts;
  off::ui::RetailUiTextureSet ui_textures;
  off::platform::StartupWindow startup_window;
  if (!verify_only) {
    auto preflight = off::platform::run_sdl_startup_preflight(
        data_path,
        [&] {
          if (diagnostic_scene) {
            const auto asset =
                diagnostic_scene_archive
                    ? off::graphics::load_owned_diagnostic_scene_render_asset(
                          data_path, *diagnostic_scene_archive)
                    : off::graphics::load_diagnostic_scene_render_asset(
                          data_path);
            scene_summary.emplace(
                off::graphics::summarize_scene_render_resolutions(asset));
            scene.emplace(off::graphics::prepare_scene_gpu_plan(asset));
          } else if (!diagnostic_startup_graphics) {
            // Supported normal (non-restore) cold-load boundary, before
            // resources. Native monotonic samples are an explicit CRT
            // portability policy.
            application.reset_clock();
            // Retain exact UI-archive resources, not an original first-scene
            // selection or a guessed camera/world draw plan.
            startup_ui_scene_resources.emplace(
                off::graphics::load_startup_scene_render_asset(data_path));
            // Prepare authored first-cut resources without admitting a scene or
            // manufacturing lifecycle state. Keep ownership through the
            // runtime.
            auto intro_runtime = std::make_unique<off::graphics::IntroRuntime>(
                off::graphics::load_intro_prepared_resources(
                    data_path / "Scenes" / "FF-Intro.ZIP"),
                application, component_sequence, "FF-Intro.gms",
                off::graphics::IntroSoundLoadPolicy::directory_construction);
            intro = intro_runtime.get();
            // Execute the actual fresh root stage. Authored source construction
            // and its loader tail are still required before fallback/view
            // admission.
            intro->construct_root();
            // The engine GPU runtime is created below, after CPU preflight. The
            // startup splash is a separate renderer. Reset load progress once
            // under the native staging policy, then construct the reviewed
            // directory prefix.
            intro->begin_source_loading_without_engine_renderer();
            intro->construct_first_authored_group();
            intro->construct_window_language_groups_without_engine_renderer();
            intro->construct_picture_component_prefix_without_engine_renderer();
            intro->construct_authored_camera_without_engine_renderer();
            intro->construct_second_window_picture_without_engine_renderer();
            intro->construct_second_window_scope_without_engine_renderer();
            intro->construct_following_visual_scope_without_engine_renderer();
            intro->construct_room_animation_scope_without_engine_renderer();
            intro
                ->construct_lens_flare_animation_scope_without_engine_renderer();
            intro->construct_remaining_directory_without_engine_renderer();
            // The reviewed cold-load reader bracket establishes source-backed
            // first-cut ownership and reference translation.  It deliberately
            // does not admit a global lifecycle, schedule an event, activate a
            // cut, or claim that the engine renderer has consumed these
            // records.
            intro_session = off::graphics::make_normal_intro_scene_session(
                std::move(intro_runtime));
            intro_session->complete_postconstruction_reader_bracket(0U);
            intro_session->prepare_supported_first_cut_player();
            const auto legal_source =
                intro->resources()
                    .sources()
                    .local_source_for_authored_reference(
                        intro->resources().member().references[1]);
            if (!legal_source)
              throw std::runtime_error(
                  "first-cut legal picture source is unavailable");
            intro_legal_picture_preflight.emplace(
                off::graphics::build_intro_preview(
                    *intro, *legal_source, {.width = 1280U, .height = 720U}));
          }
          startup_graphics.emplace(off::graphics::load_startup_graphics_asset(
              data_path / "Scenes" / "FF-StartUp.ZIP"));
          startup_graphics_cpu_plan.emplace(
              off::graphics::expand_startup_graphics_plan_with_composed_transforms(
                  *startup_graphics, 0x01U));
          if (diagnostic_startup_graphics)
            scene.emplace(off::graphics::make_startup_graphics_diagnostic_plan(
                *startup_graphics, *startup_graphics_cpu_plan));
          ui_fonts = off::ui::load_retail_ui_fonts(data_path / "Scenes" /
                                                   "FF-StartUp.ZIP");
        },
        locale);
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
        data_path, {},
        {.deep_audit_cache_root =
             off::platform::application_deep_audit_cache_root()});
  }

  if (!*verification) {
    std::cerr << "Game-data verification failed: " << verification->message
              << '\n';
    return 3;
  }
  std::cout << verification->message << '\n'
            << "Mode: " << off::mode_name(mode) << '\n';
  for (const auto &warning : verification->optional_file_warnings)
    std::cerr << "Optional file skipped: " << warning << '\n';
  std::cout << "Optional soundtrack: "
            << verification->soundtrack_candidates.size()
            << " hash-verified candidates; playback remains disabled until "
               "cue mapping is verified, so game music is retained.\n";
  if (verify_only) {
    return 0;
  }
  if (scene_summary) {
    const auto &summary = *scene_summary;
    std::cout << "Diagnostic scene geometry: " << summary.local_primitive
              << " local, " << summary.no_local_source << " no-local-source, "
              << summary.source_without_primitive
              << " source-without-primitive, " << summary.missing_primitive
              << " missing-primitive, " << summary.unresolved_primitive_alias
              << " unresolved-alias.\n";
    if (summary.local_primitive == 0U) {
      std::cout << "Diagnostic scene has no direct-local geometry to draw; "
                   "indirect source resolution is pending.\n";
    }
  }
  if (!diagnostic_scene && !diagnostic_startup_graphics)
    std::cout << "Authored startup resources loaded; world rendering pending. "
                 "This is not gameplay or a faithful rendered startup menu.\n";
  if (startup_graphics_cpu_plan)
    std::cout << "Startup menu CPU plan: "
              << startup_graphics_cpu_plan->submissions().size()
              << " source-backed picture submissions; GPU submission pending.\n";
  if (diagnostic_startup_graphics)
    std::cout << "Startup graphics diagnostic: source images and source quad "
                 "geometry, generic fit projection; not a faithful menu.\n";
  if (intro)
    std::cout << "Source-backed intro runtime retained: "
              << intro->pictures().size() << " picture definitions, "
              << intro->resources().images().size()
              << " images; automatic scene activation remains pending.\n";
  if (intro_legal_picture_preflight)
    std::cout << "First-cut legal picture preflight: "
              << intro_legal_picture_preflight->draw.draw_plan.groups().size()
              << " draw groups, "
              << intro_legal_picture_preflight->images.size()
              << " referenced images; not admitted for display.\n";
  if (intro_session && intro_session->first_cut_player())
    std::cout << "Source-backed first-cut session retained: cold command "
                 "admission and no playback.\n";
  if (intro)
    std::cout
        << "Retained component catalog: " << intro->components().size()
        << " entries; " << intro->components().construction_order().size()
        << " constructed. The full authored construction directory is retained;"
           " loader tail, activation and rendering remain pending.\n";
  if (intro && !intro->source_resource_scopes().empty()) {
    std::size_t allocated = 0;
    for (const auto &scope : intro->source_resource_scopes())
      allocated += scope.resources.size();
    std::cout << "Source loading: " << intro->source_resource_scopes().size()
              << " scopes, " << allocated << " resources allocated; "
              << intro->loaded_resource_handles().size()
              << " authored owners constructed and attached.\n";
  }
  if (intro) {
    const auto reader_preflight = intro->preflight_global_lifecycle();
    std::cout << "Deferred readers: " << reader_preflight.covered_readers
              << " source-backed readers admitted of "
              << reader_preflight.expected_readers
              << "; remaining lifecycle and playback work is pending.\n";
  }
  if (intro)
    std::cout
        << "Source-bound intro sound definitions: "
        << intro->resources().sounds().size()
        << "; retained authored metadata, no playback or readiness event.\n";
  if (intro)
    std::cout << "Canonical intro sound records: " << intro->sounds().size()
              << "; logical backend retained, owner preparation and playback "
                 "not activated.\n";
  const auto runtime = off::platform::run_sdl_gpu_runtime(
      startup_window, mode, scene ? &*scene : nullptr, *startup_graphics,
      ui_fonts, ui_textures, intro, frame_limit, show_graphics_menu,
      screenshot_path, locale, diagnostic_startup_graphics);
  if (!runtime.success) {
    std::cerr << "Native runtime failed: " << runtime.message << '\n';
    return 4;
  }
  std::cout << runtime.message << '\n';
  return 0;
}
