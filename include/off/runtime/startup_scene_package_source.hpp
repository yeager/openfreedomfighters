#pragma once

#include "off/data/gms_image.hpp"
#include "off/data/packed_resource.hpp"
#include "off/data/scene_support.hpp"
#include "off/data/zip_archive.hpp"
#include "off/runtime/startup_scene_loader.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace off::runtime {
namespace detail {

[[nodiscard]] inline std::string startup_lowercase(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char character) {
                   return static_cast<char>(std::tolower(character));
                 });
  return value;
}

[[nodiscard]] inline const data::ZipEntry &
startup_unique_member(const data::ZipArchive &archive,
                      std::string_view expected_name,
                      std::string_view expected_extension) {
  const data::ZipEntry *selected = archive.find(expected_name);
  std::size_t extension_count{};
  for (const auto &entry : archive.entries()) {
    const auto dot = entry.name.find_last_of('.');
    if (dot != std::string::npos &&
        startup_lowercase(entry.name.substr(dot)) == expected_extension) {
      ++extension_count;
    }
  }
  if (selected == nullptr || extension_count != 1U) {
    throw std::runtime_error(
        "startup scene archive has invalid required members");
  }
  return *selected;
}

struct StartupScenePackageSourceOwner final {
  data::ZipArchive archive;
  std::vector<std::byte> gms_source;
  data::GmsImage gms;
  std::vector<std::byte> support_source;
  data::SceneSupport support;
};

} // namespace detail

// Source-backed preparation for the one manager-admitted target. It validates
// the actual selected ZIP/GMS/SUP input families and uses aliasing shared
// leases so their archive, source bytes, and parsed forms survive the later
// factory attempt. It does not construct a scene or select companion resources.
class StartupScenePackageSource final {
public:
  [[nodiscard]] static StartupSceneLoadPackage
  prepare_checked(std::string_view target,
                  const std::filesystem::path &archive_path) {
    if (target != "FF-Startup") {
      throw std::runtime_error(
          "startup scene package has an unsupported target");
    }

    auto owner = std::make_shared<detail::StartupScenePackageSourceOwner>(
        detail::StartupScenePackageSourceOwner{
            .archive = data::ZipArchive::open(archive_path)});
    const auto &gms_member = detail::startup_unique_member(
        owner->archive, "SCENES/FF-StartUp.GMS", ".gms");
    const auto &support_member = detail::startup_unique_member(
        owner->archive, "SCENES/FF-StartUp.SUP", ".sup");

    owner->gms_source = owner->archive.read(gms_member);
    owner->gms =
        data::GmsImage::parse(data::PackedResource::parse(owner->gms_source));
    owner->support_source = owner->archive.read(support_member);
    owner->support = data::SceneSupport::parse(owner->support_source);
    if (owner->support.dependencies().empty()) {
      throw std::runtime_error(
          "startup scene support input has no dependencies");
    }

    return StartupSceneLoadPackage::complete(
        target,
        std::shared_ptr<const void>(owner, std::addressof(owner->archive)),
        std::shared_ptr<const void>(owner, std::addressof(owner->gms)),
        std::shared_ptr<const void>(owner, std::addressof(owner->support)));
  }
};

} // namespace off::runtime
