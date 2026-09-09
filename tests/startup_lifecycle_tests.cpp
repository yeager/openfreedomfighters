#include "off/graphics/startup_picture_pass_admission.hpp"
#include "off/data/packed_resource.hpp"
#include "off/data/zip_archive.hpp"
#include "off/platform/startup_lifecycle.hpp"
#include "off/runtime/movie_cut_loader_package_source.hpp"
#include "off/runtime/movie_cut_main_package_source.hpp"
#include "off/runtime/startloader_load_screen.hpp"
#include "off/runtime/startloader_prepared_route.hpp"
#include "off/runtime/startup_active_window_root.hpp"
#include "off/runtime/startup_boot_menu_admission.hpp"
#include "off/runtime/startup_boot_scene_construction.hpp"
#include "off/runtime/startup_boot_scene_directory_source.hpp"
#include "off/runtime/startup_boot_scene_factory.hpp"
#include "off/runtime/startup_scene_loader.hpp"
#include "off/runtime/startup_scene_package_source.hpp"
#include "off/runtime/startup_window_hierarchy_snapshot.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include <zlib.h>

namespace {

void check(bool condition, const char *message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
  }
}

void append_u16(std::vector<std::byte> &bytes, std::uint16_t value) {
  bytes.push_back(static_cast<std::byte>(value & 0xffU));
  bytes.push_back(static_cast<std::byte>(value >> 8U));
}

void append_u32(std::vector<std::byte> &bytes, std::uint32_t value) {
  for (unsigned shift = 0; shift < 32; shift += 8U)
    bytes.push_back(static_cast<std::byte>((value >> shift) & 0xffU));
}

void append_text(std::vector<std::byte> &bytes, std::string_view value) {
  const auto characters = std::as_bytes(std::span{value.data(), value.size()});
  bytes.insert(bytes.end(), characters.begin(), characters.end());
}

void set_u32(std::vector<std::byte> &bytes, std::size_t offset,
             std::uint32_t value) {
  for (unsigned shift = 0; shift < 32; shift += 8U)
    bytes[offset + shift / 8U] =
        static_cast<std::byte>((value >> shift) & 0xffU);
}

std::vector<std::byte> package_gms_fixture() {
  std::vector<std::byte> payload(512);
  const auto write = [&](std::size_t offset, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8U)
      payload[offset + shift / 8U] =
          static_cast<std::byte>((value >> shift) & 0xffU);
  };
  write(0, 32);
  write(4, 60);
  write(12, 4);
  write(20, 128);
  write(32, 3);
  write(36, (1U << 24U) | 20U);
  write(40, 7);
  write(44, 84);
  write(48, 0);
  write(52, (1U << 25U) | 84U);
  write(56, 0);
  write(60, 2);
  write(64, 72);
  write(68, 324);
  constexpr char first[] = "first";
  constexpr char second[] = "second";
  std::copy_n(reinterpret_cast<const std::byte *>(first), sizeof(first),
              payload.begin() + 72);
  std::copy_n(reinterpret_cast<const std::byte *>(second), sizeof(second),
              payload.begin() + 324);
  write(80, 32);
  write(84, 384);
  write(88, 420);
  write(96, 0x00100000U);
  write(100, 432);
  write(108, 16);
  write(128, 2);
  write(132, 1);
  write(144, 1);
  write(240, 1);
  write(336, 40);
  write(340, 384);
  write(344, 420);
  write(432, 1);
  write(436, 444);
  for (std::size_t component = 0; component < 9; ++component)
    write(384 + component * 4U,
          std::bit_cast<std::uint32_t>(component % 4U == 0U ? 1.0F : 0.0F));
  write(420, std::bit_cast<std::uint32_t>(10.0F));
  write(424, std::bit_cast<std::uint32_t>(20.0F));
  write(428, std::bit_cast<std::uint32_t>(30.0F));
  write(440, std::bit_cast<std::uint32_t>(2.0F));
  std::vector<std::byte> result;
  append_u32(result, static_cast<std::uint32_t>(payload.size()));
  append_u32(result, static_cast<std::uint32_t>(payload.size() + 9U));
  result.push_back(std::byte{1});
  result.insert(result.end(), payload.begin(), payload.end());
  return result;
}

std::vector<std::byte> boot_directory_gms_fixture() {
  auto bytes = package_gms_fixture();
  constexpr std::size_t payload = 9U;
  set_u32(bytes, payload + 96U, 0x00100031U);
  set_u32(bytes, payload + 436U, 460U);
  set_u32(bytes, payload + 440U, std::bit_cast<std::uint32_t>(1.0F));
  constexpr char identifier[] = "ZWINDOW_BootMenu";
  std::copy_n(reinterpret_cast<const std::byte *>(identifier),
              sizeof(identifier), bytes.begin() +
                                      static_cast<std::ptrdiff_t>(payload + 460U));
  return bytes;
}

std::vector<std::byte> package_support_fixture() {
  constexpr std::string_view dependency = "fixture.dlc";
  const auto size = static_cast<std::uint32_t>(24U + dependency.size() + 1U);
  std::vector<std::byte> result;
  append_u32(result, 0);
  append_u32(result, 0x80000000U | size);
  append_u32(result, size);
  append_u32(result, 1);
  append_u32(result, 0x46434c44U);
  append_u32(result, size - 16U);
  append_text(result, dependency);
  result.push_back(std::byte{0});
  return result;
}

std::vector<std::byte> package_zgf_fixture() {
  std::vector<std::byte> decoded;
  append_u32(decoded, 0x5a474654U);
  append_u32(decoded, 8U);
  std::vector<std::byte> result;
  append_u32(result, static_cast<std::uint32_t>(decoded.size()));
  append_u32(result, static_cast<std::uint32_t>(decoded.size() + 9U));
  result.push_back(std::byte{1});
  result.insert(result.end(), decoded.begin(), decoded.end());
  return result;
}

std::vector<std::byte> package_buf_fixture() {
  // The paired GMS fixture has an auxiliary block at offset 16. Its declared
  // extent is 12 bytes; all names remain bounded by the zeroed allocation.
  std::vector<std::byte> bytes(512U, std::byte{0});
  set_u32(bytes, 20U, 12U);
  return bytes;
}

std::vector<std::pair<std::string, std::vector<std::byte>>>
startup_package_members() {
  return {{"SCENES/FF-StartUp.ZGF", package_zgf_fixture()},
          {"SCENES/FF-StartUp.SUP", package_support_fixture()},
          {"SCENES/FF-StartUp.BUF", package_buf_fixture()},
          {"SCENES/FF-StartUp.GMS", package_gms_fixture()},
          {"SCENES/FF-StartUp.TEX", {std::byte{1}}},
          {"SCENES/FF-StartUp.SND", {std::byte{2}}},
          {"SCENES/FF-StartUp.LOC", {std::byte{3}}},
          {"SCENES/FF-StartUp.OCT", {std::byte{4}}},
          {"SCENES/FF-StartUp.SGP", {std::byte{5}}},
          {"SCENES/FF-StartUp.RMC", {std::byte{6}}},
          {"SCENES/FF-StartUp.RMI", {std::byte{7}}},
          {"SCENES/FF-StartUp.PRM", {std::byte{8}}}};
}

// Project-authored GMS grammar fixture for the checked StartLoader handoff.
// It is not derived from or representative of retail source bytes.
std::vector<std::byte> startloader_route_gms_fixture() {
  constexpr std::size_t envelope_size = 9U;
  std::vector<std::byte> payload(432U);
  const auto write = [&](std::size_t offset, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8U) {
      payload[offset + shift / 8U] =
          static_cast<std::byte>((value >> shift) & 0xffU);
    }
  };
  const auto write_float = [&](std::size_t offset, float value) {
    write(offset, std::bit_cast<std::uint32_t>(value));
  };
  write(0U, 32U);
  write(4U, 96U);
  write(12U, 4U);
  write(20U, 128U);
  write(32U, 1U);
  write(36U, 12U);
  write(96U, 0U);
  write(52U, 324U);
  write(56U, 360U);
  write(64U, 0x0010002eU);
  write(68U, 372U);
  write(80U, 384U);
  write(128U, 2U);
  write(132U, 1U);
  write_float(324U, 1.0F);
  write_float(340U, 1.0F);
  write_float(356U, 1.0F);
  write(372U, 1U);
  write(376U, 100U);
  write_float(380U, 0.0F);
  constexpr std::string_view identifier = "ZWINGROUP_LoadScreen";
  std::copy(identifier.begin(), identifier.end(),
            reinterpret_cast<char *>(payload.data() + 100U));

  std::vector<std::byte> block(4U);
  const auto scalar = [&](std::uint8_t tag, std::uint32_t value) {
    block.push_back(static_cast<std::byte>(tag));
    append_u32(block, value);
  };
  scalar(3U, 0U);
  scalar(2U, std::bit_cast<std::uint32_t>(1.0F));
  scalar(3U, 1U);
  scalar(3U, 1U);
  scalar(3U, 0U);
  block.push_back(std::byte{6});
  block.push_back(std::byte{6});
  block.push_back(std::byte{0x84});
  constexpr std::string_view target = "FF-Startup";
  for (const char character : target)
    block.push_back(static_cast<std::byte>(character));
  block.push_back(std::byte{0});
  block.push_back(std::byte{6});
  block.push_back(std::byte{0xff});
  set_u32(block, 0U, static_cast<std::uint32_t>(block.size()));
  std::copy(block.begin(), block.end(), payload.begin() + 384U);

  std::vector<std::byte> bytes;
  append_u32(bytes, static_cast<std::uint32_t>(payload.size()));
  append_u32(bytes, static_cast<std::uint32_t>(payload.size() + envelope_size));
  bytes.push_back(std::byte{1});
  bytes.insert(bytes.end(), payload.begin(), payload.end());
  return bytes;
}

