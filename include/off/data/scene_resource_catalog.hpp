#pragma once

#include "off/data/zip_archive.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace off::data {

// A scene package is one authored resource set, not an arbitrary collection
// of files with familiar extensions.  This catalog records the checked ZIP
// members by role before any individual resource parser receives bytes.
enum class SceneResourceKind : std::uint8_t {
  zgf,
  sup,
  gms,
  tex,
  prm,
  rmc,
  rmi,
  snd,
  oct,
  sgp,
  buf,
  loc,
  anm,
};

class SceneResourceCatalog final {
public:
  [[nodiscard]] static SceneResourceCatalog
  from_archive(const ZipArchive &archive);

  [[nodiscard]] const ZipEntry &member(SceneResourceKind kind) const;
  [[nodiscard]] const ZipEntry *
  optional_member(SceneResourceKind kind) const noexcept;
  [[nodiscard]] std::span<const ZipEntry> required_members() const noexcept;

private:
  static constexpr std::size_t required_count = 10;
  static constexpr std::size_t optional_count = 3;
  std::array<ZipEntry, required_count> required_members_{};
  std::array<std::optional<ZipEntry>, optional_count> optional_members_{};
};

} // namespace off::data
