#include "off/data/install.hpp"
#include "off/data/archive_vfs.hpp"
#include "off/data/audio_bank_header.hpp"
#include "off/data/audio_bank_profile.hpp"
#include "off/data/deferred_attachment_dispatch_shape.hpp"
#include "off/data/deferred_compact_block_profile.hpp"
#include "off/data/first_cut_component_payload_session.hpp"
#include "off/data/first_cut_owner_reader.hpp"
#include "off/data/first_cut_list_component_reader.hpp"
#include "off/data/first_cut_command_component_reader.hpp"
#include "off/data/loc_string_index.hpp"
#include "off/data/loc_catalog.hpp"
#include "off/data/zip_archive.hpp"
#include "off/audio/duration_comparison.hpp"
#include "off/audio/soundtrack_catalog.hpp"
#include "off/audio/soundtrack_stream.hpp"
#include "off/cutscene/first_cut_player_initialization.hpp"
#include "off/cutscene/first_cut_command_session.hpp"
#include "off/cutscene/first_cut_timeline_profile.hpp"
#include "off/graphics/intro_preview_builder.hpp"
#include "off/graphics/intro_named_global_section_envelope.hpp"
#include "off/graphics/intro_renderer_payload_observation.hpp"
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
#include "off/runtime/startup_boot_menu_component_envelope.hpp"
#include "off/runtime/startup_boot_scene_probe_host.hpp"
#include "off/runtime/startup_boot_scene_registry.hpp"
#include "off/runtime/startloader_prepared_route.hpp"
#include "off/runtime/movie_cut_loader_package_source.hpp"
#include "off/runtime/movie_cut_main_package_source.hpp"
#include "off/runtime/startup_scene_package_source.hpp"
#include "off/ui/retail_ui_fonts.hpp"
#include "off/ui/retail_ui_textures.hpp"
#include "off/ui/retail_localization_cache.hpp"
#include "off/ui/private_translation_pack.hpp"

#include <charconv>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#ifndef OFF_VERSION
#error "OFF_VERSION must be supplied by the CMake release version"
#endif

