#pragma once

#include "off/runtime/startup_boot_scene_factory.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace off::runtime {

// This is an observation instrument, not a startup host.  Its values are
// deliberately synthetic and its result contains a trace rather than a live
// controller, scene, registration, or allocator handle.  Normal startup must
// not include or call this type.
enum class SyntheticStartupBootSceneProbeCall {
  registry_live,
  allocate_ordinary_window,
  canonical_live_window_owner,
  attach_boot_menu_component,
  live_boot_menu_component,
};

struct SyntheticStartupBootSceneProbeTraceEntry final {
  SyntheticStartupBootSceneProbeCall call{};
  // This ordinal comes from the checked GMS directory; it is evidence, not an
  // allocator identity.
  std::size_t source_boot_owner_directory_ordinal{};
  // These are test-instrument values supplied by the caller, never recovered
  // object-service identities.
  std::uint64_t synthetic_owner_identity{};
  std::uint64_t synthetic_component_identity{};
};

struct SyntheticStartupBootSceneProbeIds final {
  std::uint64_t synthetic_factory_generation{};
  std::uint64_t synthetic_owner_identity{};
  std::uint64_t synthetic_component_identity{};
};

// A content-free fingerprint of the retained source hierarchy.  All ordinals
// in the digest are a traversal-local numbering, never GMS directory indexes.
struct BootMenuSourceHierarchySummary final {
  std::size_t retained_node_count{};
  std::size_t boot_owner_depth{};
  std::size_t boot_owner_direct_child_count{};
  std::size_t maximum_depth{};
  std::uint64_t topology_digest{};
};

class SyntheticStartupBootSceneProbeResult final {
public:
  [[nodiscard]] std::size_t
  source_boot_owner_directory_ordinal() const noexcept {
    return source_boot_owner_directory_ordinal_;
  }
  [[nodiscard]] const SyntheticStartupBootSceneProbeIds &
  synthetic_ids() const noexcept {
    return synthetic_ids_;
  }
  [[nodiscard]] const std::vector<SyntheticStartupBootSceneProbeTraceEntry> &
  trace() const noexcept {
    return trace_;
  }
  [[nodiscard]] const BootMenuSourceHierarchySummary &
  hierarchy_summary() const noexcept {
    return hierarchy_summary_;
  }

private:
  friend class SyntheticStartupBootSceneProbeHost;
  SyntheticStartupBootSceneProbeResult(
      std::size_t source_boot_owner_directory_ordinal,
      SyntheticStartupBootSceneProbeIds synthetic_ids,
      BootMenuSourceHierarchySummary hierarchy_summary,
      std::vector<SyntheticStartupBootSceneProbeTraceEntry> trace)
      : source_boot_owner_directory_ordinal_(source_boot_owner_directory_ordinal),
        synthetic_ids_(synthetic_ids), hierarchy_summary_(hierarchy_summary),
        trace_(std::move(trace)) {}

  std::size_t source_boot_owner_directory_ordinal_{};
  SyntheticStartupBootSceneProbeIds synthetic_ids_{};
  BootMenuSourceHierarchySummary hierarchy_summary_{};
  std::vector<SyntheticStartupBootSceneProbeTraceEntry> trace_;
};

