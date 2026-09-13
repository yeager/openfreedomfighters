#pragma once

#include "off/data/gms_image.hpp"
#include "off/data/packed_resource.hpp"
#include "off/data/scene_support.hpp"
#include "off/data/zgf_bundle.hpp"
#include "off/data/zip_archive.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <filesystem>
#include <iterator>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace off::runtime {
namespace detail {

[[nodiscard]] inline bool movie_cut_safe_identifier(
    std::string_view identifier) noexcept {
  return !identifier.empty() && identifier.size() <= 64U &&
         std::all_of(identifier.begin(), identifier.end(),
                     [](const unsigned char character) {
                       return std::isalnum(character) || character == '_';
                     });
}

[[nodiscard]] inline std::filesystem::path movie_cut_canonical_data_root(
    const std::filesystem::path &data_root) {
  std::error_code error;
  const auto root_status = std::filesystem::symlink_status(data_root, error);
  if (error || !std::filesystem::is_directory(root_status) ||
      std::filesystem::is_symlink(root_status)) {
    throw std::runtime_error("MovieCut data root must be a real directory");
  }
  const auto canonical_root = std::filesystem::canonical(data_root, error);
  if (error) {
    throw std::runtime_error("MovieCut data root cannot be canonicalized");
  }
  return canonical_root;
}

[[nodiscard]] inline std::filesystem::path movie_cut_real_archive(
    const std::filesystem::path &root, std::string_view cut_identifier,
    std::string_view package_identifier) {
  const auto archive = root / "Scenes" / "Cutscenes" / "MovieCuts" /
                       std::string(cut_identifier) /
                       (std::string(package_identifier) + ".ZIP");
  std::error_code error;
  const auto status = std::filesystem::symlink_status(archive, error);
  if (error || !std::filesystem::is_regular_file(status) ||
      std::filesystem::is_symlink(status)) {
    throw std::runtime_error("MovieCut main archive must be a real file");
  }
  const auto canonical_archive = std::filesystem::canonical(archive, error);
  if (error || canonical_archive != archive.lexically_normal()) {
    throw std::runtime_error("MovieCut main archive must not traverse a symlink");
  }
  return canonical_archive;
}

struct MovieCutMainPackageOwner final {
  data::ZipArchive archive;
  std::vector<std::byte> zgf_bytes;
  data::ZgfBundle zgf;
  std::vector<std::byte> gms_bytes;
  data::GmsImage gms;
  std::vector<std::byte> support_bytes;
  data::SceneSupport support;
  std::vector<std::vector<std::byte>> raw_sources;
};

} // namespace detail

// Opaque source leases for one explicitly caller-selected MovieCuts main
// archive. Preparation only validates and retains source material. It does not
// infer a cut route, construct a scene, or begin video, audio, rendering, or
// transition work.
class MovieCutMainPackage final {
public:
  [[nodiscard]] std::string_view cut_identifier() const noexcept {
    return cut_identifier_;
  }
  [[nodiscard]] std::string_view package_identifier() const noexcept {
    return package_identifier_;
  }

private:
  friend class MovieCutMainPackageSource;
  MovieCutMainPackage(std::string cut_identifier, std::string package_identifier,
                      std::shared_ptr<const void> archive,
                      std::shared_ptr<const void> zgf,
                      std::shared_ptr<const void> gms,
                      std::shared_ptr<const void> support,
                      std::shared_ptr<const void> raw_sources)
      : cut_identifier_(std::move(cut_identifier)),
        package_identifier_(std::move(package_identifier)),
        archive_(std::move(archive)), zgf_(std::move(zgf)), gms_(std::move(gms)),
        support_(std::move(support)), raw_sources_(std::move(raw_sources)) {}

  std::string cut_identifier_;
  std::string package_identifier_;
  std::shared_ptr<const void> archive_, zgf_, gms_, support_, raw_sources_;
};

// Aggregate member capabilities from one already validated MovieCut main
// archive. These are package-format facts only; they do not imply that any
// resource is decodable by a renderer, selected by a cut, or playable.
struct MovieCutMainPackageCapabilities final {
  bool static_scene_graph{};
  bool texture_resources{};
  bool sound_definitions{};
  bool animation_resources{};

  [[nodiscard]] bool operator==(const MovieCutMainPackageCapabilities &) const =
      default;
};

class MovieCutMainPackageSource final {
public:
  [[nodiscard]] static MovieCutMainPackage prepare_checked(
      const std::filesystem::path &data_root, std::string_view cut_identifier,
      std::string_view package_identifier) {
    return prepare_checked_impl(data_root, cut_identifier, package_identifier,
                                true)
        .value();
  }

