#pragma once

#include "off/data/gms_image.hpp"
#include "off/data/packed_resource.hpp"
#include "off/data/scene_support.hpp"
#include "off/data/zip_archive.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace off::runtime {

// Opaque source leases for one explicitly caller-selected MovieCuts Loader
// archive. This is preparation only: it neither chooses a following main
// archive nor starts a cinematic, audio stream, transition, or renderer.
class MovieCutLoaderPackage final {
public:
  [[nodiscard]] std::string_view cut_identifier() const noexcept {
    return cut_identifier_;
  }

private:
  friend class MovieCutLoaderPackageSource;
  MovieCutLoaderPackage(std::string identifier, std::shared_ptr<const void> archive,
                        std::shared_ptr<const void> gms,
                        std::shared_ptr<const void> support)
      : cut_identifier_(std::move(identifier)), archive_(std::move(archive)),
        gms_(std::move(gms)), support_(std::move(support)) {}

  std::string cut_identifier_;
  std::shared_ptr<const void> archive_, gms_, support_;
};

class MovieCutLoaderPackageSource final {
public:
  [[nodiscard]] static MovieCutLoaderPackage prepare_checked(
      std::string_view cut_identifier, const std::filesystem::path& archive_path) {
    const bool valid_identifier =
        std::all_of(cut_identifier.begin(), cut_identifier.end(),
                    [](const unsigned char character) {
                      return std::isalnum(character) || character == '_';
                    });
    if (cut_identifier.empty() || cut_identifier.size() > 64U || !valid_identifier) {
      throw std::runtime_error("MovieCut loader identifier is invalid");
    }
    struct Owner final {
      data::ZipArchive archive;
      std::vector<std::byte> gms_bytes;
      data::GmsImage gms;
      std::vector<std::byte> support_bytes;
      data::SceneSupport support;
    };
    auto owner = std::make_shared<Owner>(Owner{.archive = data::ZipArchive::open(archive_path)});
    const std::string prefix = "SCENES/Cutscenes/MovieCuts/" +
                               std::string(cut_identifier) + "/Loader";
    const auto* gms_member = owner->archive.find(prefix + ".GMS");
    const auto* support_member = owner->archive.find(prefix + ".SUP");
    if (!gms_member || !support_member) {
      throw std::runtime_error("MovieCut loader archive has missing required members");
    }
    std::size_t gms_count{}, support_count{};
    for (const auto& member : owner->archive.entries()) {
      if (member.name == prefix + ".GMS") ++gms_count;
      if (member.name == prefix + ".SUP") ++support_count;
    }
    if (gms_count != 1U || support_count != 1U) {
      throw std::runtime_error("MovieCut loader archive has duplicate required members");
    }
    owner->gms_bytes = owner->archive.read(*gms_member);
    owner->gms = data::GmsImage::parse(data::PackedResource::parse(owner->gms_bytes));
    owner->support_bytes = owner->archive.read(*support_member);
    owner->support = data::SceneSupport::parse(owner->support_bytes);
    if (owner->support.dependencies().empty()) {
      throw std::runtime_error("MovieCut loader support has no dependencies");
    }
    return MovieCutLoaderPackage(
        std::string(cut_identifier),
        std::shared_ptr<const void>(owner, std::addressof(owner->archive)),
        std::shared_ptr<const void>(owner, std::addressof(owner->gms)),
        std::shared_ptr<const void>(owner, std::addressof(owner->support)));
  }
};

} // namespace off::runtime
