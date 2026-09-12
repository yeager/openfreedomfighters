#pragma once

#include "off/crypto/sha256.hpp"
#include "off/simulation/world.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace off::simulation {

// Project-authored identity for a portable campaign save. It binds a save to
// a verified required-data manifest and a project campaign contract; neither
// field is a retail save identifier or payload.
struct ProjectSaveIdentity final {
  crypto::Sha256Digest data_manifest_fingerprint{};
  std::string campaign_id;
  auto operator<=>(const ProjectSaveIdentity &) const = default;
};

enum class SaveLoadStatus : unsigned char {
  missing,
  loaded,
  invalid,
  ambiguous,
  io_error,
};

struct SaveLoadResult final {
  SaveLoadStatus status{SaveLoadStatus::missing};
  std::uint64_t generation{};
};

// Two-generation portable save store. A slot root "campaign" owns exactly
// "campaign.0.offsave" and "campaign.1.offsave" in the same directory.
// The store deliberately has no retail save import path.
class ProjectSaveStore final {
public:
  explicit ProjectSaveStore(std::filesystem::path slot_root);

  [[nodiscard]] bool save(const SimulationWorld &, const ProjectSaveIdentity &) const;
  [[nodiscard]] SaveLoadResult load(SimulationWorld &,
                                    const ProjectSaveIdentity &) const;

  [[nodiscard]] std::filesystem::path generation_path(unsigned) const;

private:
  std::filesystem::path slot_root_;
};

} // namespace off::simulation