namespace {

void usage(std::ostream &output) {
  output << "Usage: openfreedomfighters [--data PATH] [--mode original|modern] "
            "[--verify-only] [--frame-limit COUNT] [--show-graphics-menu] "
            "[--screenshot FILE.bmp] [--locale TAG] "
            "[--diagnostic-scene [RELATIVE_ARCHIVE.ZIP]] [--diagnostic-startup-graphics] [--diagnostic-intro-picture] "
            "[--probe-startup-boot] [--probe-startup-route-cold] [--probe-soundtrack] [--probe-localization] [--probe-movie-cuts] [--probe-first-cut-cold] [--probe-first-cut-initialization] [--probe-intro-renderer-payload] [--probe-intro-named-global]\n";
}

[[nodiscard]] off::data::AudioBankProfile inspect_verified_game_audio(
    const std::filesystem::path& root) {
  off::data::ArchiveVfs installation_vfs;
  constexpr std::array<std::string_view, 5> excluded{
      "Freedom_Fighters_OST", "Launcher.exe", "eax.dll", "steam_api.dll",
      "steam_appid.txt"};
  static_cast<void>(installation_vfs.mount_directory(root, excluded));
  off::data::AudioBankProfile profile;
  std::error_code error;
  std::filesystem::recursive_directory_iterator entries(root / "Scenes", error);
  if (error) throw std::runtime_error("could not enumerate verified scene audio");
  for (const auto& entry : entries) {
    if (!entry.is_regular_file() || entry.path().extension() != ".WHD") continue;
    const auto relative = std::filesystem::relative(entry.path(), root, error);
    if (error) throw std::runtime_error("could not resolve verified scene audio path");
    profile.add(off::data::AudioBankHeader::parse(
        installation_vfs.read(relative.generic_string())).records());
  }
  return profile;
}

[[nodiscard]] std::vector<off::audio::AudioDuration>
inspect_verified_global_vorbis_durations(const std::filesystem::path& root) {
  off::data::ArchiveVfs installation_vfs;
  constexpr std::array<std::string_view, 5> excluded{
      "Freedom_Fighters_OST", "Launcher.exe", "eax.dll", "steam_api.dll",
      "steam_appid.txt"};
  static_cast<void>(installation_vfs.mount_directory(root, excluded));
  std::set<std::array<std::uint32_t, 5>> unique_records;
  std::vector<off::audio::AudioDuration> result;
  std::error_code error;
  std::filesystem::recursive_directory_iterator entries(root / "Scenes", error);
  if (error) throw std::runtime_error("could not enumerate verified scene audio");
  for (const auto& entry : entries) {
    if (!entry.is_regular_file() || entry.path().extension() != ".WHD") continue;
    const auto relative = std::filesystem::relative(entry.path(), root, error);
    if (error) throw std::runtime_error("could not resolve verified scene audio path");
    const auto header = off::data::AudioBankHeader::parse(
        installation_vfs.read(relative.generic_string()));
    for (const auto& record : header.records()) {
      constexpr std::uint32_t global_bank_flag = 0x80000000U;
      constexpr std::uint32_t vorbis_format = 0x00001000U;
      if (!record.uses_global_bank() ||
          (record.format_flags & ~global_bank_flag) != vorbis_format) {
        continue;
      }
      const std::array identity{record.data_offset, record.encoded_size,
                                record.sample_rate, record.channels,
                                record.sample_value_count};
      if (!unique_records.insert(identity).second) continue;
      if (record.channels == 0U ||
          record.sample_value_count % record.channels != 0U) {
        throw std::runtime_error(
            "global Vorbis duration has invalid channel layout");
      }
      result.push_back({.frames = record.sample_value_count / record.channels,
                        .sample_rate = record.sample_rate});
    }
  }
  if (result.empty()) {
    throw std::runtime_error("verified installation has no global Vorbis streams");
  }
  return result;
}

void write_soundtrack_probe(
    const off::data::InstallVerification& verification, std::ostream& output) {
  const auto catalog = off::audio::SoundtrackCatalog::from_verified_candidates(
      verification.soundtrack_candidates);
  std::size_t preferred_flac{}, preferred_mp3{}, fallback_editions{};
  std::set<std::uint32_t> sample_rates;
  std::set<std::uint32_t> channel_counts;
  std::uint32_t maximum_sample_rate{};
  std::uint32_t maximum_channels{};
  std::uint64_t shortest_track_frames{std::numeric_limits<std::uint64_t>::max()};
  std::uint64_t longest_track_frames{};
  std::size_t fallback_frame_matches{};
  std::size_t fallback_frame_mismatches{};
  std::vector<off::audio::AudioDuration> album_durations;
  album_durations.reserve(catalog.tracks().size());
  for (const auto& track : catalog.tracks()) {
    const auto info = off::audio::SoundtrackStream::open(track.preferred.path).info();
    if ((track.preferred.format == off::audio::SoundtrackFormat::flac &&
         info.encoding != off::audio::Encoding::flac) ||
        (track.preferred.format == off::audio::SoundtrackFormat::mp3 &&
         info.encoding != off::audio::Encoding::mp3)) {
      throw std::runtime_error("verified soundtrack edition has inconsistent stream format");
    }
    if (track.preferred.format == off::audio::SoundtrackFormat::flac)
      ++preferred_flac;
    else
      ++preferred_mp3;
    fallback_editions += track.fallback.has_value() ? 1U : 0U;
    sample_rates.insert(info.sample_rate);
    channel_counts.insert(info.channels);
    maximum_sample_rate = std::max(maximum_sample_rate, info.sample_rate);
    maximum_channels = std::max(maximum_channels, info.channels);
    shortest_track_frames = std::min(shortest_track_frames, info.total_frames);
    longest_track_frames = std::max(longest_track_frames, info.total_frames);
    album_durations.push_back(
        {.frames = info.total_frames, .sample_rate = info.sample_rate});
    if (track.fallback) {
      const auto fallback = off::audio::SoundtrackStream::open(
          track.fallback->path).info();
      if (fallback.sample_rate != info.sample_rate ||
          fallback.channels != info.channels)
        throw std::runtime_error("soundtrack editions disagree on stream layout");
      if (fallback.total_frames == info.total_frames)
        ++fallback_frame_matches;
      else
        ++fallback_frame_mismatches;
    }
  }
  const auto game_audio = inspect_verified_game_audio(verification.root);
  const auto game_vorbis = inspect_verified_global_vorbis_durations(verification.root);
  const auto duration_comparison = off::audio::compare_audio_durations(
      game_vorbis, album_durations, 250'000U);
  output << "soundtrack-probe=completed\n"
         << "soundtrack-hash-verified-files=" << verification.soundtrack_candidates.size() << '\n'
         << "soundtrack-album-tracks=" << catalog.tracks().size() << '\n'
         << "soundtrack-preferred-flac=" << preferred_flac << '\n'
         << "soundtrack-preferred-mp3=" << preferred_mp3 << '\n'
         << "soundtrack-fallback-editions=" << fallback_editions << '\n'
         << "soundtrack-distinct-sample-rates=" << sample_rates.size() << '\n'
         << "soundtrack-distinct-channel-counts=" << channel_counts.size() << '\n'
         << "soundtrack-maximum-sample-rate=" << maximum_sample_rate << '\n'
         << "soundtrack-maximum-channels=" << maximum_channels << '\n'
         << "soundtrack-shortest-track-frames=" << shortest_track_frames << '\n'
         << "soundtrack-longest-track-frames=" << longest_track_frames << '\n'
         << "soundtrack-fallback-exact-frame-matches=" << fallback_frame_matches << '\n'
         << "soundtrack-fallback-frame-mismatches=" << fallback_frame_mismatches << '\n'
         << "game-audio-profile-records=" << game_audio.record_count() << '\n'
         << "game-audio-profile-pcm=" << game_audio.pcm_record_count() << '\n'
         << "game-audio-profile-ima-adpcm=" << game_audio.ima_adpcm_record_count() << '\n'
         << "game-audio-profile-vorbis=" << game_audio.vorbis_record_count() << '\n'
         << "game-audio-profile-other=" << game_audio.other_record_count() << '\n'
         << "game-audio-profile-distinct-sample-rates="
         << game_audio.distinct_sample_rate_count() << '\n'
         << "game-audio-profile-distinct-channel-counts="
         << game_audio.distinct_channel_count() << '\n'
         << "game-audio-profile-maximum-sample-rate="
         << game_audio.maximum_sample_rate() << '\n'
         << "game-audio-profile-maximum-bits-per-sample="
         << game_audio.maximum_bits_per_sample() << '\n'
         << "game-audio-profile-maximum-channels="
         << game_audio.maximum_channels() << '\n'
         << "game-audio-profile-scope=all-scene-streams-not-music-cues\n"
         << "soundtrack-global-vorbis-unique-streams=" << game_vorbis.size() << '\n'
         << "soundtrack-duration-comparison-pairs="
         << duration_comparison.compared_pairs << '\n'
         << "soundtrack-duration-exact-pairs="
         << duration_comparison.exact_duration_pairs << '\n'
         << "soundtrack-duration-near-pairs-250ms="
         << duration_comparison.near_duration_pairs << '\n'
         << "soundtrack-duration-only-cue-mapping=unavailable\n";
}

void write_localization_probe(const std::filesystem::path& root,
                              std::ostream& output) {
  const auto archive = off::data::ZipArchive::open(root / "Scenes" / "FF-StartUp.ZIP");
  const auto* entry = archive.find("SCENES/FF-StartUp.LOC");
  if (!entry)
    throw std::runtime_error("startup LOC member is unavailable");
  const auto profile = off::data::LocStringIndex::profile(archive.read(*entry));
  output << "localization-probe=completed\n"
         << "localization-startup-member-bytes=" << profile.member_bytes << '\n'
         << "localization-startup-candidates=" << profile.candidate_count << '\n'
         << "localization-startup-identifier-like-candidates="
         << profile.ascii_identifier_candidate_count << '\n'
         << "localization-startup-candidate-bytes=" << profile.candidate_bytes << '\n'
         << "localization-startup-maximum-candidate-bytes="
         << profile.maximum_candidate_bytes << '\n'
         << "localization-startup-structure-digest=" << profile.structure_digest << '\n'
         << "localization-record-grammar=unavailable\n"
         << "localization-text-decoding=unavailable\n";
}

void write_movie_cut_probe(const std::filesystem::path& root,
                           std::ostream& output) {
  const auto movie_root = root / "Scenes" / "Cutscenes" / "MovieCuts";
  std::error_code error;
  if (!std::filesystem::is_directory(movie_root, error) || error)
    throw std::runtime_error("MovieCuts directory is unavailable");
  std::size_t cut_directories{}, loader_packages{}, main_packages{}, other_packages{};
  for (const auto& entry : std::filesystem::directory_iterator(movie_root, error)) {
    if (error) throw std::runtime_error("MovieCuts directory enumeration failed");
    if (!entry.is_directory() || entry.is_symlink()) continue;
    const auto cut = entry.path().filename().string();
    if (!off::runtime::detail::movie_cut_safe_identifier(cut))
      throw std::runtime_error("MovieCuts directory has an invalid identifier");
    ++cut_directories;
    const auto loader = entry.path() / "Loader.ZIP";
    if (std::filesystem::is_regular_file(loader, error) && !error) {
      const auto package = off::runtime::MovieCutLoaderPackageSource::prepare_checked(cut, loader);
      if (package.cut_identifier() != cut)
        throw std::runtime_error("MovieCut loader identity disagrees with its directory");
      ++loader_packages;
    }
    const auto main_name = cut + "_MAIN";
    const auto main = entry.path() / (main_name + ".ZIP");
    if (std::filesystem::is_regular_file(main, error) && !error) {
      const auto package = off::runtime::MovieCutMainPackageSource::prepare_checked(
          root, cut, main_name);
      if (package.cut_identifier() != cut || package.package_identifier() != main_name)
        throw std::runtime_error("MovieCut main identity disagrees with its directory");
      ++main_packages;
    }
    for (const auto& file : std::filesystem::directory_iterator(entry.path(), error)) {
      if (error) throw std::runtime_error("MovieCut package enumeration failed");
      if (!file.is_regular_file() || file.path().extension() != ".ZIP" ||
          file.path().filename() == "Loader.ZIP" || file.path() == main)
        continue;
      ++other_packages;
    }
  }
  if (cut_directories == 0U || loader_packages == 0U || main_packages == 0U)
    throw std::runtime_error("MovieCuts probe found no complete source packages");
  output << "movie-cut-probe=completed\n"
         << "movie-cut-directories=" << cut_directories << '\n'
         << "movie-cut-verified-loader-packages=" << loader_packages << '\n'
         << "movie-cut-verified-main-packages=" << main_packages << '\n'
         << "movie-cut-other-packages=" << other_packages << '\n'
         << "movie-cut-routing=unavailable\n"
         << "movie-cut-playback=unavailable\n";
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

void write_startup_route_cold_probe(const std::filesystem::path &root,
                                    std::ostream &output) {
  const auto startloader_archive =
      off::data::ZipArchive::open(root / "Scenes" / "StartLoader.ZIP");
  const auto *source_entry =
      startloader_archive.find("SCENES/StartLoader.GMS");
  if (source_entry == nullptr) {
    throw std::runtime_error("StartLoader source is unavailable");
  }
  const auto startloader_gms = off::data::GmsImage::parse(
      off::data::PackedResource::parse(startloader_archive.read(*source_entry)));
  auto route = off::runtime::StartLoaderPreparedRoute::from_checked_gms(
      startloader_gms, root / "Scenes" / "FF-StartUp.ZIP",
      {.initial_update_count = 0U, .one_time_setup_pending = true});
  std::size_t setup_calls{};
  for (std::size_t update = 0; update < 3U; ++update) {
    const auto result = route.ordinary_update([&setup_calls] { ++setup_calls; });
    if ((update < 2U && result !=
                           off::runtime::StartLoaderPreparedRouteResult::awaiting_target) ||
        (update == 2U && result !=
                            off::runtime::StartLoaderPreparedRouteResult::package_prepared)) {
      throw std::runtime_error("StartLoader route did not retain its checked handoff");
    }
  }
  if (setup_calls != 1U || route.update_count() != 3U ||
      route.one_time_setup_pending() || !route.has_prepared_package()) {
    throw std::runtime_error("StartLoader route reached an invalid prepared state");
  }
  const auto &package = *route.prepared_package();
  if (!package.factory_inputs().has_value()) {
    throw std::runtime_error("StartLoader route prepared no typed startup inputs");
  }
  const auto directory = route.prepared_boot_directory_source();
  if (!directory.matches_checked_gms(package.factory_inputs()->gms())) {
    throw std::runtime_error("StartLoader route lost BootMenu source provenance");
  }
  output << "startup-route-cold-probe=completed\n"
         << "startup-route-one-time-setup-calls=" << setup_calls << '\n'
         << "startup-route-updates=" << route.update_count() << '\n'
         << "startup-route-package-prepared=true\n"
         << "startup-route-boot-directory-source=verified\n"
         << "startup-route-scene-construction=unavailable\n"
         << "startup-route-lifecycle=unavailable\n"
         << "startup-route-renderer=unavailable\n"
         << "startup-route-input=unavailable\n";
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
  case Family::vert_anim_component: return "vert-anim-component";
  case Family::lens_flare_component: return "lens-flare-component";
  case Family::matpos_component: return "matpos-component";
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
  constexpr std::size_t family_count=15U;
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
}

int run_first_cut_probe(const std::filesystem::path &data_path, bool run_initialization,
                        bool observe_renderer_payload, bool observe_named_global) {
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
  const auto& bracket_observation=session->reader_bracket_observation();
  if(bracket_observation.external_loader_values!=std::vector<std::uint64_t>{0U,0U} ||
      bracket_observation.source_scripts.size()!=intro.source_script_work().size() ||
      bracket_observation.pre_reader_calls!=1U ||
      bracket_observation.prepared_reader_calls!=intro.deferred_reader_work().size() ||
      bracket_observation.end_reader_calls!=1U)
    throw std::runtime_error("first-cut cold probe found an incomplete reader-bracket observation");
  const auto reader_coverage=intro.reader_coverage_inventory();
  const auto matpos_dispatch=intro.matpos_deferred_dispatch_inventory();
  const auto paramanim_dispatch=intro.paramanim_deferred_dispatch_inventory();
  const auto particle_dispatch=intro.particle_emitter_deferred_dispatch_inventory();
  if(reader_coverage.stage!=off::graphics::IntroReaderBracketStage::ordinary_reader_boundary_complete ||
      reader_coverage.total_discovered!=intro.deferred_reader_work().size())
    throw std::runtime_error("first-cut cold probe found incomplete reader coverage");
  std::size_t paramanim_profiled_owners{};
  for(const auto& shape:paramanim_dispatch.shapes) paramanim_profiled_owners+=shape.count;
  if(paramanim_dispatch.attachment_instances<paramanim_dispatch.attachment_owners ||
      paramanim_dispatch.owners_with_deferred_blocks!=paramanim_profiled_owners)
    throw std::runtime_error("first-cut cold probe found inconsistent ParamAnim structural inventory");
  std::size_t particle_profiled_owners{};
  for(const auto& shape:particle_dispatch.shapes) particle_profiled_owners+=shape.count;
  if(particle_dispatch.attachment_instances<particle_dispatch.attachment_owners ||
      particle_dispatch.owners_with_deferred_blocks!=particle_profiled_owners)
    throw std::runtime_error("first-cut cold probe found inconsistent ParticleEmitter structural inventory");
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
  // MovieControl is the source-backed gate immediately before the recovered
  // first-cut path. Verify its immutable reader boundary here, without
  // inferring a component lifecycle, event enrollment, or renderer state.
  const auto* movie_controller=intro.movie_controller_reader_state();
  const auto* movie_controller_component=intro.movie_controller_component_reader_state();
  const auto controller_source=intro.resources().controller_index();
  if(!movie_controller || !movie_controller_component ||
      movie_controller->owner!=intro.source_handle(controller_source) ||
      controller_source>=mapping.size() ||
      movie_controller->resource!=mapping[controller_source].value_or(
          off::graphics::IntroRuntimeResourceHandle{}) ||
      movie_controller->component_index!=intro.controller_component_index() ||
      movie_controller_component->owner!=movie_controller->owner ||
      movie_controller_component->component_index!=movie_controller->component_index ||
      movie_controller->sequence_list_resource.value==0U ||
      movie_controller->group_list_resource.value==0U)
    throw std::runtime_error("first-cut cold probe found an incomplete MovieControl reader");
  const auto verify_controller_references=[&](std::span<const std::uint32_t> references,
                                              std::span<const std::optional<off::graphics::IntroRuntimeResourceHandle>> translated) {
    if(references.size()!=translated.size()) return false;
    for(std::size_t index=0;index<references.size();++index) {
      if(references[index]==0U) {
        if(translated[index]) return false;
        continue;
      }
      const auto source=intro.resources().sources().local_source_for_authored_reference(
          references[index]);
      if(!source || *source>=mapping.size() || translated[index]!=mapping[*source])
        return false;
    }
    return true;
  };
  if(!verify_controller_references(intro.resources().cut_references(),
                                   movie_controller->sequence_members) ||
      !verify_controller_references(intro.resources().group_references(),
                                    movie_controller->group_members))
    throw std::runtime_error("first-cut cold probe found inconsistent MovieControl references");
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
  auto* first_cut=session->first_cut_player();
  if(!first_cut || first_cut->initialization().phase_one_complete() ||
      first_cut->initialization().phase_two_complete() || first_cut->receiver().open() ||
      first_cut->receiver().closed())
    throw std::runtime_error("first-cut cold probe observed an unexpected lifecycle transition");
  std::optional<std::int32_t> latest_command_position;
  for(const auto& command:first_cut_source_data.commands) {
    const auto position=std::bit_cast<std::int32_t>(command.timeline_position);
    if(position>=0 && (!latest_command_position || position>*latest_command_position))
      latest_command_position=position;
  }
  std::size_t command_delivery_attempts{};
  std::size_t command_component_delivery_attempts{};
  std::size_t expected_command_target_deliveries{};
  if(latest_command_position) {
    const auto* prepared_player=intro.first_cut_player_prepared_state();
    if(!prepared_player || !std::isfinite(prepared_player->sequence_values[1]))
      throw std::runtime_error("first-cut cold probe found no finite command-derived end");
    std::vector<std::uint64_t> source_backed_targets;
    for(const auto& target:intro.first_cut_command_target_provenance())
      source_backed_targets.push_back(target.owner.value);
    for(const auto& command:first_cut_source_data.commands) {
      const auto position=std::bit_cast<std::int32_t>(command.timeline_position);
      const auto event_is_mapped=
          command.event_reference<intro.source_event_name_mapping().size() &&
          intro.source_event_name_mapping()[command.event_reference].has_value() &&
          *intro.source_event_name_mapping()[command.event_reference]!=0U &&
          *intro.source_event_name_mapping()[command.event_reference]<=
              std::numeric_limits<std::uint16_t>::max();
      const auto target_is_source_backed=std::ranges::any_of(
          intro.first_cut_command_target_provenance(),[&](const auto& target) {
            return target.authored_reference==command.target_reference;
          });
      if(position>=0 && event_is_mapped && command.target_reference!=0U &&
          target_is_source_backed)
        ++expected_command_target_deliveries;
    }
    off::cutscene::FirstCutRuntimeCommandRouter command_router{intro};
    off::cutscene::FirstCutCommandSession command_session{
        intro,prepared_player->sequence_values[1],
        command_router.command_session_services({
            .direct_target=[&](std::uint64_t target,std::uint16_t,std::uint32_t,
                               std::uint64_t sender) {
              if(sender!=command_router.sender() ||
                  std::ranges::find(source_backed_targets,target)==source_backed_targets.end())
                throw std::runtime_error("first-cut cold probe found an unbound command target");
              ++command_delivery_attempts;
            },
            .direct_component=[&](std::uint64_t,std::uint64_t,std::uint16_t,
                                  std::uint32_t,std::uint64_t) {
              ++command_component_delivery_attempts;
            }}),
        command_router.sender()};
    const auto sampled_position=std::nextafter(
        static_cast<float>(*latest_command_position),
        std::numeric_limits<float>::infinity());
    command_session.run(sampled_position);
    if(command_delivery_attempts!=expected_command_target_deliveries)
      throw std::runtime_error(
          "first-cut cold probe found incomplete source-backed command delivery");
  }
  const auto tail_readiness=session->outer_loader_tail_readiness();
  const auto tail_inputs=intro.outer_loader_source_inputs();
  const auto& retained_tail_sources=intro.resources().outer_loader_sources();
  const auto same_payload=[](const auto& supplied,const auto& retained) {
    if(supplied.has_value()!=retained.has_value()) return false;
    return !supplied || std::ranges::equal(supplied->bytes,*retained);
  };
  if(!same_payload(tail_inputs.named_global_payload,retained_tail_sources.named_global) ||
      !same_payload(tail_inputs.renderer_resource_payload,
                    retained_tail_sources.renderer_resource) ||
      tail_inputs.resource_associations.size()!=
          retained_tail_sources.resource_associations.size())
    throw std::runtime_error("first-cut cold probe found incomplete loader-tail source inputs");
  for(std::size_t index=0;index<tail_inputs.resource_associations.size();++index) {
    const auto& input=tail_inputs.resource_associations[index];
    const auto& retained=retained_tail_sources.resource_associations[index];
    if(input.first_reference!=retained[0] || input.second_reference!=retained[1])
      throw std::runtime_error("first-cut cold probe found altered loader-tail associations");
  }
  if(tail_readiness.ready_to_run())
      throw std::runtime_error("first-cut cold probe unexpectedly considers the loader tail runnable");
  std::optional<off::graphics::IntroRendererPayloadObservation> renderer_observation;
  if(observe_renderer_payload) {
    const auto& outer_sources=intro.resources().outer_loader_sources();
    if(!outer_sources.renderer_resource)
      throw std::runtime_error("intro renderer payload probe found no retained source payload");
    renderer_observation.emplace(off::graphics::observe_intro_renderer_payload(
        *outer_sources.renderer_resource,[&intro](std::uint32_t reference) {
          return intro.resolve_marked_source_address(reference);
        }));
  }
  std::optional<off::graphics::IntroNamedGlobalSectionProfile> named_global_observation;
  std::optional<off::graphics::IntroNamedGlobalWordPair> named_global_word_pair;
  if(observe_named_global) {
    const auto& outer_sources=intro.resources().outer_loader_sources();
    if(!outer_sources.named_global)
      throw std::runtime_error("intro named/global probe found no retained source payload");
    const auto envelope=off::graphics::parse_intro_named_global_section_envelope(
        *outer_sources.named_global);
    named_global_observation.emplace(
        off::graphics::profile_intro_named_global_section(envelope));
    named_global_word_pair.emplace(
        off::graphics::read_intro_named_global_word_pair(envelope));
  }
  std::optional<off::cutscene::FirstCutPlayerInitializationObservation> initialization_observation;
  std::optional<off::cutscene::FirstCutTimelineProfile> timeline_observation;
  if(run_initialization) {
    const auto* prepared=intro.first_cut_player_prepared_state();
    if(!prepared || prepared->camera_owner.value==0U || prepared->sequence_owner.value==0U ||
       prepared->legal_picture_owner.value==0U || prepared->started.empty() ||
       !std::isfinite(prepared->sequence_values[1]))
      throw std::runtime_error("first-cut initialization probe found incomplete live bindings");
    initialization_observation.emplace(off::cutscene::observe_first_cut_player_initialization(
        *first_cut,{.active_camera_list=prepared->camera_owner.value,
                    .cut_sequence_object=prepared->sequence_owner.value,
                    .member=prepared->legal_picture_owner.value,
                    .member_end=prepared->sequence_values[1],
                    .member_count=prepared->started.size(),
                    .queue_property=prepared->list_resource.value}));
    if(initialization_observation->phase_one_command_invocations!=first_cut_source_data.commands.size() ||
       initialization_observation->phase_two_command_invocations!=first_cut_source_data.commands.size() ||
       !initialization_observation->retained_source_read ||
       !initialization_observation->list_events_registered ||
       !initialization_observation->queue_property_written ||
       !initialization_observation->action_map_setup ||
       !initialization_observation->receiver_open || !initialization_observation->receiver_closed ||
       !initialization_observation->active_camera_list_resolved ||
       !initialization_observation->cut_sequence_object_resolved)
      throw std::runtime_error("first-cut initialization probe observed an incomplete cold lifecycle");
    timeline_observation.emplace(off::cutscene::profile_first_cut_timeline(
        first_cut->receiver().commands()));
  }
  std::cout << "First-cut cold probe verified\n"
            << "reader-states=2\n"
            << "component-payloads=6\n"
            << "owner-envelope=verified\n"
            << "movie-controller-reader=verified\n"
            << "movie-controller-component-reader=verified\n"
            << "first-component-payload=verified\n"
            << "command-component-payloads=5-verified\n"
            << "reader-bracket-external-loader-calls="
            << bracket_observation.external_loader_values.size() << '\n'
            << "reader-bracket-source-script-calls="
            << bracket_observation.source_scripts.size() << '\n'
            << "reader-bracket-pre-reader-calls="
            << bracket_observation.pre_reader_calls << '\n'
            << "reader-bracket-prepared-reader-calls="
            << bracket_observation.prepared_reader_calls << '\n'
            << "reader-bracket-end-reader-calls="
            << bracket_observation.end_reader_calls << '\n'
            << "first-cut-command-delivery-attempts="
            << command_delivery_attempts << '\n'
            << "first-cut-command-target-deliveries-expected="
            << expected_command_target_deliveries << '\n'
            << "first-cut-command-component-delivery-attempts="
            << command_component_delivery_attempts << '\n'
            ;
  write_reader_coverage_probe(std::cout,reader_coverage);
  std::cout << "matpos-deferred-records=" << matpos_dispatch.associated_records << '\n'
            << "matpos-terminal-first=" << matpos_dispatch.terminal_before_first_attachment_delimiter << '\n'
            << "matpos-attachment-first=" << matpos_dispatch.attachment_delimiter_precedes_terminal << '\n'
            << "matpos-attachment-delimiters=" << matpos_dispatch.attachment_delimiters << '\n'
            << "paramanim-attachment-owners=" << paramanim_dispatch.attachment_owners << '\n'
            << "paramanim-attachment-instances=" << paramanim_dispatch.attachment_instances << '\n'
            << "paramanim-owners-with-deferred-blocks=" << paramanim_dispatch.owners_with_deferred_blocks << '\n';
  for(const auto& shape:paramanim_dispatch.shapes) {
    std::cout << "paramanim-structural-shape="
              << "bytes=" << shape.block_bytes
              << " attachments=" << shape.attachment_count
              << " delimiters=" << shape.attachment_delimiters
              << " tags=" << shape.tag_classes[0] << ',' << shape.tag_classes[1]
              << ',' << shape.tag_classes[2] << ',' << shape.tag_classes[3]
              << ',' << shape.tag_classes[4] << ',' << shape.tag_classes[5]
              << " framing-digest=" << shape.framing_digest
              << " count=" << shape.count << '\n';
  }
  std::cout << "particle-emitter-attachment-owners=" << particle_dispatch.attachment_owners << '\n'
            << "particle-emitter-attachment-instances=" << particle_dispatch.attachment_instances << '\n'
            << "particle-emitter-owners-with-deferred-blocks=" << particle_dispatch.owners_with_deferred_blocks << '\n';
  for(const auto& shape:particle_dispatch.shapes) {
    std::cout << "particle-emitter-structural-shape="
              << "bytes=" << shape.block_bytes
              << " attachments=" << shape.attachment_count
              << " delimiters=" << shape.attachment_delimiters
              << " tags=" << shape.tag_classes[0] << ',' << shape.tag_classes[1]
              << ',' << shape.tag_classes[2] << ',' << shape.tag_classes[3]
              << ',' << shape.tag_classes[4] << ',' << shape.tag_classes[5]
              << " framing-digest=" << shape.framing_digest
              << " count=" << shape.count << '\n';
  }
  std::cout
            << "outer-loader-source-inputs=verified\n";
  std::cout << "outer-loader-tail=native-services-required\n"
            << "outer-loader-named-global-bytes=" << tail_readiness.named_global_bytes << '\n'
            << "outer-loader-named-global-native-supported="
            << (tail_readiness.named_global_native_supported ? "yes" : "no") << '\n'
            << "outer-loader-renderer-bytes=" << tail_readiness.renderer_resource_bytes << '\n'
            << "outer-loader-associations=" << tail_readiness.resource_association_count << '\n'
            << "outer-loader-allocation-sizing-rows=" << tail_readiness.allocation_sizing_row_count << '\n';
  for(const auto boundary:tail_readiness.required_boundaries)
    std::cout << "outer-loader-pending="
              << off::graphics::intro_outer_loader_tail_boundary_label(boundary) << '\n';
  if(initialization_observation) {
    std::cout << "first-cut-initialization-probe=completed\n"
              << "phase-one-command-invocations=" << initialization_observation->phase_one_command_invocations << '\n'
              << "phase-two-command-invocations=" << initialization_observation->phase_two_command_invocations << '\n'
              << "ordered-command-registrations=" << initialization_observation->ordered_command_registrations << '\n'
              << "timeline-command-count=" << timeline_observation->command_count << '\n'
              << "timeline-distinct-positions=" << timeline_observation->distinct_positions << '\n'
              << "timeline-final-position=" << timeline_observation->final_position << '\n'
              << "timeline-position-digest=" << timeline_observation->position_digest << '\n'
              << "phase-one=cold-complete\n"
              << "phase-two=cold-complete\n";
  } else {
    std::cout << "phase-one=not-run\n"
              << "phase-two=not-run\n";
  }
  if(renderer_observation) {
    std::cout << "intro-renderer-payload-probe=completed\n"
              << "renderer-relocation-groups=" << renderer_observation->relocation_groups << '\n'
              << "renderer-relocation-references=" << renderer_observation->relocation_references << '\n'
              << "renderer-relocated-references=" << renderer_observation->resolved_references << '\n'
              << "renderer-eight-byte-slots=" << renderer_observation->eight_byte_slots << '\n'
              << "renderer-sixteen-byte-slots=" << renderer_observation->sixteen_byte_slots << '\n'
              << "renderer-trailing-bytes=" << renderer_observation->trailing_bytes << '\n';
  }
  if(named_global_observation) {
    const auto& profile=*named_global_observation;
    std::cout << "intro-named-global-probe=completed\n"
              << "named-global-body-bytes=" << profile.body_bytes << '\n'
              << "named-global-values=" << profile.tagged_values.encoded_values << '\n'
              << "named-global-attachment-delimiters=" << profile.tagged_values.attachment_delimiters << '\n'
              << "named-global-continuation-values=" << profile.tagged_values.continuation_values << '\n'
              << "named-global-framing-digest=" << profile.tagged_values.framing_digest << '\n'
              << "named-global-word-pair="
              << (named_global_word_pair ? "validated" : "unavailable") << '\n';
  }
  std::cout
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

void initialize_private_owned_localization_cache(
    const off::data::InstallVerification& verification) noexcept {
  try {
    const auto cache_root = off::platform::application_deep_audit_cache_root();
    if (cache_root.empty()) return;
    const auto source_set = off::data::verified_owned_loc_source_set(verification.root);
    if (!source_set) return;
    const auto installation_identity = "steam-pc-" + verification.executable_sha256;
    const auto result = off::ui::l10n::ensure_retail_localization_metadata(
        cache_root, installation_identity, off::data::loc_catalog_parser_identity,
        *source_set, [root = verification.root, expected_source_set = *source_set] {
          const auto catalog = off::data::extract_verified_owned_loc_catalog(root);
          if (!catalog || catalog->source_set != expected_source_set) {
            return std::optional<std::vector<off::ui::l10n::RetailSourceString>>{};
          }
          std::vector<off::ui::l10n::RetailSourceString> strings;
          strings.reserve(catalog->values.size());
          for (std::size_t index{}; index < catalog->values.size(); ++index) {
            strings.push_back({static_cast<std::uint64_t>(index), catalog->values[index]});
          }
          return std::optional<std::vector<off::ui::l10n::RetailSourceString>>{
              std::move(strings)};
        });
    if (!result.metadata)
      return;
    const auto binding = off::ui::l10n::translation_source_binding(*result.metadata);
    const auto packs_directory =
        off::platform::application_translation_packs_directory();
    if (!binding || packs_directory.empty())
      return;
    // Optional packs are accepted only after the private cache has established
    // a real text-free binding for this verified installation. The vector is
    // intentionally dormant: no resolver is built, no ID is resolved, and no
    // UI path observes any pack content until native LOC lookup is recovered.
    const auto dormant_packs =
        off::ui::l10n::load_canonical_local_translation_packs(packs_directory,
                                                               *binding);
    static_cast<void>(dormant_packs);
    // Intentionally no text, member name, ordinal, cache path, or pack result
    // is reported. This remains an extraction/enrollment substrate only.
  } catch (const std::exception&) {
    // Localization extraction is optional until the native lookup contract is
    // recovered.  It must never weaken verified game-data startup.
  }
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
  bool diagnostic_intro_picture = false;
  bool probe_startup_boot = false;
  bool probe_startup_route_cold = false;
  bool probe_soundtrack = false;
  bool probe_localization = false;
  bool probe_movie_cuts = false;
  bool probe_first_cut_cold = false;
  bool probe_first_cut_initialization = false;
  bool probe_intro_renderer_payload = false;
  bool probe_intro_named_global = false;
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
    } else if (argument == "--diagnostic-intro-picture") {
      diagnostic_intro_picture = true;
    } else if (argument == "--probe-startup-boot") {
      probe_startup_boot = true;
    } else if (argument == "--probe-startup-route-cold") {
      probe_startup_route_cold = true;
    } else if (argument == "--probe-soundtrack") {
      probe_soundtrack = true;
    } else if (argument == "--probe-localization") {
      probe_localization = true;
    } else if (argument == "--probe-movie-cuts") {
      probe_movie_cuts = true;
    } else if (argument == "--probe-first-cut-cold") {
      probe_first_cut_cold = true;
    } else if (argument == "--probe-first-cut-initialization") {
      probe_first_cut_initialization = true;
    } else if (argument == "--probe-intro-renderer-payload") {
      probe_intro_renderer_payload = true;
    } else if (argument == "--probe-intro-named-global") {
      probe_intro_named_global = true;
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
  if (data_path.empty() && (verify_only || probe_startup_boot || probe_startup_route_cold || probe_soundtrack || probe_localization || probe_movie_cuts || probe_first_cut_cold || probe_first_cut_initialization || probe_intro_renderer_payload || probe_intro_named_global)) {
    std::cerr
        << "A legally purchased Freedom Fighters installation is required; "
           "pass --data PATH or set OPENFREEDOMFIGHTERS_DATA.\n";
    usage(std::cerr);
    return 2;
  }
  if (static_cast<unsigned>(diagnostic_scene) +
          static_cast<unsigned>(diagnostic_startup_graphics) +
          static_cast<unsigned>(diagnostic_intro_picture) >
      1U) {
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
  if ((probe_startup_boot || probe_startup_route_cold) &&
      (verify_only || diagnostic_scene || diagnostic_intro_picture || frame_limit != 0U ||
       show_graphics_menu || !screenshot_path.empty() || !locale.empty() || probe_soundtrack ||
       probe_localization || probe_movie_cuts ||
       mode_specified)) {
    std::cerr
        << "Startup probes cannot be combined with runtime options.\n";
    usage(std::cerr);
    return 2;
  }
  if (probe_soundtrack &&
      (verify_only || probe_startup_boot || probe_startup_route_cold || diagnostic_scene || diagnostic_intro_picture || frame_limit != 0U ||
       show_graphics_menu || !screenshot_path.empty() || !locale.empty() || mode_specified ||
       probe_localization || probe_movie_cuts || probe_first_cut_cold || probe_first_cut_initialization || probe_intro_renderer_payload || probe_intro_named_global)) {
    std::cerr << "Soundtrack probe cannot be combined with runtime options.\n";
    usage(std::cerr);
    return 2;
  }
  if (probe_localization &&
      (verify_only || probe_startup_boot || probe_startup_route_cold || probe_soundtrack || diagnostic_scene || diagnostic_intro_picture || frame_limit != 0U ||
       show_graphics_menu || !screenshot_path.empty() || !locale.empty() || mode_specified ||
       probe_movie_cuts || probe_first_cut_cold || probe_first_cut_initialization || probe_intro_renderer_payload || probe_intro_named_global)) {
    std::cerr << "Localization probe cannot be combined with runtime options.\n";
    usage(std::cerr);
    return 2;
  }
  if (probe_movie_cuts &&
      (verify_only || probe_startup_boot || probe_startup_route_cold || probe_soundtrack || probe_localization || diagnostic_scene || diagnostic_intro_picture || frame_limit != 0U ||
       show_graphics_menu || !screenshot_path.empty() || !locale.empty() || mode_specified ||
       probe_first_cut_cold || probe_first_cut_initialization || probe_intro_renderer_payload || probe_intro_named_global)) {
    std::cerr << "MovieCut probe cannot be combined with runtime options.\n";
    usage(std::cerr);
    return 2;
  }
  if ((probe_first_cut_cold || probe_first_cut_initialization || probe_intro_renderer_payload || probe_intro_named_global) &&
      (verify_only || probe_startup_boot || probe_startup_route_cold || probe_soundtrack || probe_localization || probe_movie_cuts || diagnostic_scene || diagnostic_intro_picture || frame_limit != 0U ||
       show_graphics_menu || !screenshot_path.empty() || !locale.empty() || mode_specified)) {
    std::cerr << "First-cut probes cannot be combined with runtime options.\n";
    usage(std::cerr);
    return 2;
  }
  if (static_cast<unsigned>(probe_first_cut_cold) +
          static_cast<unsigned>(probe_first_cut_initialization) +
          static_cast<unsigned>(probe_intro_renderer_payload) +
          static_cast<unsigned>(probe_intro_named_global) > 1U) {
    std::cerr << "Select only one intro probe.\n";
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

  if (probe_startup_route_cold) {
    const auto verification = off::data::verify_install(
        data_path, {}, {.deep_audit_cache_root =
                             off::platform::application_deep_audit_cache_root()});
    if (!verification) {
      std::cerr << "Game-data verification failed: " << verification.message
                << '\n';
      return 3;
    }
    try {
      write_startup_route_cold_probe(verification.root, std::cout);
      return 0;
    } catch (const std::exception &error) {
      std::cerr << "Startup route cold probe failed: " << error.what() << '\n';
      return 3;
    }
  }

  if (probe_soundtrack) {
    const auto verification = off::data::verify_install(
        data_path, {}, {.deep_audit_cache_root =
                            off::platform::application_deep_audit_cache_root()});
    if (!verification) {
      std::cerr << "Game-data verification failed: " << verification.message << '\n';
      return 3;
    }
    try {
      write_soundtrack_probe(verification, std::cout);
      return 0;
    } catch (const std::exception& error) {
      std::cerr << "Soundtrack probe failed: " << error.what() << '\n';
      return 3;
    }
  }

  if (probe_localization) {
    const auto verification=off::data::verify_install(
        data_path,{}, {.deep_audit_cache_root=off::platform::application_deep_audit_cache_root()});
    if(!verification) {
      std::cerr << "Game-data verification failed: " << verification.message << '\n';
      return 3;
    }
    try {
      write_localization_probe(verification.root, std::cout);
      return 0;
    } catch(const std::exception& error) {
      std::cerr << "Localization probe failed: " << error.what() << '\n';
      return 3;
    }
  }

  if (probe_movie_cuts) {
    const auto verification=off::data::verify_install(
        data_path,{}, {.deep_audit_cache_root=off::platform::application_deep_audit_cache_root()});
    if(!verification) {
      std::cerr << "Game-data verification failed: " << verification.message << '\n';
      return 3;
    }
    try {
      write_movie_cut_probe(verification.root, std::cout);
      return 0;
    } catch(const std::exception& error) {
      std::cerr << "MovieCut probe failed: " << error.what() << '\n';
      return 3;
    }
  }

  if (probe_first_cut_cold || probe_first_cut_initialization || probe_intro_renderer_payload || probe_intro_named_global) {
    const auto verification=off::data::verify_install(
        data_path,{}, {.deep_audit_cache_root=off::platform::application_deep_audit_cache_root()});
    if(!verification) {
      std::cerr << "Game-data verification failed: " << verification.message << '\n';
      return 3;
    }
    try { return run_first_cut_probe(data_path,probe_first_cut_initialization,
                                     probe_intro_renderer_payload,probe_intro_named_global); }
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
  std::optional<off::runtime::StartupBootSceneRegistry> startup_boot_registry;
  std::optional<off::runtime::StartupBootMenuComponentEnvelope>
      startup_boot_menu_envelope;
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
          // Retain the complete checked startup hierarchy through the normal
          // application lifetime. This is source-backed construction only:
          // no component reader, active-root choice, input, camera, rendering
          // or scene transition is admitted here.
          if (!diagnostic_scene && !diagnostic_startup_graphics &&
              !diagnostic_intro_picture) {
            auto package = std::make_shared<const off::runtime::StartupSceneLoadPackage>(
                off::runtime::StartupScenePackageSource::prepare_checked(
                    "FF-Startup", data_path / "Scenes" / "FF-StartUp.ZIP"));
            const auto directory =
                off::runtime::StartupBootSceneDirectorySource::from_checked_gms(
                    package->factory_inputs()->gms());
            // This owns only the native registry transaction. It is not a
            // substitute for the unrecovered original scene-manager lease.
            const auto registry_lifetime =
                std::make_shared<const std::uint8_t>(0U);
            const auto registry_scene =
                off::runtime::StartupBootSceneLease::live(registry_lifetime);
            off::runtime::StartupBootSceneRegistryFactory registry_factory;
            startup_boot_registry.emplace(registry_factory.construct(
                package, directory, registry_scene, 1U));
            if (!startup_boot_registry->valid() ||
                startup_boot_registry->nodes().size() !=
                    directory.hierarchy_scope().nodes.size())
              throw std::runtime_error(
                  "startup hierarchy registry construction was incomplete");
            off::runtime::StartupBootMenuComponentEnvelopeFactory envelope_factory;
            startup_boot_menu_envelope.emplace(envelope_factory.construct(
                std::move(package), directory, *startup_boot_registry));
            if (!startup_boot_menu_envelope->valid())
              throw std::runtime_error(
                  "startup BootMenu source envelope construction was incomplete");
          }
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
                    *intro, *legal_source, {.width = 1280U, .height = 720U},
                    off::graphics::IntroPreviewPolicy::
                        admitted_first_cut_legal_picture));
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
  initialize_private_owned_localization_cache(*verification);
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
  if (!diagnostic_scene && !diagnostic_startup_graphics && !diagnostic_intro_picture)
    std::cout << "Authored startup resources loaded; world rendering pending. "
                 "This is not gameplay or a faithful rendered startup menu.\n";
  if (startup_graphics_cpu_plan)
    std::cout << "Startup menu CPU plan: "
              << startup_graphics_cpu_plan->submissions().size()
              << " source-backed picture submissions; GPU submission pending.\n";
  if (startup_boot_registry)
    std::cout << "Source-backed startup hierarchy retained: "
              << startup_boot_registry->nodes().size()
              << " nodes; menu activation remains pending.\n";
  if (startup_boot_menu_envelope)
    std::cout << "Source-backed BootMenu deferred block retained: "
              << startup_boot_menu_envelope->deferred_source_block().size()
              << " bytes; reader admission remains pending.\n";
  if (diagnostic_startup_graphics)
    std::cout << "Startup graphics diagnostic: source images and source quad "
                 "geometry, generic fit projection; not a faithful menu.\n";
  if (diagnostic_intro_picture)
    std::cout << "Intro picture diagnostic: source image data and source quad "
                 "geometry, generic fit projection; not cutscene playback.\n";
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
              << " referenced images; displayed as a source-backed startup frame, "
                 "not cutscene playback.\n";
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
      startup_window, mode, mode_specified,
      off::platform::application_graphics_settings_path(),
      scene ? &*scene : nullptr, *startup_graphics,
      ui_fonts, ui_textures, intro,
      intro_legal_picture_preflight ? &*intro_legal_picture_preflight : nullptr,
      frame_limit, show_graphics_menu,
      screenshot_path, locale, diagnostic_startup_graphics);
  if (!runtime.success) {
    std::cerr << "Native runtime failed: " << runtime.message << '\n';
    return 4;
  }
  std::cout << runtime.message << '\n';
  return 0;
}