// A deliberately disconnected harness for exercising the checked package +
// GMS directory boundary.  It can establish only the call order required by
// StartupBootSceneFactory.  It does not discover real allocator values; use
// its trace to compare future original-runtime observations.
class SyntheticStartupBootSceneProbeHost final {
public:
  [[nodiscard]] static SyntheticStartupBootSceneProbeResult observe(
      std::shared_ptr<const StartupSceneLoadPackage> package,
      const StartupBootSceneDirectorySource &directory,
      SyntheticStartupBootSceneProbeIds synthetic_ids) {
    if (!package || !package->factory_inputs().has_value() ||
        synthetic_ids.synthetic_factory_generation == 0U ||
        synthetic_ids.synthetic_owner_identity == 0U ||
        synthetic_ids.synthetic_component_identity == 0U) {
      throw std::runtime_error(
          "synthetic startup probe requires explicit nonzero synthetic IDs");
    }

    const auto ordinal = directory.boot_owner_directory_index();
    const auto hierarchy_summary = summarize_hierarchy(directory, ordinal);
    std::vector<SyntheticStartupBootSceneProbeTraceEntry> trace;
    const auto record = [&](SyntheticStartupBootSceneProbeCall call) {
      trace.push_back({.call = call,
                       .source_boot_owner_directory_ordinal = ordinal,
                       .synthetic_owner_identity =
                           synthetic_ids.synthetic_owner_identity,
                       .synthetic_component_identity =
                           synthetic_ids.synthetic_component_identity});
    };
    const auto synthetic_scene_lifetime =
        std::make_shared<const std::uint8_t>(0U);
    const auto scene = StartupBootSceneLease::live(synthetic_scene_lifetime);
    const StartupBootSceneConstructionServices services{
        .registry_live = [&] {
          record(SyntheticStartupBootSceneProbeCall::registry_live);
          return true;
        },
        .allocate_ordinary_window = [&] {
          record(SyntheticStartupBootSceneProbeCall::allocate_ordinary_window);
          return synthetic_ids.synthetic_owner_identity;
        },
        .canonical_live_window_owner = [&](std::uint64_t owner) {
          record(SyntheticStartupBootSceneProbeCall::canonical_live_window_owner);
          return owner == synthetic_ids.synthetic_owner_identity;
        },
        .attach_boot_menu_component = [&](std::uint64_t owner, float) {
          record(SyntheticStartupBootSceneProbeCall::attach_boot_menu_component);
          return owner == synthetic_ids.synthetic_owner_identity
                     ? synthetic_ids.synthetic_component_identity : 0U;
        },
        .live_boot_menu_component = [&](std::uint64_t component) {
          record(SyntheticStartupBootSceneProbeCall::live_boot_menu_component);
          return component == synthetic_ids.synthetic_component_identity;
        },
    };
    StartupBootSceneFactory factory;
    // Discard the token: retaining it would falsely make this probe appear to
    // own a runnable scene.
    static_cast<void>(factory.construct(std::move(package), directory, scene,
                                        synthetic_ids.synthetic_factory_generation,
                                        services));
    return SyntheticStartupBootSceneProbeResult(ordinal, synthetic_ids,
                                                hierarchy_summary,
                                                std::move(trace));
  }

private:
  [[nodiscard]] static BootMenuSourceHierarchySummary summarize_hierarchy(
      const StartupBootSceneDirectorySource &directory,
      std::size_t boot_owner_directory_ordinal) {
    const auto scope = directory.hierarchy_scope();
    if (!scope.complete_directory_mapping || scope.nodes.empty())
      throw std::runtime_error("synthetic startup probe hierarchy is incomplete");

    std::unordered_map<std::size_t, const StartupWindowHierarchySourceNode *>
        by_directory;
    by_directory.reserve(scope.nodes.size());
    for (const auto &node : scope.nodes) {
      if (!by_directory.emplace(node.directory_index, &node).second)
        throw std::runtime_error("synthetic startup probe hierarchy is duplicated");
    }
    const auto root = by_directory.find(scope.root_directory_index);
    const auto owner = by_directory.find(boot_owner_directory_ordinal);
    if (root == by_directory.end() || owner == by_directory.end())
      throw std::runtime_error("synthetic startup probe hierarchy lacks root or owner");

    constexpr std::uint64_t offset_basis = 14695981039346656037ULL;
    constexpr std::uint64_t prime = 1099511628211ULL;
    std::uint64_t digest = offset_basis;
    const auto hash = [&](std::uint64_t value) {
      for (unsigned byte{}; byte < 8U; ++byte) {
        digest ^= (value >> (byte * 8U)) & 0xffU;
        digest *= prime;
      }
    };
    std::unordered_map<std::size_t, std::size_t> preorder;
    preorder.reserve(scope.nodes.size());
    std::vector<const StartupWindowHierarchySourceNode *> ordered;
    ordered.reserve(scope.nodes.size());
    std::unordered_set<std::size_t> visiting;
    std::size_t owner_depth{};
    std::size_t maximum_depth{};
    const auto visit = [&](auto &&self, const StartupWindowHierarchySourceNode *node,
                           std::size_t depth) -> void {
      if (!visiting.insert(node->directory_index).second)
        throw std::runtime_error("synthetic startup probe hierarchy is cyclic");
      if (!preorder.emplace(node->directory_index, ordered.size()).second)
        throw std::runtime_error("synthetic startup probe hierarchy is shared");
      ordered.push_back(node);
      maximum_depth = std::max(maximum_depth, depth);
      if (node->directory_index == boot_owner_directory_ordinal)
        owner_depth = depth;
      for (const auto child_directory : node->children_in_directory_order) {
        const auto child = by_directory.find(child_directory);
        if (child == by_directory.end())
          throw std::runtime_error("synthetic startup probe child is outside scope");
        self(self, child->second, depth + 1U);
      }
      visiting.erase(node->directory_index);
    };
    visit(visit, root->second, 0U);
    if (ordered.size() != scope.nodes.size())
      throw std::runtime_error("synthetic startup probe hierarchy is partial");

    hash(ordered.size());
    for (const auto *node : ordered) {
      hash(preorder.at(node->directory_index));
      hash(node->parent_directory_index
               ? preorder.at(*node->parent_directory_index) + 1U
               : 0U);
      hash(node->children_in_directory_order.size());
      for (const auto child_directory : node->children_in_directory_order)
        hash(preorder.at(child_directory));
    }
    return {.retained_node_count = ordered.size(),
            .boot_owner_depth = owner_depth,
            .boot_owner_direct_child_count =
                owner->second->children_in_directory_order.size(),
            .maximum_depth = maximum_depth,
            .topology_digest = digest};
  }
};

} // namespace off::runtime