void write_package_zip(
    const std::filesystem::path &path,
    const std::vector<std::pair<std::string, std::vector<std::byte>>>
        &members) {
  struct Central {
    std::string name;
    std::uint32_t crc, size, offset;
  };
  std::vector<std::byte> bytes;
  std::vector<Central> central;
  for (const auto &[name, contents] : members) {
    const auto crc = static_cast<std::uint32_t>(
        ::crc32(0, reinterpret_cast<const Bytef *>(contents.data()),
                static_cast<uInt>(contents.size())));
    const auto offset = static_cast<std::uint32_t>(bytes.size());
    append_u32(bytes, 0x04034b50U);
    append_u16(bytes, 20);
    append_u16(bytes, 0);
    append_u16(bytes, 0);
    append_u16(bytes, 0);
    append_u16(bytes, 0);
    append_u32(bytes, crc);
    append_u32(bytes, static_cast<std::uint32_t>(contents.size()));
    append_u32(bytes, static_cast<std::uint32_t>(contents.size()));
    append_u16(bytes, static_cast<std::uint16_t>(name.size()));
    append_u16(bytes, 0);
    append_text(bytes, name);
    bytes.insert(bytes.end(), contents.begin(), contents.end());
    central.push_back(
        {name, crc, static_cast<std::uint32_t>(contents.size()), offset});
  }
  const auto central_offset = static_cast<std::uint32_t>(bytes.size());
  for (const auto &entry : central) {
    append_u32(bytes, 0x02014b50U);
    append_u16(bytes, 20);
    append_u16(bytes, 20);
    append_u16(bytes, 0);
    append_u16(bytes, 0);
    append_u16(bytes, 0);
    append_u16(bytes, 0);
    append_u32(bytes, entry.crc);
    append_u32(bytes, entry.size);
    append_u32(bytes, entry.size);
    append_u16(bytes, static_cast<std::uint16_t>(entry.name.size()));
    append_u16(bytes, 0);
    append_u16(bytes, 0);
    append_u16(bytes, 0);
    append_u16(bytes, 0);
    append_u32(bytes, 0);
    append_u32(bytes, entry.offset);
    append_text(bytes, entry.name);
  }
  const auto central_size =
      static_cast<std::uint32_t>(bytes.size()) - central_offset;
  append_u32(bytes, 0x06054b50U);
  append_u16(bytes, 0);
  append_u16(bytes, 0);
  append_u16(bytes, static_cast<std::uint16_t>(central.size()));
  append_u16(bytes, static_cast<std::uint16_t>(central.size()));
  append_u32(bytes, central_size);
  append_u32(bytes, central_offset);
  append_u16(bytes, 0);
  std::ofstream output(path, std::ios::binary);
  output.write(reinterpret_cast<const char *>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
  if (!output)
    throw std::runtime_error("could not write startup package fixture");
}

} // namespace

