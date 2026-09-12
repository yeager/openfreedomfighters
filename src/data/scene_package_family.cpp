#include "off/data/scene_package_family.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <stdexcept>
#include <string>

namespace off::data {
namespace {

constexpr std::array all_kinds{
    SceneResourceKind::zgf, SceneResourceKind::sup, SceneResourceKind::gms,
    SceneResourceKind::tex, SceneResourceKind::prm, SceneResourceKind::rmc,
    SceneResourceKind::rmi, SceneResourceKind::snd, SceneResourceKind::oct,
    SceneResourceKind::sgp, SceneResourceKind::buf, SceneResourceKind::loc,
    SceneResourceKind::anm};

[[nodiscard]] std::string normalized(std::string_view value) {
  std::string result;
  result.reserve(value.size());
  for (const auto character : value) {
    const auto slash = character == '\\' ? '/' : character;
    result.push_back(static_cast<char>(
        std::tolower(static_cast<unsigned char>(slash))));
  }
  return result;
}

[[nodiscard]] std::string_view extension(SceneResourceKind kind) {
  switch (kind) {
  case SceneResourceKind::zgf: return ".zgf";
  case SceneResourceKind::sup: return ".sup";
  case SceneResourceKind::gms: return ".gms";
  case SceneResourceKind::tex: return ".tex";
  case SceneResourceKind::prm: return ".prm";
  case SceneResourceKind::rmc: return ".rmc";
  case SceneResourceKind::rmi: return ".rmi";
  case SceneResourceKind::snd: return ".snd";
  case SceneResourceKind::oct: return ".oct";
  case SceneResourceKind::sgp: return ".sgp";
  case SceneResourceKind::buf: return ".buf";
  case SceneResourceKind::loc: return ".loc";
  case SceneResourceKind::anm: return ".anm";
  }
  throw std::runtime_error("scene package has an invalid resource kind");
}

void require_regular_archive(const std::filesystem::path &archive_path) {
  std::error_code error;
  const auto status = std::filesystem::symlink_status(archive_path, error);
  if (error || std::filesystem::is_symlink(status) ||
      !std::filesystem::is_regular_file(status)) {
    throw std::runtime_error("scene package archive source is unavailable");
  }
}

} // namespace

ScenePackageFamily::ScenePackageFamily(
    std::shared_ptr<const ZipArchive> archive,
    SceneResourceCatalog catalog) noexcept
    : archive_(std::move(archive)), catalog_(std::move(catalog)) {}

ScenePackageFamily ScenePackageFamily::open_complete_checked(
    const std::filesystem::path &archive_path, std::string_view expected_stem) {
  if (expected_stem.empty()) {
    throw std::runtime_error("scene package has no expected source identity");
  }
  require_regular_archive(archive_path);
  auto archive = std::make_shared<ZipArchive>(ZipArchive::open(archive_path));
  if (archive->entries().size() != all_kinds.size()) {
    throw std::runtime_error("scene package does not have a complete resource family");
  }
  auto catalog = SceneResourceCatalog::from_archive(*archive);
  const auto prefix = normalized("scenes/" + std::string(expected_stem));
  for (const auto kind : all_kinds) {
    const auto expected = prefix + std::string(extension(kind));
    const auto *entry = kind == SceneResourceKind::buf ||
                                kind == SceneResourceKind::loc ||
                                kind == SceneResourceKind::anm
                            ? catalog.optional_member(kind)
                            : std::addressof(catalog.member(kind));
    if (entry == nullptr || normalized(entry->name) != expected) {
      throw std::runtime_error("scene package has an unexpected resource family");
    }
  }
  return ScenePackageFamily(std::move(archive), std::move(catalog));
}

const ZipEntry &ScenePackageFamily::member(SceneResourceKind kind) const {
  if (const auto *optional = catalog_.optional_member(kind)) return *optional;
  return catalog_.member(kind);
}

std::vector<std::byte> ScenePackageFamily::read(SceneResourceKind kind) const {
  return archive_->read(member(kind));
}

std::size_t ScenePackageFamily::member_count() const noexcept {
  return archive_->entries().size();
}

} // namespace off::data
