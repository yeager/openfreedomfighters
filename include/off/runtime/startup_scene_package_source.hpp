#pragma once

#include "off/data/gms_image.hpp"
#include "off/data/packed_resource.hpp"
#include "off/data/scene_support.hpp"
#include "off/data/zgf_bundle.hpp"
#include "off/data/zip_archive.hpp"
#include "off/runtime/startup_scene_loader.hpp"

#include <algorithm>
#include <filesystem>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace off::runtime {
namespace detail {

// This loader receives a path after installation verification.  Keep the
// source boundary fail-closed as well: a later package preparation must not
// silently follow a replacement symlink or accept a directory as an archive.
// ZipArchive owns an in-memory snapshot after this check; this guard only
// admits the filesystem object selected for that snapshot and exposes no path
// detail to callers.
inline void require_regular_startup_archive(
    const std::filesystem::path &archive_path) {
  std::error_code error;
  const auto status = std::filesystem::symlink_status(archive_path, error);
  if (error || std::filesystem::is_symlink(status) ||
      !std::filesystem::is_regular_file(status)) {
    throw std::runtime_error("startup scene archive source is unavailable");
  }
}

[[nodiscard]] inline const data::ZipEntry &startup_exact_member(
    const data::ZipArchive &archive, std::string_view expected_name) {
  const auto count = static_cast<std::size_t>(std::count_if(
      archive.entries().begin(), archive.entries().end(),
      [&](const data::ZipEntry &entry) { return entry.name == expected_name; }));
  const auto *selected = archive.find(expected_name);
  if (count != 1U || selected == nullptr) {
    throw std::runtime_error("startup scene archive has invalid required members");
  }
  return *selected;
}

struct StartupScenePackageSourceOwner final {
  data::ZipArchive archive;
  std::vector<std::byte> zgf_source;
  data::ZgfBundle zgf;
  std::vector<std::byte> gms_source;
  data::GmsImage gms;
  std::vector<std::byte> support_source;
  data::SceneSupport support;
  std::vector<std::byte> buf_source;
  // Remaining named members are deliberately retained as opaque source bytes.
  // Their per-format parsers and runtime roles are separate reconstruction work.
  std::vector<std::vector<std::byte>> raw_sources;
};

} // namespace detail

// Source-backed preparation for the one manager-admitted target. It validates
// the canonical selected ZIP input family and uses aliasing shared leases so its
// archive, parsed ZGF/GMS/SUP forms, paired BUF and opaque companions survive a
// later factory attempt. It does not construct a scene or select resources.
class StartupScenePackageSource final {
public:
  [[nodiscard]] static StartupSceneLoadPackage
  prepare_checked(std::string_view target,
                  const std::filesystem::path &archive_path) {
    if (target != "FF-Startup") {
      throw std::runtime_error(
          "startup scene package has an unsupported target");
    }

    detail::require_regular_startup_archive(archive_path);

    auto owner = std::make_shared<detail::StartupScenePackageSourceOwner>(
        detail::StartupScenePackageSourceOwner{
            .archive = data::ZipArchive::open(archive_path)});
    constexpr std::string_view extensions[] = {
        ".ZGF", ".SUP", ".BUF", ".GMS", ".TEX", ".SND",
        ".LOC", ".OCT", ".SGP", ".RMC", ".RMI", ".PRM"};
    constexpr std::string_view prefix = "SCENES/FF-StartUp";
    if (owner->archive.entries().size() != std::size(extensions)) {
      throw std::runtime_error("startup scene archive has non-canonical members");
    }
    const auto member = [&](std::string_view extension) -> const data::ZipEntry & {
      return detail::startup_exact_member(owner->archive,
                                          std::string(prefix) + std::string(extension));
    };
    for (const auto extension : extensions) {
      static_cast<void>(member(extension));
    }
    const auto &zgf_member = member(".ZGF");
    const auto &gms_member = member(".GMS");
    const auto &support_member = member(".SUP");
    const auto &buf_member = member(".BUF");

    owner->zgf_source = owner->archive.read(zgf_member);
    owner->zgf = data::ZgfBundle::parse(
        data::PackedResource::parse(owner->zgf_source));
    owner->gms_source = owner->archive.read(gms_member);
    owner->gms =
        data::GmsImage::parse(data::PackedResource::parse(owner->gms_source));
    owner->support_source = owner->archive.read(support_member);
    owner->support = data::SceneSupport::parse(owner->support_source);
    if (owner->support.dependencies().empty()) {
      throw std::runtime_error(
          "startup scene support input has no dependencies");
    }
    owner->buf_source = owner->archive.read(buf_member);
    owner->gms.validate_buf(owner->buf_source);
    for (const auto extension : extensions) {
      if (extension != ".ZGF" && extension != ".GMS" &&
          extension != ".SUP" && extension != ".BUF") {
        owner->raw_sources.push_back(owner->archive.read(member(extension)));
      }
    }

    return StartupSceneLoadPackage::complete_checked_source(
        target,
        std::shared_ptr<const void>(owner, std::addressof(owner->archive)),
        std::shared_ptr<const void>(owner, std::addressof(owner->gms)),
        std::shared_ptr<const void>(owner, std::addressof(owner->support)),
        std::shared_ptr<const void>(owner, std::addressof(owner->raw_sources)),
        StartupSceneFactoryInputs(
            std::shared_ptr<const data::ZgfBundle>(owner,
                                                    std::addressof(owner->zgf)),
            std::shared_ptr<const data::GmsImage>(owner,
                                                   std::addressof(owner->gms)),
            std::shared_ptr<const std::vector<std::byte>>(
                owner, std::addressof(owner->buf_source))));
  }
};

} // namespace off::runtime
