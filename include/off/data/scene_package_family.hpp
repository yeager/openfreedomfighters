#pragma once

#include "off/data/scene_resource_catalog.hpp"
#include "off/data/zip_archive.hpp"

#include <filesystem>
#include <memory>
#include <string_view>
#include <vector>

namespace off::data {

// A checked, complete scene archive family.  This is intentionally a source
// boundary only: it owns a ZIP snapshot and exposes member bytes by resource
// role, but it neither parses the role-specific formats nor creates a scene.
//
// The checked family has ten core scene resources plus BUF and LOC. ANM is a
// separately optional resource: FF-Intro is the observed twelve-member form,
// while FF-C03A carries all thirteen roles. Keeping this admission separate
// from later readers prevents a partial or mixed archive from becoming a
// common input merely because one consumer happened to need fewer members.
class ScenePackageFamily final {
public:
  [[nodiscard]] static ScenePackageFamily
  open_complete_checked(const std::filesystem::path &archive_path,
                        std::string_view expected_stem);

  [[nodiscard]] const ZipEntry &member(SceneResourceKind kind) const;
  [[nodiscard]] std::vector<std::byte> read(SceneResourceKind kind) const;
  [[nodiscard]] std::size_t member_count() const noexcept;

private:
  explicit ScenePackageFamily(std::shared_ptr<const ZipArchive> archive,
                              SceneResourceCatalog catalog) noexcept;

  std::shared_ptr<const ZipArchive> archive_;
  SceneResourceCatalog catalog_;
};

} // namespace off::data