  // Used by the aggregate MovieCuts probe after verify_install has hashed the
  // complete owned corpus.  It preserves the package's exact archive layout,
  // core-resource parsing, and GMS/BUF cross-check, but does not inflate and
  // retain opaque members which the probe neither inspects nor exposes.
  // This is validation only: it cannot select, load, or play a cut.
  [[nodiscard]] static MovieCutMainPackageCapabilities validate_checked(
      const std::filesystem::path &data_root, std::string_view cut_identifier,
      std::string_view package_identifier) {
    MovieCutMainPackageCapabilities capabilities;
    static_cast<void>(prepare_checked_impl(data_root, cut_identifier,
                                            package_identifier, false,
                                            &capabilities));
    return capabilities;
  }

private:
  [[nodiscard]] static std::optional<MovieCutMainPackage> prepare_checked_impl(
      const std::filesystem::path &data_root, std::string_view cut_identifier,
      std::string_view package_identifier, bool retain_opaque_sources,
      MovieCutMainPackageCapabilities *capabilities = nullptr) {
    if (!retain_opaque_sources) {
      if (capabilities == nullptr) {
        throw std::runtime_error("MovieCut validation requires capabilities");
      }
    }
    if (!detail::movie_cut_safe_identifier(cut_identifier) ||
        !detail::movie_cut_safe_identifier(package_identifier) ||
        package_identifier != std::string(cut_identifier) + "_MAIN") {
      throw std::runtime_error("MovieCut main package identifier is invalid");
    }
    const auto root = detail::movie_cut_canonical_data_root(data_root);
    auto owner = std::make_shared<detail::MovieCutMainPackageOwner>(
        detail::MovieCutMainPackageOwner{.archive = data::ZipArchive::open(
            detail::movie_cut_real_archive(root, cut_identifier,
                                           package_identifier))});
    const std::string prefix = "SCENES/Cutscenes/MovieCuts/" +
                               std::string(cut_identifier) + "/" +
                               std::string(package_identifier);
    constexpr std::string_view required_extensions[] = {
        ".ZGF", ".SUP", ".BUF", ".GMS", ".TEX", ".SND",
        ".LOC", ".OCT", ".SGP", ".RMC", ".RMI", ".PRM"};
    constexpr std::string_view optional_animation = ".ANM";
    const auto exact_member = [&](std::string_view extension) -> const data::ZipEntry & {
      const std::string expected = prefix + std::string(extension);
      const auto count = static_cast<std::size_t>(std::count_if(
          owner->archive.entries().begin(), owner->archive.entries().end(),
          [&](const data::ZipEntry &entry) { return entry.name == expected; }));
      const auto *entry = owner->archive.find(expected);
      if (count != 1U || entry == nullptr) {
        throw std::runtime_error("MovieCut main archive has invalid required members");
      }
      return *entry;
    };
    const auto expected_count = static_cast<std::size_t>(
        std::count_if(owner->archive.entries().begin(), owner->archive.entries().end(),
                      [&](const data::ZipEntry &entry) {
                        return entry.name == prefix + std::string(optional_animation);
                      }));
    if (expected_count > 1U || owner->archive.entries().size() !=
            std::size(required_extensions) + expected_count) {
      throw std::runtime_error("MovieCut main archive has non-canonical members");
    }
    for (const auto extension : required_extensions) {
      static_cast<void>(exact_member(extension));
    }
    const auto &zgf_member = exact_member(".ZGF");
    const auto &gms_member = exact_member(".GMS");
    const auto &support_member = exact_member(".SUP");
    owner->zgf_bytes = owner->archive.read(zgf_member);
    owner->zgf = data::ZgfBundle::parse(
        data::PackedResource::parse(owner->zgf_bytes));
    owner->gms_bytes = owner->archive.read(gms_member);
    owner->gms = data::GmsImage::parse(
        data::PackedResource::parse(owner->gms_bytes));
    auto buf_bytes = owner->archive.read(exact_member(".BUF"));
    owner->gms.validate_buf(buf_bytes);
    owner->support_bytes = owner->archive.read(support_member);
    owner->support = data::SceneSupport::parse(owner->support_bytes);
    if (owner->support.dependencies().empty()) {
      throw std::runtime_error("MovieCut main package support has no dependencies");
    }
    if (capabilities != nullptr) {
      *capabilities = {.static_scene_graph = true,
                       .texture_resources = true,
                       .sound_definitions = true,
                       .animation_resources = expected_count == 1U};
    }
    if (!retain_opaque_sources) {
      return std::nullopt;
    }
    owner->raw_sources.push_back(std::move(buf_bytes));
    for (const auto extension : required_extensions) {
      if (extension != ".ZGF" && extension != ".GMS" && extension != ".SUP" &&
          extension != ".BUF") {
        owner->raw_sources.push_back(owner->archive.read(exact_member(extension)));
      }
    }
    if (expected_count == 1U) {
      owner->raw_sources.push_back(owner->archive.read(exact_member(optional_animation)));
    }
    return MovieCutMainPackage(
        std::string(cut_identifier), std::string(package_identifier),
        std::shared_ptr<const void>(owner, std::addressof(owner->archive)),
        std::shared_ptr<const void>(owner, std::addressof(owner->zgf)),
        std::shared_ptr<const void>(owner, std::addressof(owner->gms)),
        std::shared_ptr<const void>(owner, std::addressof(owner->support)),
        std::shared_ptr<const void>(owner, std::addressof(owner->raw_sources)));
  }
};

} // namespace off::runtime
