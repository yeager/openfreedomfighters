#include "off/data/scene_resource_catalog.hpp"

#include <algorithm>
#include <cctype>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>

namespace off::data {
namespace {

[[nodiscard]] std::string lowercase(std::string_view value) {
  std::string result;
  result.reserve(value.size());
  for (const auto character : value) {
    result.push_back(
        static_cast<char>(std::tolower(static_cast<unsigned char>(character))));
  }
  return result;
}

[[nodiscard]] std::optional<SceneResourceKind>
kind_for_extension(std::string_view extension) noexcept {
  if (extension == ".zgf")
    return SceneResourceKind::zgf;
  if (extension == ".sup")
    return SceneResourceKind::sup;
  if (extension == ".gms")
    return SceneResourceKind::gms;
  if (extension == ".tex")
    return SceneResourceKind::tex;
  if (extension == ".prm")
    return SceneResourceKind::prm;
  if (extension == ".rmc")
    return SceneResourceKind::rmc;
  if (extension == ".rmi")
    return SceneResourceKind::rmi;
  if (extension == ".snd")
    return SceneResourceKind::snd;
  if (extension == ".oct")
    return SceneResourceKind::oct;
  if (extension == ".sgp")
    return SceneResourceKind::sgp;
  if (extension == ".buf")
    return SceneResourceKind::buf;
  if (extension == ".loc")
    return SceneResourceKind::loc;
  if (extension == ".anm")
    return SceneResourceKind::anm;
  return std::nullopt;
}

[[nodiscard]] constexpr std::size_t index(SceneResourceKind kind) noexcept {
  return static_cast<std::size_t>(kind);
}

[[nodiscard]] constexpr bool required(SceneResourceKind kind) noexcept {
  return index(kind) < 10U;
}

struct MemberIdentity {
  std::string directory;
  std::string stem;
};

[[nodiscard]] MemberIdentity member_identity(const ZipEntry &entry) {
  const auto slash = entry.name.find_last_of("/\\");
  const auto name = std::string_view(entry.name)
                        .substr(slash == std::string::npos ? 0U : slash + 1U);
  const auto dot = name.find_last_of('.');
  if (dot == std::string_view::npos || dot == 0U || dot + 1U == name.size()) {
    throw std::runtime_error(
        "scene archive has an invalid resource member name");
  }
  return {
      .directory = lowercase(
          std::string_view(entry.name)
              .substr(0U, slash == std::string::npos ? 0U : slash + 1U)),
      .stem = lowercase(name.substr(0U, dot)),
  };
}

} // namespace

SceneResourceCatalog
SceneResourceCatalog::from_archive(const ZipArchive &archive) {
  SceneResourceCatalog result;
  for (const auto &entry : archive.entries()) {
    const auto slash = entry.name.find_last_of("/\\");
    const auto name = std::string_view(entry.name)
                          .substr(slash == std::string::npos ? 0U : slash + 1U);
    const auto dot = name.find_last_of('.');
    if (dot == std::string_view::npos) {
      throw std::runtime_error("scene archive has an unknown resource family");
    }
    const auto kind = kind_for_extension(lowercase(name.substr(dot)));
    if (!kind) {
      throw std::runtime_error("scene archive has an unknown resource family");
    }
    const auto kind_index = index(*kind);
    if (required(*kind)) {
      auto &slot = result.required_members_[kind_index];
      if (!slot.name.empty()) {
        throw std::runtime_error(
            "scene archive contains duplicate scene-resource members");
      }
      slot = entry;
    } else {
      auto &slot = result.optional_members_[kind_index - required_count];
      if (slot) {
        throw std::runtime_error(
            "scene archive has duplicate optional resources");
      }
      slot = entry;
    }
  }
  if (std::ranges::any_of(result.required_members_, [](const ZipEntry &entry) {
        return entry.name.empty();
      })) {
    throw std::runtime_error(
        "scene archive does not contain every required resource exactly once");
  }
  std::optional<MemberIdentity> identity;
  for (const auto &entry : archive.entries()) {
    const auto candidate_identity = member_identity(entry);
    if (!identity) {
      identity = candidate_identity;
    } else if (identity->directory != candidate_identity.directory ||
               identity->stem != candidate_identity.stem) {
      throw std::runtime_error(
          "scene archive resources do not share one source identity");
    }
  }
  return result;
}

const ZipEntry &SceneResourceCatalog::member(SceneResourceKind kind) const {
  if (!required(kind)) {
    throw std::invalid_argument("scene resource is optional");
  }
  return required_members_[index(kind)];
}

const ZipEntry *
SceneResourceCatalog::optional_member(SceneResourceKind kind) const noexcept {
  if (required(kind))
    return nullptr;
  const auto &entry = optional_members_[index(kind) - required_count];
  return entry ? &*entry : nullptr;
}

std::span<const ZipEntry>
SceneResourceCatalog::required_members() const noexcept {
  return required_members_;
}

} // namespace off::data