int main() {
  using namespace std::chrono_literals;
  const off::platform::StartupClock::time_point origin{123s};

  const auto package_fixture =
      std::filesystem::current_path() / "off-startup-scene-package-fixture.zip";
  std::error_code package_error;
  std::filesystem::remove(package_fixture, package_error);
  write_package_zip(package_fixture, startup_package_members());
  bool rejected_package = false;
  try {
    static_cast<void>(off::runtime::StartupScenePackageSource::prepare_checked(
        "other", package_fixture));
  } catch (const std::runtime_error &) {
    rejected_package = true;
  }
  check(rejected_package,
        "package source rejects a non-admitted target before loading");
  auto source_package =
      off::runtime::StartupScenePackageSource::prepare_checked("FF-Startup",
                                                               package_fixture);
  check(source_package.factory_inputs().has_value() &&
            source_package.factory_inputs()->zgf().decoded_size() > 0U &&
            !source_package.factory_inputs()->gms().directory().empty() &&
            source_package.factory_inputs()->buf().size() == 512U,
        "source package exposes only its checked typed ZGF/GMS/BUF factory inputs");
  std::optional<off::runtime::StartupSceneFactoryInputs> retained_factory_inputs;
  {
    auto lifetime_package =
        off::runtime::StartupScenePackageSource::prepare_checked("FF-Startup",
                                                                 package_fixture);
    retained_factory_inputs = lifetime_package.factory_inputs();
  }
  check(retained_factory_inputs.has_value() &&
            retained_factory_inputs->buf().size() == 512U &&
            !retained_factory_inputs->gms().directory().empty(),
        "typed factory inputs retain checked BUF and parsed GMS after package release");
  const auto source_package_once =
      std::make_shared<std::optional<off::runtime::StartupSceneLoadPackage>>(
          std::move(source_package));
  off::runtime::SceneTransitionQueue source_package_queue;
  source_package_queue.request_clear();
  check(source_package_queue.request_target("FF-Startup"),
        "source package fixture queues the admitted target");
  off::runtime::StartupSceneLoadState source_package_state;
  off::runtime::StartupSceneLoader source_package_loader;
  const auto source_lease = [](std::uint64_t value) {
    return std::shared_ptr<const void>(
        std::make_shared<const std::uint64_t>(value));
  };
  check(source_package_loader.consume(
            source_package_queue, source_package_state,
            {.prepare_complete_checked_package =
                 [source_package_once](std::string_view) mutable
                 -> std::optional<off::runtime::StartupSceneLoadPackage> {
               return std::move(*source_package_once);
             },
             .construct_live_scene =
                 [&](const off::runtime::StartupSceneLoadPackage &)
                 -> std::optional<off::runtime::StartupLiveScene> {
               return off::runtime::StartupLiveScene::from_factory(
                   9U, source_lease(9U));
             }}) == off::runtime::StartupSceneLoaderResult::committed &&
            source_package_state.current_scene()->identity() == 9U,
        "package source supplies a durable package compatible with the loader "
        "transaction");
  auto duplicate_startup_members = startup_package_members();
  duplicate_startup_members.emplace_back("SCENES/duplicate.GMS",
                                         package_gms_fixture());
  write_package_zip(package_fixture, duplicate_startup_members);
  rejected_package = false;
  try {
    static_cast<void>(off::runtime::StartupScenePackageSource::prepare_checked(
        "FF-Startup", package_fixture));
  } catch (const std::runtime_error &) {
    rejected_package = true;
  }
  check(rejected_package,
        "package source rejects a duplicate selected GMS family");
  auto bad_buf_members = startup_package_members();
  for (auto &[name, contents] : bad_buf_members) {
    if (name == "SCENES/FF-StartUp.BUF") {
      contents = std::vector<std::byte>(8U, std::byte{0});
      break;
    }
  }
  write_package_zip(package_fixture, bad_buf_members);
  rejected_package = false;
  try {
    static_cast<void>(off::runtime::StartupScenePackageSource::prepare_checked(
        "FF-Startup", package_fixture));
  } catch (const std::runtime_error &) {
    rejected_package = true;
  }
  check(rejected_package,
        "package source rejects a BUF that cannot satisfy paired GMS references");
  std::filesystem::remove(package_fixture, package_error);
  if (const auto *retail_data_root = std::getenv("OFF_STARTUP_PACKAGE_DATA_ROOT");
      retail_data_root != nullptr && *retail_data_root != '\0') {
    const auto retail_startup_package =
        off::runtime::StartupScenePackageSource::prepare_checked(
            "FF-Startup",
            std::filesystem::path(retail_data_root) / "Scenes" / "FF-StartUp.ZIP");
    const auto &retail_factory_inputs = retail_startup_package.factory_inputs();
    check(retail_factory_inputs.has_value() &&
              !retail_factory_inputs->zgf().entries().empty() &&
              !retail_factory_inputs->gms().directory().empty() &&
              !retail_factory_inputs->buf().empty(),
          "retail startup package exposes nonempty checked typed factory inputs");
  }

  const auto startloader_route_root =
      std::filesystem::current_path() / "off-startloader-prepared-route-data";
  std::error_code startloader_route_error;
  std::filesystem::remove_all(startloader_route_root, startloader_route_error);
  const auto startloader_route_package =
      startloader_route_root / "Scenes" / "FF-StartUp.ZIP";
  std::filesystem::create_directories(startloader_route_package.parent_path(),
                                      startloader_route_error);
  check(!startloader_route_error,
        "create checked StartLoader route package fixture directory");
  write_package_zip(startloader_route_package, startup_package_members());
  const auto parsed_startloader = off::data::GmsImage::parse(
      off::data::PackedResource::parse(startloader_route_gms_fixture()));
  bool rejected_route = false;
  try {
    static_cast<void>(off::runtime::StartLoaderPreparedRoute::from_checked_gms(
        parsed_startloader, startloader_route_package,
        {.initial_update_count = 1U, .one_time_setup_pending = true}));
  } catch (const std::runtime_error &) {
    rejected_route = true;
  }
  check(rejected_route,
        "StartLoader prepared route rejects an unreviewed initial counter");
  rejected_route = false;
  try {
    static_cast<void>(off::runtime::StartLoaderPreparedRoute::from_checked_gms(
        parsed_startloader, startloader_route_root / "Scenes" / "other.zip",
        {.initial_update_count = 0U, .one_time_setup_pending = true}));
  } catch (const std::runtime_error &) {
    rejected_route = true;
  }
  check(rejected_route,
        "StartLoader prepared route requires the canonical package name");
  auto prepared_route =
      off::runtime::StartLoaderPreparedRoute::from_checked_gms(
          parsed_startloader, startloader_route_package,
          {.initial_update_count = 0U, .one_time_setup_pending = true});
  bool route_setup_missing = false;
  try {
    static_cast<void>(prepared_route.ordinary_update({}));
  } catch (const std::runtime_error &) {
    route_setup_missing = true;
  }
  check(
      route_setup_missing && prepared_route.update_count() == 0U &&
          prepared_route.one_time_setup_pending() &&
          !prepared_route.has_prepared_package(),
      "StartLoader prepared route preserves its state without explicit setup");
  std::uint32_t route_setup_calls{};
  const auto route_setup = [&] { ++route_setup_calls; };
  check(prepared_route.ordinary_update(route_setup) ==
                off::runtime::StartLoaderPreparedRouteResult::awaiting_target &&
            prepared_route.ordinary_update(route_setup) ==
                off::runtime::StartLoaderPreparedRouteResult::awaiting_target &&
            prepared_route.update_count() == 2U && route_setup_calls == 1U &&
            !prepared_route.has_prepared_package(),
        "StartLoader prepared route retains no package before three updates");
  check(
      prepared_route.ordinary_update(route_setup) ==
              off::runtime::StartLoaderPreparedRouteResult::package_prepared &&
          prepared_route.update_count() == 3U && route_setup_calls == 1U &&
          prepared_route.has_prepared_package() &&
          prepared_route.prepared_package().has_value(),
      "parsed StartLoader source prepares the canonical package on update "
      "three");
  check(
      prepared_route.ordinary_update(route_setup) ==
              off::runtime::StartLoaderPreparedRouteResult::package_prepared &&
          prepared_route.update_count() == 3U,
      "prepared route stops before live-scene construction or later "
      "transitions");
  std::filesystem::remove_all(startloader_route_root, startloader_route_error);

  const auto movie_cut_fixture =
      std::filesystem::current_path() / "off-movie-cut-loader-fixture.zip";
  std::error_code movie_cut_error;
  std::filesystem::remove(movie_cut_fixture, movie_cut_error);

  const auto movie_cut_root =
      std::filesystem::current_path() / "off-movie-cut-main-data";
  std::filesystem::remove_all(movie_cut_root, movie_cut_error);
  constexpr std::string_view main_cut_identifier = "SyntheticCut";
  constexpr std::string_view main_package_identifier = "SyntheticCut_MAIN";
  const auto movie_cut_main_dir = movie_cut_root / "Scenes" / "Cutscenes" /
                                  "MovieCuts" /
                                  std::string(main_cut_identifier);
  std::filesystem::create_directories(movie_cut_main_dir, movie_cut_error);
  check(!movie_cut_error, "create MovieCut main package fixture directory");
  const auto movie_cut_main_archive =
      movie_cut_main_dir / (std::string(main_package_identifier) + ".ZIP");
  const auto main_member = [](std::string_view extension) {
    return "SCENES/Cutscenes/MovieCuts/SyntheticCut/SyntheticCut_MAIN" +
           std::string(extension);
  };
  std::vector<std::pair<std::string, std::vector<std::byte>>> main_members{
      {main_member(".ZGF"), package_zgf_fixture()},
      {main_member(".SUP"), package_support_fixture()},
      {main_member(".BUF"), {std::byte{1}}},
      {main_member(".GMS"), package_gms_fixture()},
      {main_member(".TEX"), {std::byte{2}}},
      {main_member(".SND"), {std::byte{3}}},
      {main_member(".LOC"), {std::byte{4}}},
      {main_member(".OCT"), {std::byte{5}}},
      {main_member(".SGP"), {std::byte{6}}},
      {main_member(".RMC"), {std::byte{7}}},
      {main_member(".RMI"), {std::byte{8}}},
      {main_member(".PRM"), {std::byte{9}}}};
  write_package_zip(movie_cut_main_archive, main_members);
  const auto main_package =
      off::runtime::MovieCutMainPackageSource::prepare_checked(
          movie_cut_root, main_cut_identifier, main_package_identifier);
  check(main_package.cut_identifier() == main_cut_identifier &&
            main_package.package_identifier() == main_package_identifier,
        "MovieCut main package retains caller-selected identifiers");

  main_members.emplace_back(main_member(".ANM"),
                            std::vector<std::byte>{std::byte{10}});
  write_package_zip(movie_cut_main_archive, main_members);
  static_cast<void>(off::runtime::MovieCutMainPackageSource::prepare_checked(
      movie_cut_root, main_cut_identifier, main_package_identifier));

  bool rejected_main_package = false;
  try {
    static_cast<void>(off::runtime::MovieCutMainPackageSource::prepare_checked(
        movie_cut_root, "SyntheticCut/escape", main_package_identifier));
  } catch (const std::runtime_error &) {
    rejected_main_package = true;
  }
  check(rejected_main_package,
        "MovieCut main package rejects path-like cut identifiers");

  main_members.pop_back();
  main_members.pop_back();
  write_package_zip(movie_cut_main_archive, main_members);
  rejected_main_package = false;
  try {
    static_cast<void>(off::runtime::MovieCutMainPackageSource::prepare_checked(
        movie_cut_root, main_cut_identifier, main_package_identifier));
  } catch (const std::runtime_error &) {
    rejected_main_package = true;
  }
  check(rejected_main_package,
        "MovieCut main package rejects a missing required core member");

  main_members.emplace_back(main_member(".PRM"),
                            std::vector<std::byte>{std::byte{9}});
  main_members.emplace_back("SCENES/Cutscenes/MovieCuts/SyntheticCut/extra.bin",
                            std::vector<std::byte>{std::byte{10}});
  write_package_zip(movie_cut_main_archive, main_members);
  rejected_main_package = false;
  try {
    static_cast<void>(off::runtime::MovieCutMainPackageSource::prepare_checked(
        movie_cut_root, main_cut_identifier, main_package_identifier));
  } catch (const std::runtime_error &) {
    rejected_main_package = true;
  }
  check(rejected_main_package,
        "MovieCut main package rejects non-canonical archive members");
  std::filesystem::remove_all(movie_cut_root, movie_cut_error);
  if (const auto *retail_data_root = std::getenv("OFF_MOVIECUT_MAIN_DATA_ROOT");
      retail_data_root != nullptr && *retail_data_root != '\0') {
    const auto retail_main_package =
        off::runtime::MovieCutMainPackageSource::prepare_checked(
            retail_data_root, "FF_MC01", "FF_MC01_MAIN");
    check(retail_main_package.cut_identifier() == "FF_MC01" &&
              retail_main_package.package_identifier() == "FF_MC01_MAIN",
          "MovieCut main package accepts the caller-selected retail source");
  }
  constexpr std::string_view movie_cut_identifier = "SyntheticCut";
  constexpr std::string_view movie_cut_prefix =
      "SCENES/Cutscenes/MovieCuts/SyntheticCut/Loader";
  write_package_zip(
      movie_cut_fixture,
      {{std::string(movie_cut_prefix) + ".GMS", package_gms_fixture()},
       {std::string(movie_cut_prefix) + ".SUP", package_support_fixture()}});
  const auto movie_cut_package =
      off::runtime::MovieCutLoaderPackageSource::prepare_checked(
          movie_cut_identifier, movie_cut_fixture);
  check(movie_cut_package.cut_identifier() == movie_cut_identifier,
        "MovieCut loader package retains its caller-selected identifier");

  bool rejected_movie_cut = false;
  try {
    static_cast<void>(
        off::runtime::MovieCutLoaderPackageSource::prepare_checked(
            "SyntheticCut/escape", movie_cut_fixture));
  } catch (const std::runtime_error &) {
    rejected_movie_cut = true;
  }
  check(rejected_movie_cut,
        "MovieCut loader package rejects path-like identifiers");

  write_package_zip(movie_cut_fixture, {{std::string(movie_cut_prefix) + ".GMS",
                                         package_gms_fixture()}});
  rejected_movie_cut = false;
  try {
    static_cast<void>(
        off::runtime::MovieCutLoaderPackageSource::prepare_checked(
            movie_cut_identifier, movie_cut_fixture));
  } catch (const std::runtime_error &) {
    rejected_movie_cut = true;
  }
  check(rejected_movie_cut,
        "MovieCut loader package rejects a missing support member");

  write_package_zip(
      movie_cut_fixture,
      {{std::string(movie_cut_prefix) + ".GMS", package_gms_fixture()},
       {std::string(movie_cut_prefix) + ".GMS", package_gms_fixture()},
       {std::string(movie_cut_prefix) + ".SUP", package_support_fixture()}});
  rejected_movie_cut = false;
  try {
    static_cast<void>(
        off::runtime::MovieCutLoaderPackageSource::prepare_checked(
            movie_cut_identifier, movie_cut_fixture));
  } catch (const std::runtime_error &) {
    rejected_movie_cut = true;
  }
  check(rejected_movie_cut,
        "MovieCut loader package rejects duplicate required members");
  std::filesystem::remove(movie_cut_fixture, movie_cut_error);

  off::platform::StartupLifecycle ready_early;
  check(ready_early.tick(origin + 20s, true) ==
            off::platform::StartupPhase::awaiting_first_presentation,
        "work completion cannot start the splash deadline");
  ready_early.presented(origin);
  check(ready_early.tick(origin + 2999ms, true) ==
            off::platform::StartupPhase::splash,
        "keep an early result behind the full splash duration");
  check(ready_early.tick(origin + 3s, true) ==
            off::platform::StartupPhase::ready,
        "release an early result at the exact deadline");

  off::platform::StartupLifecycle ready_late;
  ready_late.presented(origin);
  ready_late.presented(origin + 2s);
  check(ready_late.tick(origin + 3s, false) ==
            off::platform::StartupPhase::loading,
        "remove the splash at its deadline while work continues");
  check(ready_late.tick(origin + 4s, true) ==
            off::platform::StartupPhase::ready,
        "release a result completed after the deadline");

  off::platform::StartupLifecycle cancelled;
  cancelled.presented(origin);
  cancelled.cancel();
  check(cancelled.tick(origin + 30s, true) ==
            off::platform::StartupPhase::cancelled,
        "cancellation is terminal");

  off::platform::StartupLifecycle near_clock_limit;
  const auto near_max = off::platform::StartupClock::time_point::max() - 1s;
  near_clock_limit.presented(near_max);
  check(
      near_clock_limit.tick(near_max, true) ==
              off::platform::StartupPhase::splash &&
          near_clock_limit.tick(off::platform::StartupClock::time_point::max(),
                                true) == off::platform::StartupPhase::ready,
      "saturate a deadline that would exceed the clock range");

  bool rejected = false;
  try {
    static_cast<void>(
        off::runtime::StartLoaderLoadScreenSource::from_parsed_attachment(
            "ZWINGROUP_LoadScreen", 0.0F, "FF-StartUp", false));
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  check(rejected, "LoadScreen source requires a caller-proven exact wrapper");
  rejected = false;
  try {
    static_cast<void>(
        off::runtime::StartLoaderLoadScreenSource::from_parsed_attachment(
            "ZWINGROUP_LoadScreen", 0.0F, "FF-StartUp", true));
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  check(rejected, "LoadScreen source rejects a non-supported target");

  const auto source =
      off::runtime::StartLoaderLoadScreenSource::from_parsed_source(
          {.directory_index = 7U, .target = "FF-Startup"});
  check(source.target() == "FF-Startup",
        "typed StartLoader source retains its checked authored target");
  rejected = false;
  try {
    static_cast<void>(
        off::runtime::StartLoaderLoadScreenSource::from_parsed_source(
            {.directory_index = 7U, .target = "OtherScene"}));
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  check(rejected, "typed StartLoader source rejects a non-supported target");
  off::runtime::SceneTransitionQueue transitions;
  transitions.retain_scene_entry(11U);
  transitions.retain_scene_entry(12U);
  transitions.set_current_scene(11U);
  off::runtime::LoadScreenTransition load_screen(source);
  bool setup_missing = false;
  try {
    static_cast<void>(load_screen.ordinary_update(transitions));
  } catch (const std::runtime_error &) {
    setup_missing = true;
  }
  check(setup_missing && load_screen.one_time_setup_pending() &&
            load_screen.update_count() == 0U,
        "a missing setup service preserves LoadScreen state");
  std::uint32_t setup_calls{};
  const auto setup = [&] { ++setup_calls; };
  check(
      !load_screen.ordinary_update(transitions, setup) &&
          !load_screen.ordinary_update(transitions, setup) &&
          setup_calls == 1U && transitions.targets().empty(),
      "LoadScreen setup runs once and the first two updates retain no target");
  check(load_screen.ordinary_update(transitions, setup) && setup_calls == 1U &&
            transitions.clear_requests() == 1U && transitions.pending() &&
            !transitions.current_scene().has_value() &&
            transitions.targets().size() == 1U &&
            transitions.targets().front() == "FF-Startup" &&
            load_screen.retained_target().empty(),
        "third LoadScreen update clears then queues its retained target");
  check(transitions.entries().size() == 2U &&
            transitions.entries()[0].removal_requested &&
            transitions.entries()[1].removal_requested,
        "clear request marks retained entries without consuming them");
  check(!load_screen.ordinary_update(transitions, setup) &&
            transitions.clear_requests() == 2U &&
            transitions.targets().size() == 1U,
        "post-request updates retain native empty-target behavior without "
        "loading a scene");

  off::runtime::SceneTransitionQueue slash_queue;
  check(slash_queue.request_target("Scenes/FF-StartUp") &&
            slash_queue.targets().front() == "Scenes\\FF-StartUp",
        "target requests retain a copied slash-normalized path");

  off::runtime::SceneTransitionQueue pump_queue;
  pump_queue.retain_scene_entry(31U);
  pump_queue.retain_scene_entry(32U);
  pump_queue.set_current_scene(31U);
  pump_queue.request_clear();
  check(pump_queue.request_target("FF-Startup"),
        "pump test retains the supported target");
  off::runtime::SceneTransitionPump pump;
  std::vector<std::string> pump_events;
  off::runtime::StartupSceneLoadState pump_state;
  const auto pump_lease = [](std::uint64_t value) {
    return std::shared_ptr<const void>(
        std::make_shared<const std::uint64_t>(value));
  };
  const off::runtime::SceneTransitionPumpServices pump_services{
      .startup_loader = {
          .prepare_complete_checked_package = [&](std::string_view target)
              -> std::optional<off::runtime::StartupSceneLoadPackage> {
            pump_events.emplace_back("prepare:" + std::string(target));
            return off::runtime::StartupSceneLoadPackage::complete(
                target, pump_lease(31U), pump_lease(32U), pump_lease(33U),
                pump_lease(35U));
          },
          .construct_live_scene =
              [&](const off::runtime::StartupSceneLoadPackage &)
              -> std::optional<off::runtime::StartupLiveScene> {
            // Regression: the old notification pump had already consumed this
            // request before invoking its handoff. Construction must instead
            // observe the still-owned pending request and marked removals.
            check(
                pump_queue.pending() && pump_queue.entries().size() == 2U &&
                    pump_queue.targets() ==
                        std::vector<std::string>{"FF-Startup"},
                "manager pump retains request until the live candidate exists");
            pump_events.emplace_back("factory");
            return off::runtime::StartupLiveScene::from_factory(
                34U, pump_lease(34U));
          },
      },
  };
  check(
      pump.consume(pump_queue, pump_state, pump_services) ==
              off::runtime::SceneTransitionPumpResult::committed &&
          !pump_queue.pending() && pump_queue.entries().empty() &&
          pump_queue.targets().empty() &&
          !pump_queue.current_scene().has_value() &&
          pump_state.current_scene()->identity() == 34U &&
          pump_events ==
              std::vector<std::string>{"prepare:FF-Startup", "factory"},
      "manager pump keeps its request through construction then commits once");

  off::runtime::SceneTransitionQueue rejected_queue;
  rejected_queue.retain_scene_entry(41U);
  rejected_queue.set_current_scene(41U);
  rejected_queue.request_clear();
  check(rejected_queue.request_target("FF-Startup"),
        "rejection test retains the supported target");
  const off::runtime::SceneTransitionPumpServices rejected_services{
      .startup_loader = {
          .prepare_complete_checked_package = [](std::string_view)
              -> std::optional<off::runtime::StartupSceneLoadPackage> {
            return std::nullopt;
          },
          .construct_live_scene =
              [](const off::runtime::StartupSceneLoadPackage &)
              -> std::optional<off::runtime::StartupLiveScene> {
            return std::nullopt;
          },
      },
  };
  check(pump.consume(rejected_queue, pump_state, rejected_services) ==
                off::runtime::SceneTransitionPumpResult::rejected &&
            rejected_queue.pending() && rejected_queue.entries().size() == 1U &&
            rejected_queue.entries().front().removal_requested &&
            rejected_queue.targets() ==
                std::vector<std::string>{"FF-Startup"} &&
            !rejected_queue.current_scene().has_value() &&
            pump_state.current_scene()->identity() == 34U,
        "manager-pump rejection preserves its pending request and prior scene");

  off::runtime::SceneTransitionQueue multiple_targets_queue;
  multiple_targets_queue.request_clear();
  check(multiple_targets_queue.request_target("FF-Startup") &&
            multiple_targets_queue.request_target("FF-Startup"),
        "multiple-target test retains both requests");
  check(pump.consume(multiple_targets_queue, pump_state, pump_services) ==
                off::runtime::SceneTransitionPumpResult::rejected &&
            multiple_targets_queue.pending() &&
            multiple_targets_queue.targets().size() == 2U &&
            pump_state.current_scene()->identity() == 34U,
        "manager pump leaves unsupported target ordering untouched");

  off::runtime::SceneTransitionQueue throwing_queue;
  throwing_queue.retain_scene_entry(43U);
  throwing_queue.request_clear();
  check(throwing_queue.request_target("FF-Startup"),
        "throwing manager-pump test retains target");
  bool pump_threw = false;
  try {
    static_cast<void>(pump.consume(
        throwing_queue, pump_state,
        {.startup_loader = {
             .prepare_complete_checked_package = [](std::string_view)
                 -> std::optional<off::runtime::StartupSceneLoadPackage> {
               throw std::runtime_error("package failed");
             },
             .construct_live_scene =
                 [](const off::runtime::StartupSceneLoadPackage &)
                 -> std::optional<off::runtime::StartupLiveScene> {
               return std::nullopt;
             },
         }}));
  } catch (const std::runtime_error &) {
    pump_threw = true;
  }
  check(pump_threw && !pump.active() && throwing_queue.pending() &&
            throwing_queue.entries().size() == 1U &&
            throwing_queue.targets() ==
                std::vector<std::string>{"FF-Startup"} &&
            pump_state.current_scene()->identity() == 34U,
        "manager-pump exceptions preserve deferred and committed state");

  off::runtime::SceneTransitionQueue reentrant_queue;
  reentrant_queue.request_clear();
  check(reentrant_queue.request_target("FF-Startup"),
        "nonreentrant test retains the supported target");
  bool recursive_call_rejected = false;
  const off::runtime::SceneTransitionPumpServices reentrant_services{
      .startup_loader = {
          .prepare_complete_checked_package = [&](std::string_view)
              -> std::optional<off::runtime::StartupSceneLoadPackage> {
            try {
              static_cast<void>(pump.consume(reentrant_queue, pump_state, {}));
            } catch (const std::runtime_error &) {
              recursive_call_rejected = true;
            }
            return std::nullopt;
          },
          .construct_live_scene =
              [](const off::runtime::StartupSceneLoadPackage &)
              -> std::optional<off::runtime::StartupLiveScene> {
            return std::nullopt;
          },
      },
  };
  check(pump.consume(reentrant_queue, pump_state, reentrant_services) ==
                off::runtime::SceneTransitionPumpResult::rejected &&
            recursive_call_rejected && !pump.active() &&
            reentrant_queue.pending(),
        "pump rejects recursive consumption and resets its active guard");

  const auto lease = [](std::uint64_t value) {
    return std::shared_ptr<const void>(
        std::make_shared<const std::uint64_t>(value));
  };
  const auto package = [&] {
    return off::runtime::StartupSceneLoadPackage::complete(
        "FF-Startup", lease(100U), lease(101U), lease(102U), lease(103U));
  };
  check(!package().factory_inputs().has_value(),
        "generic complete package API does not claim source-checked factory inputs");
  rejected = false;
  try {
    static_cast<void>(off::runtime::StartupSceneLoadPackage::complete(
        "FF-Startup", lease(100U), {}, lease(102U), lease(103U)));
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  check(rejected,
        "startup transaction rejects a package without complete source leases");
  const auto old_scene =
      off::runtime::StartupLiveScene::from_factory(71U, lease(71U));
  off::runtime::StartupSceneLoadState startup_state;
  {
    off::runtime::SceneTransitionQueue initial_queue;
    initial_queue.request_clear();
    check(initial_queue.request_target("FF-Startup"),
          "initial transaction retains supported target");
    off::runtime::StartupSceneLoader initial_loader;
    const off::runtime::StartupSceneLoaderServices initial_services{
        .prepare_complete_checked_package = [&](std::string_view target)
            -> std::optional<off::runtime::StartupSceneLoadPackage> {
          return target == "FF-Startup" ? std::optional{package()}
                                        : std::nullopt;
        },
        .construct_live_scene =
            [&](const off::runtime::StartupSceneLoadPackage &)
            -> std::optional<off::runtime::StartupLiveScene> {
          return old_scene;
        },
    };
    check(initial_loader.consume(initial_queue, startup_state,
                                 initial_services) ==
                  off::runtime::StartupSceneLoaderResult::committed &&
              startup_state.current_scene()->identity() == 71U,
          "transaction commits only a factory-produced live scene token");
  }

  off::runtime::SceneTransitionQueue failed_load_queue;
  failed_load_queue.retain_scene_entry(72U);
  failed_load_queue.request_clear();
  check(failed_load_queue.request_target("FF-Startup"),
        "failed transaction retains supported target");
  off::runtime::StartupSceneLoader loader;
  const off::runtime::StartupSceneLoaderServices package_failure{
      .prepare_complete_checked_package = [](std::string_view)
          -> std::optional<off::runtime::StartupSceneLoadPackage> {
        return std::nullopt;
      },
      .construct_live_scene = [](const off::runtime::StartupSceneLoadPackage &)
          -> std::optional<off::runtime::StartupLiveScene> { std::abort(); },
  };
  check(loader.consume(failed_load_queue, startup_state, package_failure) ==
                off::runtime::StartupSceneLoaderResult::rejected &&
            failed_load_queue.pending() &&
            failed_load_queue.entries().size() == 1U &&
            failed_load_queue.targets() ==
                std::vector<std::string>{"FF-Startup"} &&
            startup_state.current_scene()->identity() == 71U,
        "package preparation failure retains the request and prior committed "
        "scene");

  const off::runtime::StartupSceneLoaderServices factory_failure{
      .prepare_complete_checked_package = [&](std::string_view)
          -> std::optional<off::runtime::StartupSceneLoadPackage> {
        return package();
      },
      .construct_live_scene = [](const off::runtime::StartupSceneLoadPackage &)
          -> std::optional<off::runtime::StartupLiveScene> {
        return std::nullopt;
      },
  };
  check(loader.consume(failed_load_queue, startup_state, factory_failure) ==
                off::runtime::StartupSceneLoaderResult::rejected &&
            failed_load_queue.pending() &&
            startup_state.current_scene()->identity() == 71U,
        "factory failure cannot retire the prior scene or request");

  const off::runtime::StartupSceneLoaderServices throwing_factory{
      .prepare_complete_checked_package = [&](std::string_view)
          -> std::optional<off::runtime::StartupSceneLoadPackage> {
        return package();
      },
      .construct_live_scene = [](const off::runtime::StartupSceneLoadPackage &)
          -> std::optional<off::runtime::StartupLiveScene> {
        throw std::runtime_error("factory failed");
      },
  };
  rejected = false;
  try {
    static_cast<void>(
        loader.consume(failed_load_queue, startup_state, throwing_factory));
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  check(rejected && !loader.active() && failed_load_queue.pending() &&
            startup_state.current_scene()->identity() == 71U,
        "factory exceptions preserve deferred and committed ownership state");

  std::vector<std::string> load_order;
  const off::runtime::StartupSceneLoaderServices committed_loader_services{
      .prepare_complete_checked_package = [&](std::string_view target)
          -> std::optional<off::runtime::StartupSceneLoadPackage> {
        load_order.emplace_back("prepare:" + std::string(target));
        return package();
      },
      .construct_live_scene = [&](const off::runtime::StartupSceneLoadPackage &)
          -> std::optional<off::runtime::StartupLiveScene> {
        load_order.emplace_back("factory");
        return off::runtime::StartupLiveScene::from_factory(73U, lease(73U));
      },
  };
  check(loader.consume(failed_load_queue, startup_state,
                       committed_loader_services) ==
                off::runtime::StartupSceneLoaderResult::committed &&
            !failed_load_queue.pending() &&
            failed_load_queue.entries().empty() &&
            failed_load_queue.targets().empty() &&
            startup_state.current_scene()->identity() == 73U &&
            load_order ==
                std::vector<std::string>{"prepare:FF-Startup", "factory"},
        "successful transaction prepares then constructs before committing "
        "retirement");

  off::runtime::SceneTransitionQueue loader_reentrant_queue;
  loader_reentrant_queue.request_clear();
  check(loader_reentrant_queue.request_target("FF-Startup"),
        "reentrant scene-loader test retains target");
  bool loader_recursive_call_rejected = false;
  const off::runtime::StartupSceneLoaderServices loader_reentrant_services{
      .prepare_complete_checked_package = [&](std::string_view)
          -> std::optional<off::runtime::StartupSceneLoadPackage> {
        try {
          static_cast<void>(
              loader.consume(loader_reentrant_queue, startup_state, {}));
        } catch (const std::runtime_error &) {
          loader_recursive_call_rejected = true;
        }
        return std::nullopt;
      },
      .construct_live_scene = [](const off::runtime::StartupSceneLoadPackage &)
          -> std::optional<off::runtime::StartupLiveScene> {
        return std::nullopt;
      },
  };
  check(loader.consume(loader_reentrant_queue, startup_state,
                       loader_reentrant_services) ==
                off::runtime::StartupSceneLoaderResult::rejected &&
            loader_recursive_call_rejected && !loader.active() &&
            loader_reentrant_queue.pending(),
        "scene-loader rejects reentrant consumption without consuming the "
        "request");

  static_assert(
      !std::is_copy_constructible_v<off::runtime::StartupBootControllerToken>);
  const auto boot_directory_image = off::data::GmsImage::parse(
      off::data::PackedResource::parse(boot_directory_gms_fixture()));
  const auto boot_directory_source =
      off::runtime::StartupBootSceneDirectorySource::from_checked_gms(
          boot_directory_image);
  const auto boot_source_scope = boot_directory_source.hierarchy_scope();
  check(boot_directory_source.proof().complete_directory_mapping &&
            boot_directory_source.proof().canonical_ordinary_window_source &&
            boot_directory_source.proof().component_identifier ==
                "ZWINDOW_BootMenu" &&
            boot_directory_source.proof().component_parameter == 1.0F &&
            boot_directory_source.boot_owner_directory_index() == 0U &&
            boot_source_scope.complete_directory_mapping &&
            boot_source_scope.root_directory_index == 0U &&
            boot_source_scope.nodes.size() == 2U,
        "boot directory source derives only the checked window attachment and "
        "complete retained GMS hierarchy");
  if (const auto *retail_data_root =
          std::getenv("OFF_STARTUP_BOOT_DATA_ROOT");
      retail_data_root != nullptr && *retail_data_root != '\0') {
    const auto startup_archive = off::data::ZipArchive::open(
        std::filesystem::path(retail_data_root) / "Scenes" / "FF-StartUp.ZIP");
    const auto *startup_gms_entry =
        startup_archive.find("SCENES/FF-StartUp.GMS");
    check(startup_gms_entry != nullptr,
          "retail startup archive contains its canonical GMS source");
    const auto retail_boot_directory_image = off::data::GmsImage::parse(
        off::data::PackedResource::parse(startup_archive.read(*startup_gms_entry)));
    const auto retail_boot_directory_source =
        off::runtime::StartupBootSceneDirectorySource::from_checked_gms(
            retail_boot_directory_image);
    const auto retail_boot_scope =
        retail_boot_directory_source.hierarchy_scope();
    const auto &retail_boot_proof = retail_boot_directory_source.proof();
    const auto contains_retail_boot_owner = std::any_of(
        retail_boot_scope.nodes.begin(), retail_boot_scope.nodes.end(),
        [&](const off::runtime::StartupWindowHierarchySourceNode &node) {
          return node.directory_index ==
                 retail_boot_directory_source.boot_owner_directory_index();
        });
    const auto contains_retail_boot_root = std::any_of(
        retail_boot_scope.nodes.begin(), retail_boot_scope.nodes.end(),
        [&](const off::runtime::StartupWindowHierarchySourceNode &node) {
          return node.directory_index == retail_boot_scope.root_directory_index;
        });
    check(retail_boot_proof.complete_directory_mapping &&
              retail_boot_proof.canonical_ordinary_window_source &&
              retail_boot_proof.component_identifier == "ZWINDOW_BootMenu" &&
              retail_boot_proof.component_parameter == 1.0F &&
              retail_boot_directory_source.boot_owner_directory_index() <
                  retail_boot_directory_image.directory().size() &&
              retail_boot_scope.complete_directory_mapping &&
              !retail_boot_scope.nodes.empty() &&
              retail_boot_scope.root_directory_index <
                  retail_boot_directory_image.directory().size() &&
              contains_retail_boot_owner && contains_retail_boot_root,
          "retail startup BootMenu source retains only a valid proof and "
          "hierarchy scope");
  }
  rejected = false;
  try {
    auto malformed_boot_directory = boot_directory_gms_fixture();
    set_u32(malformed_boot_directory, 9U + 440U,
            std::bit_cast<std::uint32_t>(0.0F));
    static_cast<void>(
        off::runtime::StartupBootSceneDirectorySource::from_checked_gms(
            off::data::GmsImage::parse(off::data::PackedResource::parse(
                std::move(malformed_boot_directory)))));
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  check(rejected,
        "boot directory source rejects a non-exact BootMenu parameter");
  rejected = false;
  try {
    auto malformed_boot_directory = boot_directory_gms_fixture();
    set_u32(malformed_boot_directory, 9U + 96U, 0x0010002eU);
    static_cast<void>(
        off::runtime::StartupBootSceneDirectorySource::from_checked_gms(
            off::data::GmsImage::parse(off::data::PackedResource::parse(
                std::move(malformed_boot_directory)))));
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  check(rejected,
        "boot directory source rejects a BootMenu attachment on a non-window "
        "source");
  const auto boot_package =
      std::make_shared<const off::runtime::StartupSceneLoadPackage>(package());
  const auto boot_scene = off::runtime::StartupBootSceneLease::live(lease(81U));
  const off::runtime::StartupBootSceneDirectoryProof boot_directory{
      .complete_directory_mapping = true,
      .canonical_ordinary_window_source = true,
      .component_identifier = "ZWINDOW_BootMenu",
      .component_parameter = 1.0F,
  };
  std::uint64_t attached_owner{};
  float attached_parameter{};
  const off::runtime::StartupBootSceneConstructionServices
      boot_construction_services{
          .registry_live = [] { return true; },
          .allocate_ordinary_window = [] { return 81U; },
          .canonical_live_window_owner =
              [](std::uint64_t owner) { return owner == 81U; },
          .attach_boot_menu_component =
              [&](std::uint64_t owner, float parameter) {
                attached_owner = owner;
                attached_parameter = parameter;
                return 82U;
              },
          .live_boot_menu_component =
              [](std::uint64_t component) { return component == 82U; },
      };
  const auto boot_factory_fixture =
      std::filesystem::current_path() / "off-startup-boot-factory-fixture.zip";
  std::error_code boot_factory_error;
  std::filesystem::remove(boot_factory_fixture, boot_factory_error);
  auto boot_factory_members = startup_package_members();
  for (auto &[name, contents] : boot_factory_members) {
    if (name == "SCENES/FF-StartUp.GMS") {
      contents = boot_directory_gms_fixture();
      break;
    }
  }
  write_package_zip(boot_factory_fixture, boot_factory_members);
  const auto source_backed_boot_package =
      std::make_shared<const off::runtime::StartupSceneLoadPackage>(
          off::runtime::StartupScenePackageSource::prepare_checked(
              "FF-Startup", boot_factory_fixture));
  const auto &source_backed_boot_inputs =
      *source_backed_boot_package->factory_inputs();
  const auto source_backed_boot_directory =
      off::runtime::StartupBootSceneDirectorySource::from_checked_gms(
          source_backed_boot_inputs.gms());
  off::runtime::StartupBootSceneFactory boot_factory;
  auto factory_boot_token = boot_factory.construct(
      source_backed_boot_package, source_backed_boot_directory, boot_scene,
      10U, boot_construction_services);
  check(factory_boot_token.valid() &&
            factory_boot_token.factory_generation() == 10U,
        "boot factory delegates only a source-backed package with matching "
        "checked GMS evidence");

  rejected = false;
  try {
    static_cast<void>(boot_factory.construct(boot_package, boot_directory_source,
                                             boot_scene, 10U,
                                             boot_construction_services));
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  check(rejected, "boot factory rejects generic package leases before factory "
                  "construction");

  const auto mismatch_boot_factory_fixture =
      std::filesystem::current_path() / "off-startup-boot-factory-mismatch.zip";
  std::error_code mismatch_boot_factory_error;
  std::filesystem::remove(mismatch_boot_factory_fixture,
                          mismatch_boot_factory_error);
  write_package_zip(mismatch_boot_factory_fixture, startup_package_members());
  const auto mismatch_boot_package =
      std::make_shared<const off::runtime::StartupSceneLoadPackage>(
          off::runtime::StartupScenePackageSource::prepare_checked(
              "FF-Startup", mismatch_boot_factory_fixture));
  rejected = false;
  try {
    static_cast<void>(boot_factory.construct(
        mismatch_boot_package, source_backed_boot_directory, boot_scene, 10U,
        boot_construction_services));
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  check(rejected, "boot factory rejects a checked package whose GMS does not "
                  "match its BootMenu source evidence");
  std::filesystem::remove(boot_factory_fixture, boot_factory_error);
  std::filesystem::remove(mismatch_boot_factory_fixture,
                          mismatch_boot_factory_error);

  off::runtime::StartupBootSceneConstruction boot_construction;
  auto boot_token = boot_construction.construct(
      boot_package, boot_scene, boot_directory, 9U, boot_construction_services);
  check(boot_token.valid() && boot_token.owner() == 81U &&
            boot_token.component() == 82U &&
            boot_token.factory_generation() == 9U && attached_owner == 81U &&
            attached_parameter == 1.0F,
        "boot construction retains only a checked factory-produced owner and "
        "component");

  rejected = false;
  try {
    auto invalid_boot_directory = boot_directory;
    invalid_boot_directory.component_parameter = 0.0F;
    static_cast<void>(boot_construction.construct(boot_package, boot_scene,
                                                  invalid_boot_directory, 9U,
                                                  boot_construction_services));
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  check(rejected, "boot construction rejects a non-exact component attachment "
                  "before allocation");

  rejected = false;
  try {
    auto missing_component_services = boot_construction_services;
    missing_component_services.live_boot_menu_component = [](std::uint64_t) {
      return false;
    };
    static_cast<void>(boot_construction.construct(boot_package, boot_scene,
                                                  boot_directory, 9U,
                                                  missing_component_services));
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  check(
      rejected,
      "boot construction fails closed when the factory component is not live");

  static_assert(!std::is_copy_constructible_v<
                off::runtime::StartupWindowHierarchySnapshot>);
  const std::array<std::size_t, 4> hierarchy_root_children{11U, 12U, 13U, 14U};
  const std::array<std::size_t, 1> hierarchy_first_container_children{15U};
  const std::array<off::runtime::StartupWindowHierarchySourceNode, 6>
      hierarchy_source{{
          {10U, std::nullopt, hierarchy_root_children},
          {11U, 10U, hierarchy_first_container_children},
          {12U, 10U, {}},
          {13U, 10U, {}},
          {14U, 10U, {}},
          {15U, 11U, {}},
      }};
  const off::runtime::StartupWindowHierarchySourceScope hierarchy_scope{
      true, 10U, hierarchy_source};
  std::unordered_map<std::size_t, off::runtime::StartupFactoryProvenWindowNode>
      live_hierarchy{{
          {10U,
           {100U, 10U, std::nullopt,
            off::runtime::StartupWindowNodeFamily::container, 103U,
            std::nullopt}},
          {11U,
           {101U, 11U, 100U, off::runtime::StartupWindowNodeFamily::container,
            105U, 102U}},
          {12U,
           {102U, 12U, 100U, off::runtime::StartupWindowNodeFamily::leaf,
            std::nullopt, 104U}},
          {13U,
           {103U, 13U, 100U, off::runtime::StartupWindowNodeFamily::container,
            std::nullopt, 101U}},
          {14U,
           {104U, 14U, 100U, off::runtime::StartupWindowNodeFamily::leaf,
            std::nullopt, std::nullopt}},
          {15U,
           {105U, 15U, 101U, off::runtime::StartupWindowNodeFamily::leaf,
            std::nullopt, std::nullopt}},
      }};
  std::uint32_t guard_begins{};
  std::uint32_t guard_ends{};
  std::uint64_t observed_guard{};
  std::uint64_t hierarchy_epoch = 71U;
  const off::runtime::StartupWindowHierarchySnapshotServices hierarchy_services{
      .begin_read_guard = [&]() -> std::optional<std::uint64_t> {
        ++guard_begins;
        return 91U;
      },
      .end_read_guard =
          [&](std::uint64_t guard) {
            ++guard_ends;
            observed_guard = guard;
          },
      .hierarchy_epoch = [&] { return hierarchy_epoch; },
      .factory_generation_live =
          [](std::uint64_t generation) { return generation == 9U; },
      .read_factory_proven_node = [&](std::size_t source)
          -> std::optional<off::runtime::StartupFactoryProvenWindowNode> {
        const auto found = live_hierarchy.find(source);
        return found == live_hierarchy.end()
                   ? std::nullopt
                   : std::optional<
                         off::runtime::StartupFactoryProvenWindowNode>{
                         found->second};
      },
  };
  const auto hierarchy_lease =
      off::runtime::StartupWindowHierarchyLease::live(lease(100U));
  off::runtime::StartupWindowHierarchyFactory hierarchy_factory;
  auto hierarchy_snapshot = hierarchy_factory.capture(
      hierarchy_lease, 9U, hierarchy_scope, hierarchy_services);
  check(
      hierarchy_snapshot.valid() && hierarchy_snapshot.bound_to(9U, 71U) &&
          hierarchy_snapshot.root_identity() == 100U &&
          hierarchy_snapshot.construction_preorder() ==
              std::vector<std::uint64_t>{100U, 103U, 101U, 105U, 102U, 104U} &&
          guard_begins == 1U && guard_ends == 1U && observed_guard == 91U,
      "factory-proven startup hierarchy snapshots reverse containers, retain "
      "leaves and preorder links");

  rejected = false;
  try {
    auto source_only_services = hierarchy_services;
    source_only_services.read_factory_proven_node = [](std::size_t)
        -> std::optional<off::runtime::StartupFactoryProvenWindowNode> {
      return std::nullopt;
    };
    static_cast<void>(hierarchy_factory.capture(
        hierarchy_lease, 9U, hierarchy_scope, source_only_services));
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  check(
      rejected,
      "startup hierarchy rejects source-only nodes without factory provenance");

  rejected = false;
  try {
    auto cyclic = live_hierarchy;
    cyclic.at(14U).next_sibling_identity = 103U;
    auto cycle_services = hierarchy_services;
    cycle_services.read_factory_proven_node = [&](std::size_t source)
        -> std::optional<off::runtime::StartupFactoryProvenWindowNode> {
      const auto found = cyclic.find(source);
      return found == cyclic.end()
                 ? std::nullopt
                 : std::optional<off::runtime::StartupFactoryProvenWindowNode>{
                       found->second};
    };
    static_cast<void>(hierarchy_factory.capture(
        hierarchy_lease, 9U, hierarchy_scope, cycle_services));
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  check(rejected, "startup hierarchy rejects a cyclic or policy-invalid live "
                  "sibling chain");

  rejected = false;
  try {
    const std::array<off::runtime::StartupWindowHierarchySourceNode, 5>
        partial_source{{
            {10U, std::nullopt, hierarchy_root_children},
            {11U, 10U, hierarchy_first_container_children},
            {12U, 10U, {}},
            {13U, 10U, {}},
            {14U, 10U, {}},
        }};
    static_cast<void>(hierarchy_factory.capture(
        hierarchy_lease, 9U, {true, 10U, partial_source}, hierarchy_services));
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  check(rejected, "startup hierarchy rejects a partial source scope");

  rejected = false;
  try {
    std::uint32_t reads{};
    auto changed_epoch_services = hierarchy_services;
    changed_epoch_services.hierarchy_epoch = [&] {
      return reads++ == 0U ? 71U : 72U;
    };
    static_cast<void>(hierarchy_factory.capture(
        hierarchy_lease, 9U, hierarchy_scope, changed_epoch_services));
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  check(rejected, "startup hierarchy invalidates a snapshot when the guarded "
                  "epoch changes");

  static_assert(!std::is_copy_constructible_v<
                off::runtime::StartupActiveWindowRootToken>);
  off::runtime::StartupActiveWindowRootProvider active_root_provider;
  const off::runtime::StartupActiveWindowRootServices active_root_services{
      .selected_root =
          []() -> std::optional<off::runtime::StartupManagerSelectedRoot> {
        return {{.scene_lease_identity = 700U,
                 .root_identity = 103U,
                 .pass_context_identity = 701U}};
      },
      .scene_lease_live =
          [](std::uint64_t identity) { return identity == 700U; },
      .factory_generation_live =
          [](std::uint64_t generation) { return generation == 9U; },
      .hierarchy_epoch = [] { return 71U; },
  };
  const auto active_root = active_root_provider.admit(
      lease(700U), hierarchy_snapshot, active_root_services);
  check(active_root.valid() && active_root.selection().root_identity == 103U &&
            active_root.selection().pass_context_identity == 701U,
        "startup root requires a manager-selected live hierarchy member");

  rejected = false;
  try {
    auto outside_root_services = active_root_services;
    outside_root_services.selected_root =
        []() -> std::optional<off::runtime::StartupManagerSelectedRoot> {
      return {{.scene_lease_identity = 700U,
               .root_identity = 999U,
               .pass_context_identity = 701U}};
    };
    static_cast<void>(active_root_provider.admit(
        lease(700U), hierarchy_snapshot, outside_root_services));
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  check(rejected, "startup root rejects a directory or subtree stand-in "
                  "outside live hierarchy");

  rejected = false;
  try {
    auto stale_root_services = active_root_services;
    stale_root_services.hierarchy_epoch = [] { return 72U; };
    static_cast<void>(active_root_provider.admit(
        lease(700U), hierarchy_snapshot, stale_root_services));
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  check(rejected, "startup root rejects a changed hierarchy epoch");

  static_assert(
      !std::is_copy_constructible_v<off::graphics::StartupActivePassSnapshot>);
  off::graphics::StartupActivePassSnapshotProvider active_pass_provider;
  const off::graphics::StartupActivePassSnapshotServices active_pass_services{
      .read_selected_pass = []()
          -> std::optional<off::graphics::StartupCoordinatorPassSelection> {
        return {{.scene_lease_identity = 700U,
                 .root_identity = 103U,
                 .camera_identity = 702U,
                 .view_identity = 703U,
                 .pass_context_identity = 701U,
                 .coordinator_epoch = 81U,
                 .camera_enabled = true,
                 .normalized_viewport = {0, 0, 1, 1},
                 .owner_projection_scalar = 1.0F,
                 .external_y_basis_scale = 1.0F,
                 .rectangle = {0, 0, 640, 480}}};
      },
      .scene_lease_live =
          [](std::uint64_t identity) { return identity == 700U; },
      .factory_generation_live =
          [](std::uint64_t generation) { return generation == 9U; },
      .hierarchy_epoch = [] { return 71U; },
      .coordinator_epoch = [] { return 81U; },
  };
  const auto active_pass = active_pass_provider.capture(
      lease(700U), hierarchy_snapshot, active_pass_services);
  check(active_pass.valid() && active_pass.bound_to(9U, 71U, 81U) &&
            active_pass.selection().view_identity == 703U,
        "startup active pass keeps root, camera, view and context from one "
        "coordinator read");

  rejected = false;
  try {
    auto stale_pass_services = active_pass_services;
    stale_pass_services.coordinator_epoch = [] { return 82U; };
    static_cast<void>(active_pass_provider.capture(
        lease(700U), hierarchy_snapshot, stale_pass_services));
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  check(rejected, "startup active pass rejects a changed coordinator epoch");

  rejected = false;
  try {
    auto disabled_pass_services = active_pass_services;
    disabled_pass_services.read_selected_pass =
        []() -> std::optional<off::graphics::StartupCoordinatorPassSelection> {
      return {{.scene_lease_identity = 700U,
               .root_identity = 103U,
               .camera_identity = 702U,
               .view_identity = 703U,
               .pass_context_identity = 701U,
               .coordinator_epoch = 81U,
               .camera_enabled = false,
               .normalized_viewport = {0, 0, 1, 1},
               .owner_projection_scalar = 1.0F,
               .external_y_basis_scale = 1.0F,
               .rectangle = {0, 0, 640, 480}}};
    };
    static_cast<void>(active_pass_provider.capture(
        lease(700U), hierarchy_snapshot, disabled_pass_services));
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  check(rejected, "startup active pass rejects an unadmitted camera before "
                  "picture submission");

  static_assert(
      !std::is_copy_constructible_v<off::runtime::StartupBootMenuReaderToken>);
  off::runtime::StartupBootMenuAdmission boot_menu;
  std::uint32_t resolve_calls{};
  std::vector<std::string> lifecycle_order;
  const off::runtime::StartupBootMenuReaderServices reader_services{
      .event_registry_live = [] { return true; },
      .resolve_identity =
          [&](std::uint64_t identity) -> std::optional<std::uint16_t> {
        ++resolve_calls;
        return identity == 101U ? std::optional<std::uint16_t>{31U}
                                : std::nullopt;
      },
      .live_window_owner = [](std::uint64_t owner) { return owner == 81U; },
      .live_boot_menu_component =
          [](std::uint64_t component) { return component == 82U; },
      .common_component_reader =
          [&](std::uint64_t owner, std::uint64_t component) {
            lifecycle_order.push_back("common-reader");
            return owner == 81U && component == 82U;
          },
  };
  auto reader_token =
      boot_menu.read_component(std::move(boot_token), 101U, reader_services);
  check(boot_menu.reader_complete() && !boot_menu.initialized() &&
            !boot_menu.failed() && reader_token.valid() &&
            resolve_calls == 1U && boot_menu.reader_id() == 31U &&
            lifecycle_order.size() == 1U &&
            lifecycle_order[0] == "common-reader",
        "boot-menu reader resolves only its first opaque identity before the "
        "common reader");

  std::uint64_t initialized_owner{};
  std::uint64_t initialized_component{};
  std::uint16_t initialized_route{};
  const off::runtime::StartupBootMenuInitializationServices
      initialization_services{
          .event_registry_live = [] { return true; },
          .resolve_identity =
              [&](std::uint64_t identity) -> std::optional<std::uint16_t> {
            ++resolve_calls;
            lifecycle_order.push_back("second-lookup");
            return identity == 102U ? std::optional<std::uint16_t>{32U}
                                    : std::nullopt;
          },
          .live_window_owner = [](std::uint64_t owner) { return owner == 81U; },
          .live_boot_menu_component =
              [](std::uint64_t component) { return component == 82U; },
          .common_window_initialization =
              [&](std::uint64_t owner, std::uint64_t component) {
                lifecycle_order.push_back("common-initialization");
                return owner == 81U && component == 82U;
              },
          .route_retained_object =
              [&](std::uint64_t owner, std::uint64_t component,
                  std::uint16_t route) {
                lifecycle_order.push_back("retained-route");
                initialized_owner = owner;
                initialized_component = component;
                initialized_route = route;
                return true;
              },
      };
  boot_menu.initialize_component(std::move(reader_token), 102U,
                                 initialization_services);
  check(boot_menu.initialized() && !boot_menu.failed() && resolve_calls == 2U &&
            initialized_owner == 81U && initialized_component == 82U &&
            initialized_route == 32U && boot_menu.reader_id() == 31U &&
            boot_menu.routing_id() == 32U && lifecycle_order.size() == 4U &&
            lifecycle_order[1] == "common-initialization" &&
            lifecycle_order[2] == "second-lookup" &&
            lifecycle_order[3] == "retained-route",
        "boot-menu initialization runs common initialization, second opaque "
        "lookup, and retained routing in order");

  off::runtime::StartupBootMenuAdmission missing_registry;
  auto missing_token =
      boot_construction.construct(boot_package, boot_scene, boot_directory, 10U,
                                  boot_construction_services);
  auto unavailable_registry_services = reader_services;
  unavailable_registry_services.event_registry_live = [] { return false; };
  rejected = false;
  try {
    static_cast<void>(missing_registry.read_component(
        std::move(missing_token), 101U, unavailable_registry_services));
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  check(rejected && missing_registry.failed() &&
            !missing_registry.reader_complete() &&
            !missing_registry.initialized() &&
            missing_registry.reader_id() == 0U,
        "boot-menu reader fails closed without exposing an identity when the "
        "registry is absent");

  off::runtime::StartupBootMenuAdmission routing_failure;
  auto routing_token =
      boot_construction.construct(boot_package, boot_scene, boot_directory, 11U,
                                  boot_construction_services);
  auto routing_reader = routing_failure.read_component(std::move(routing_token),
                                                       101U, reader_services);
  auto failed_routing_services = initialization_services;
  failed_routing_services.route_retained_object =
      [](std::uint64_t, std::uint64_t, std::uint16_t) { return false; };
  rejected = false;
  try {
    routing_failure.initialize_component(std::move(routing_reader), 102U,
                                         failed_routing_services);
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  check(rejected && routing_failure.failed() &&
            !routing_failure.initialized() &&
            routing_failure.reader_id() == 0U &&
            routing_failure.routing_id() == 0U,
        "boot-menu initialization does not expose partial state when retained "
        "routing fails");

  // These focused checks model the independently recovered call boundaries:
  // first identity storage precedes the common reader; common initialization
  // precedes the second resolve; and the success latch follows retained routing.
  const auto new_boot_token = [&] {
    return boot_construction.construct(boot_package, boot_scene, boot_directory,
                                       20U, boot_construction_services);
  };

  off::runtime::StartupBootMenuAdmission store_before_common;
  bool reader_id_stored_before_common{};
  auto store_reader_services = reader_services;
  store_reader_services.resolve_identity =
      [](std::uint64_t key) -> std::optional<std::uint16_t> {
    return key == 201U ? std::optional<std::uint16_t>{41U} : std::nullopt;
  };
  store_reader_services.common_component_reader =
      [&](std::uint64_t, std::uint64_t) {
        reader_id_stored_before_common = store_before_common.reader_id() == 41U;
        return true;
      };
  auto store_reader_token = store_before_common.read_component(
      new_boot_token(), 201U, store_reader_services);
  check(store_reader_token.valid() && reader_id_stored_before_common,
        "boot-menu reader stores its first identity before the common reader");

  off::runtime::StartupBootMenuAdmission equality_allowed;
  auto equality_reader_services = reader_services;
  equality_reader_services.resolve_identity =
      [](std::uint64_t key) -> std::optional<std::uint16_t> {
    return key == 202U ? std::optional<std::uint16_t>{52U} : std::nullopt;
  };
  auto equality_reader_token = equality_allowed.read_component(
      new_boot_token(), 202U, equality_reader_services);
  auto equality_initialization_services = initialization_services;
  equality_initialization_services.resolve_identity =
      [](std::uint64_t key) -> std::optional<std::uint16_t> {
    return key == 203U ? std::optional<std::uint16_t>{52U} : std::nullopt;
  };
  equality_initialization_services.route_retained_object =
      [](std::uint64_t, std::uint64_t, std::uint16_t route) {
        return route == 52U;
      };
  equality_allowed.initialize_component(std::move(equality_reader_token), 203U,
                                        equality_initialization_services);
  check(equality_allowed.initialized() && equality_allowed.reader_id() == 52U &&
            equality_allowed.routing_id() == 52U,
        "boot-menu permits equal opaque reader and routing identities");

  off::runtime::StartupBootMenuAdmission latch_after_route;
  auto latch_reader_services = reader_services;
  latch_reader_services.resolve_identity =
      [](std::uint64_t key) -> std::optional<std::uint16_t> {
    return key == 204U ? std::optional<std::uint16_t>{61U} : std::nullopt;
  };
  auto latch_reader_token = latch_after_route.read_component(
      new_boot_token(), 204U, latch_reader_services);
  bool unlatch_seen_during_route{};
  auto latch_initialization_services = initialization_services;
  latch_initialization_services.resolve_identity =
      [](std::uint64_t key) -> std::optional<std::uint16_t> {
    return key == 205U ? std::optional<std::uint16_t>{62U} : std::nullopt;
  };
  latch_initialization_services.route_retained_object =
      [&](std::uint64_t, std::uint64_t, std::uint16_t route) {
        unlatch_seen_during_route = route == 62U && !latch_after_route.initialized() &&
                                    latch_after_route.routing_id() == 0U;
        return true;
      };
  latch_after_route.initialize_component(std::move(latch_reader_token), 205U,
                                         latch_initialization_services);
  check(unlatch_seen_during_route && latch_after_route.initialized() &&
            latch_after_route.routing_id() == 62U,
        "boot-menu success latch is set only after retained routing succeeds");

  off::runtime::StartupBootMenuAdmission reader_failure_order;
  std::vector<std::string> reader_failure_order_calls;
  auto failed_first_reader_services = reader_services;
  failed_first_reader_services.resolve_identity =
      [&](std::uint64_t) -> std::optional<std::uint16_t> {
    reader_failure_order_calls.push_back("first-resolve");
    return std::nullopt;
  };
  failed_first_reader_services.common_component_reader =
      [&](std::uint64_t, std::uint64_t) {
        reader_failure_order_calls.push_back("common-reader");
        return true;
      };
  rejected = false;
  try {
    static_cast<void>(reader_failure_order.read_component(
        new_boot_token(), 206U, failed_first_reader_services));
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  check(rejected && reader_failure_order.failed() &&
            reader_failure_order_calls == std::vector<std::string>{"first-resolve"},
        "boot-menu reader does not enter its common reader after first resolve failure");

  off::runtime::StartupBootMenuAdmission common_initialization_failure;
  auto common_failure_reader = common_initialization_failure.read_component(
      new_boot_token(), 101U, reader_services);
  std::vector<std::string> common_initialization_failure_calls;
  auto failed_common_initialization_services = initialization_services;
  failed_common_initialization_services.common_window_initialization =
      [&](std::uint64_t, std::uint64_t) {
        common_initialization_failure_calls.push_back("common-initialization");
        return false;
      };
  failed_common_initialization_services.resolve_identity =
      [&](std::uint64_t) -> std::optional<std::uint16_t> {
    common_initialization_failure_calls.push_back("second-resolve");
    return std::optional<std::uint16_t>{71U};
  };
  failed_common_initialization_services.route_retained_object =
      [&](std::uint64_t, std::uint64_t, std::uint16_t) {
        common_initialization_failure_calls.push_back("retained-route");
        return true;
      };
  rejected = false;
  try {
    common_initialization_failure.initialize_component(
        std::move(common_failure_reader), 207U,
        failed_common_initialization_services);
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  check(rejected && common_initialization_failure.failed() &&
            common_initialization_failure_calls ==
                std::vector<std::string>{"common-initialization"},
        "boot-menu initialization stops before second resolve when common initialization fails");

  off::runtime::StartupBootMenuAdmission second_resolve_failure;
  auto second_resolve_reader = second_resolve_failure.read_component(
      new_boot_token(), 101U, reader_services);
  std::vector<std::string> second_resolve_failure_calls;
  auto failed_second_resolve_services = initialization_services;
  failed_second_resolve_services.common_window_initialization =
      [&](std::uint64_t, std::uint64_t) {
        second_resolve_failure_calls.push_back("common-initialization");
        return true;
      };
  failed_second_resolve_services.resolve_identity =
      [&](std::uint64_t) -> std::optional<std::uint16_t> {
    second_resolve_failure_calls.push_back("second-resolve");
    return std::nullopt;
  };
  failed_second_resolve_services.route_retained_object =
      [&](std::uint64_t, std::uint64_t, std::uint16_t) {
        second_resolve_failure_calls.push_back("retained-route");
        return true;
      };
  rejected = false;
  try {
    second_resolve_failure.initialize_component(
        std::move(second_resolve_reader), 208U, failed_second_resolve_services);
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  check(rejected && second_resolve_failure.failed() &&
            second_resolve_failure_calls == std::vector<std::string>{
                                                 "common-initialization",
                                                 "second-resolve"},
        "boot-menu initialization does not route after second resolve failure");

  std::cout << "startup lifecycle tests passed\n";
}
