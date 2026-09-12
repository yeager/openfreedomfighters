#pragma once

#include "off/data/gms_image.hpp"
#include "off/runtime/startup_boot_scene_construction.hpp"
#include "off/runtime/startup_window_hierarchy_snapshot.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

namespace off::runtime {

// Parsed, source-only evidence for the one reviewed FF-StartUp BootMenu
// attachment.  It owns the hierarchy backing store used by hierarchy_scope();
// no caller may manufacture a scope from transient GMS vectors.  This is not
// a factory, scene lease, component reader, or lifecycle admission.
class StartupBootSceneDirectorySource final {
public:
  [[nodiscard]] static StartupBootSceneDirectorySource
  from_checked_gms(const data::GmsImage &gms) {
    constexpr std::uint32_t ordinary_window_source_type = 0x00100031U;
    constexpr std::string_view boot_menu_identifier = "ZWINDOW_BootMenu";

    const auto &directory = gms.directory();
    const auto &hierarchy = gms.hierarchy();
    if (directory.empty() || directory.size() != hierarchy.size()) {
      throw std::runtime_error(
          "startup boot directory has no complete GMS hierarchy");
    }

    std::optional<std::size_t> boot_owner;
    float boot_parameter{};
    std::vector<std::vector<std::size_t>> children;
    std::vector<std::optional<std::size_t>> parents;
    children.reserve(hierarchy.size());
    parents.reserve(hierarchy.size());
    for (std::size_t index = 0; index < hierarchy.size(); ++index) {
      const auto &node = hierarchy[index];
      if (node.directory_index != index) {
        throw std::runtime_error(
            "startup boot directory GMS hierarchy is not directory ordered");
      }
      if (node.parent_directory_index &&
          *node.parent_directory_index >= hierarchy.size()) {
        throw std::runtime_error(
            "startup boot directory GMS hierarchy parent is out of range");
      }
      children.push_back(node.children_in_directory_order);
      parents.push_back(node.parent_directory_index);

      const auto &entry = directory[index];
      if (entry.source_type != ordinary_window_source_type) {
        continue;
      }
      for (std::size_t attachment = 0; attachment < entry.attachments.size();
           ++attachment) {
        if (gms.attachment_identifier(index, attachment) !=
            boot_menu_identifier) {
          continue;
        }
        // This boundary is keyed by the reviewed attachment itself, not by a
        // best-effort search for one usable occurrence.  A second or malformed
        // BootMenu-labelled attachment would make the source ambiguous, so it
        // must not be silently ignored in favour of another record.
        if (entry.source_type != ordinary_window_source_type ||
            !std::isfinite(entry.attachments[attachment].parameter) ||
            entry.attachments[attachment].parameter != 1.0F) {
          throw std::runtime_error(
              "startup boot directory has a non-canonical BootMenu attachment");
        }
        if (boot_owner) {
          throw std::runtime_error(
              "startup boot directory has duplicate BootMenu attachments");
        }
        boot_owner = index;
        boot_parameter = entry.attachments[attachment].parameter;
      }
    }
    if (!boot_owner) {
      throw std::runtime_error(
          "startup boot directory has no canonical BootMenu window");
    }

    auto root = *boot_owner;
    for (std::size_t steps{}; hierarchy[root].parent_directory_index; ++steps) {
      if (steps >= hierarchy.size()) {
        throw std::runtime_error("startup boot directory GMS hierarchy is cyclic");
      }
      root = *hierarchy[root].parent_directory_index;
    }
    std::vector<bool> retained(hierarchy.size());
    const auto retain = [&](auto &&self, std::size_t index) -> void {
      if (index >= hierarchy.size() || retained[index]) {
        throw std::runtime_error("startup boot directory GMS hierarchy is invalid");
      }
      retained[index] = true;
      for (const auto child : hierarchy[index].children_in_directory_order) {
        self(self, child);
      }
    };
    retain(retain, root);

    return StartupBootSceneDirectorySource(
        std::addressof(gms),
        {.complete_directory_mapping = true,
         .canonical_ordinary_window_source = true,
         .component_identifier = boot_menu_identifier,
         .component_parameter = boot_parameter},
        *boot_owner, root, std::move(parents), std::move(children),
        std::move(retained));
  }

  [[nodiscard]] const StartupBootSceneDirectoryProof &proof() const noexcept {
    return proof_;
  }
  [[nodiscard]] std::size_t boot_owner_directory_index() const noexcept {
    return boot_owner_directory_index_;
  }
  [[nodiscard]] StartupWindowHierarchySourceScope hierarchy_scope() const
      noexcept {
    return {true, root_directory_index_, nodes_};
  }

  // The directory source is a derived view of one checked GMS image. A scene
  // factory may use it only with that exact retained parser object, never a
  // structurally similar source from another package.
  [[nodiscard]] bool matches_checked_gms(const data::GmsImage &gms) const {
    return source_image_ == std::addressof(gms);
  }

private:
  StartupBootSceneDirectorySource(
      const data::GmsImage *source_image, StartupBootSceneDirectoryProof proof, std::size_t boot_owner,
      std::size_t root, std::vector<std::optional<std::size_t>> parents,
      std::vector<std::vector<std::size_t>> children,
      std::vector<bool> retained)
      : source_image_(source_image), proof_(proof), boot_owner_directory_index_(boot_owner),
        root_directory_index_(root), children_(std::move(children)),
        nodes_() {
    nodes_.reserve(parents.size());
    for (std::size_t index = 0; index < parents.size(); ++index) {
      if (retained[index]) {
        nodes_.push_back({index, parents[index], children_[index]});
      }
    }
  }

  const data::GmsImage *source_image_{};
  StartupBootSceneDirectoryProof proof_;
  std::size_t boot_owner_directory_index_{};
  std::size_t root_directory_index_{};
  // Kept before nodes_ so the spans in nodes_ remain valid through destruction.
  std::vector<std::vector<std::size_t>> children_;
  std::vector<StartupWindowHierarchySourceNode> nodes_;
};

} // namespace off::runtime
