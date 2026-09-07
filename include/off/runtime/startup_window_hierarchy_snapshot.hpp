#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace off::runtime {

// Caller-owned lifetime for one factory-built startup scene. A parsed GMS map,
// source name, or diagnostic widget cannot manufacture this lease.
class StartupWindowHierarchyLease final {
public:
  [[nodiscard]] static StartupWindowHierarchyLease
  live(std::shared_ptr<const void> lifetime) {
    if (!lifetime)
      throw std::runtime_error("Startup window hierarchy has no scene lifetime");
    return StartupWindowHierarchyLease(std::move(lifetime));
  }

private:
  friend class StartupWindowHierarchyFactory;
  friend class StartupWindowHierarchySnapshot;
  explicit StartupWindowHierarchyLease(std::shared_ptr<const void> lifetime)
      : lifetime_(std::move(lifetime)) {}
  std::shared_ptr<const void> lifetime_;
};

enum class StartupWindowNodeFamily : std::uint8_t { container, leaf };

// Parsed source-map evidence only. It deliberately contains neither a runtime
// identity nor a source-type-to-family shortcut.
struct StartupWindowHierarchySourceNode {
  std::size_t directory_index{};
  std::optional<std::size_t> parent_directory_index;
  std::span<const std::size_t> children_in_directory_order;
};

struct StartupWindowHierarchySourceScope {
  bool complete_directory_mapping{};
  std::size_t root_directory_index{};
  std::span<const StartupWindowHierarchySourceNode> nodes;
};

// Read only through the live factory service while its hierarchy read guard is
// held. `family` is factory/attachment metadata: it is not inferred from the
// serialized source type by this boundary.
struct StartupFactoryProvenWindowNode {
  std::uint64_t identity{};
  std::size_t directory_index{};
  std::optional<std::uint64_t> parent_identity;
  StartupWindowNodeFamily family{StartupWindowNodeFamily::leaf};
  std::optional<std::uint64_t> first_child_identity;
  std::optional<std::uint64_t> next_sibling_identity;
};

struct StartupWindowHierarchySnapshotServices {
  // The guard is an explicit factory-owned read phase. Its opaque nonzero
  // token is returned to `end_read_guard` even when construction validation
  // fails.
  std::function<std::optional<std::uint64_t>()> begin_read_guard;
  std::function<void(std::uint64_t)> end_read_guard;
  std::function<std::uint64_t()> hierarchy_epoch;
  std::function<bool(std::uint64_t)> factory_generation_live;
  std::function<std::optional<StartupFactoryProvenWindowNode>(std::size_t)>
      read_factory_proven_node;
};

struct StartupWindowHierarchySnapshotNode {
  std::uint64_t identity{};
  std::size_t directory_index{};
  std::optional<std::uint64_t> parent_identity;
  StartupWindowNodeFamily family{StartupWindowNodeFamily::leaf};
  std::size_t source_sibling_ordinal{};
  std::optional<std::uint64_t> first_child_identity;
  std::optional<std::uint64_t> next_sibling_identity;
};

// Move-only construction/link evidence. It has no active-root, visibility,
// component, view, transform, pass, texture, or renderer state.
class StartupWindowHierarchySnapshot final {
public:
  StartupWindowHierarchySnapshot(const StartupWindowHierarchySnapshot &) = delete;
  StartupWindowHierarchySnapshot &
  operator=(const StartupWindowHierarchySnapshot &) = delete;
  StartupWindowHierarchySnapshot(StartupWindowHierarchySnapshot &&) noexcept = default;
  StartupWindowHierarchySnapshot &
  operator=(StartupWindowHierarchySnapshot &&) noexcept = default;

  [[nodiscard]] bool valid() const noexcept {
    return lifetime_ && root_identity_ != 0U && factory_generation_ != 0U &&
           hierarchy_epoch_ != 0U && !nodes_.empty();
  }
  [[nodiscard]] std::uint64_t root_identity() const noexcept {
    return root_identity_;
  }
  [[nodiscard]] std::uint64_t factory_generation() const noexcept {
    return factory_generation_;
  }
  [[nodiscard]] std::uint64_t hierarchy_epoch() const noexcept {
    return hierarchy_epoch_;
  }
  [[nodiscard]] std::span<const StartupWindowHierarchySnapshotNode>
  nodes() const noexcept {
    return nodes_;
  }
  [[nodiscard]] bool bound_to(std::uint64_t factory_generation,
                              std::uint64_t hierarchy_epoch) const noexcept {
    return valid() && factory_generation == factory_generation_ &&
           hierarchy_epoch == hierarchy_epoch_;
  }

  // Pure depth-first preorder of construction links. It does not apply the
  // later native candidate/hide/component filters and therefore is not a draw
  // traversal or active-window decision.
  [[nodiscard]] std::vector<std::uint64_t> construction_preorder() const {
    if (!valid())
      throw std::runtime_error("Startup window hierarchy snapshot is invalid");
    std::unordered_map<std::uint64_t, const StartupWindowHierarchySnapshotNode *>
        lookup;
    lookup.reserve(nodes_.size());
    for (const auto &node : nodes_)
      if (!lookup.emplace(node.identity, &node).second)
        throw std::runtime_error("Startup window hierarchy snapshot is duplicated");
    std::vector<std::uint64_t> result;
    result.reserve(nodes_.size());
    std::unordered_set<std::uint64_t> seen;
    const auto visit = [&](auto &&self, std::uint64_t identity) -> void {
      if (!seen.insert(identity).second)
        throw std::runtime_error("Startup window hierarchy snapshot is cyclic");
      const auto found = lookup.find(identity);
      if (found == lookup.end())
        throw std::runtime_error("Startup window hierarchy snapshot link is absent");
      result.push_back(identity);
      for (auto child = found->second->first_child_identity; child;) {
        const auto child_found = lookup.find(*child);
        if (child_found == lookup.end())
          throw std::runtime_error("Startup window hierarchy child is absent");
        self(self, *child);
        child = child_found->second->next_sibling_identity;
      }
    };
    visit(visit, root_identity_);
    if (result.size() != nodes_.size())
      throw std::runtime_error("Startup window hierarchy snapshot scope is partial");
    return result;
  }

private:
  friend class StartupWindowHierarchyFactory;
  StartupWindowHierarchySnapshot(std::shared_ptr<const void> lifetime,
                                 std::uint64_t root_identity,
                                 std::uint64_t factory_generation,
                                 std::uint64_t hierarchy_epoch,
                                 std::vector<StartupWindowHierarchySnapshotNode> nodes)
      : lifetime_(std::move(lifetime)), root_identity_(root_identity),
        factory_generation_(factory_generation), hierarchy_epoch_(hierarchy_epoch),
        nodes_(std::move(nodes)) {}

  std::shared_ptr<const void> lifetime_;
  std::uint64_t root_identity_{};
  std::uint64_t factory_generation_{};
  std::uint64_t hierarchy_epoch_{};
  std::vector<StartupWindowHierarchySnapshotNode> nodes_;
};

class StartupWindowHierarchyFactory final {
public:
  [[nodiscard]] StartupWindowHierarchySnapshot capture(
      const StartupWindowHierarchyLease &lease, std::uint64_t factory_generation,
      const StartupWindowHierarchySourceScope &scope,
      const StartupWindowHierarchySnapshotServices &services) const {
    if (!lease.lifetime_ || factory_generation == 0U ||
        !scope.complete_directory_mapping || scope.nodes.empty() ||
        !services.begin_read_guard || !services.end_read_guard ||
        !services.hierarchy_epoch || !services.factory_generation_live ||
        !services.read_factory_proven_node)
      throw std::runtime_error("Startup window hierarchy requires factory services");
    if (!services.factory_generation_live(factory_generation))
      throw std::runtime_error("Startup window hierarchy factory generation is stale");

    const auto source = validate_source_scope(scope);
    const auto guard = services.begin_read_guard();
    if (!guard || *guard == 0U)
      throw std::runtime_error("Startup window hierarchy read guard is unavailable");
    struct Guard final {
      const StartupWindowHierarchySnapshotServices &services;
      std::uint64_t value;
      ~Guard() noexcept {
        try {
          services.end_read_guard(value);
        } catch (...) {
          // Guard release is best-effort during stack unwinding. The factory
          // service itself owns reporting/recovery for a failed release.
        }
      }
    } guard_cleanup{services, *guard};

    const auto epoch = services.hierarchy_epoch();
    if (epoch == 0U)
      throw std::runtime_error("Startup window hierarchy epoch is invalid");
    std::vector<StartupWindowHierarchySnapshotNode> nodes;
    nodes.reserve(scope.nodes.size());
    for (const auto &source_node : scope.nodes) {
      const auto live = services.read_factory_proven_node(source_node.directory_index);
      if (!live || live->identity == 0U ||
          live->directory_index != source_node.directory_index)
        throw std::runtime_error("Startup window hierarchy has no proven runtime node");
      nodes.push_back({live->identity, live->directory_index,
                       live->parent_identity, live->family, 0U,
                       live->first_child_identity, live->next_sibling_identity});
    }
    if (!services.factory_generation_live(factory_generation) ||
        services.hierarchy_epoch() != epoch)
      throw std::runtime_error("Startup window hierarchy changed during capture");

    const auto root = validate_live_links(source, scope.root_directory_index, nodes);
    return StartupWindowHierarchySnapshot(lease.lifetime_, root, factory_generation,
                                          epoch, std::move(nodes));
  }

private:
  using SourceMap = std::unordered_map<std::size_t,
                                       const StartupWindowHierarchySourceNode *>;
  using NodeMap = std::unordered_map<std::uint64_t,
                                     StartupWindowHierarchySnapshotNode *>;

  [[nodiscard]] static SourceMap validate_source_scope(
      const StartupWindowHierarchySourceScope &scope) {
    SourceMap source;
    source.reserve(scope.nodes.size());
    for (const auto &node : scope.nodes)
      if (!source.emplace(node.directory_index, &node).second)
        throw std::runtime_error("Startup window hierarchy source identity is duplicated");
    const auto root = source.find(scope.root_directory_index);
    if (root == source.end() || root->second->parent_directory_index)
      throw std::runtime_error("Startup window hierarchy source root is invalid");
    std::unordered_map<std::size_t, std::size_t> child_count;
    for (const auto &parent : scope.nodes) {
      std::unordered_set<std::size_t> siblings;
      for (const auto child : parent.children_in_directory_order) {
        const auto found = source.find(child);
        if (found == source.end() || !siblings.insert(child).second ||
            !found->second->parent_directory_index ||
            *found->second->parent_directory_index != parent.directory_index)
          throw std::runtime_error("Startup window hierarchy source child is invalid");
        ++child_count[child];
      }
    }
    for (const auto &node : scope.nodes) {
      const auto expected = node.parent_directory_index ? 1U : 0U;
      if (child_count[node.directory_index] != expected)
        throw std::runtime_error("Startup window hierarchy source scope is partial");
    }
    std::unordered_set<std::size_t> reached;
    const auto visit = [&](auto &&self, std::size_t directory) -> void {
      if (!reached.insert(directory).second)
        throw std::runtime_error("Startup window hierarchy source scope is cyclic");
      for (const auto child : source.at(directory)->children_in_directory_order)
        self(self, child);
    };
    visit(visit, scope.root_directory_index);
    if (reached.size() != source.size())
      throw std::runtime_error("Startup window hierarchy source scope is disconnected");
    return source;
  }

  [[nodiscard]] static std::uint64_t validate_live_links(
      const SourceMap &source, std::size_t root_directory_index,
      std::vector<StartupWindowHierarchySnapshotNode> &nodes) {
    NodeMap live;
    live.reserve(nodes.size());
    std::unordered_map<std::size_t, StartupWindowHierarchySnapshotNode *> by_source;
    by_source.reserve(nodes.size());
    for (auto &node : nodes) {
      if (node.identity == 0U || !live.emplace(node.identity, &node).second ||
          !by_source.emplace(node.directory_index, &node).second)
        throw std::runtime_error("Startup window hierarchy runtime identity is invalid");
    }
    const auto root_source = source.find(root_directory_index);
    const auto root_node = by_source.find(root_directory_index);
    if (root_source == source.end() || root_node == by_source.end() ||
        root_node->second->parent_identity || root_node->second->next_sibling_identity)
      throw std::runtime_error("Startup window hierarchy runtime root is invalid");

    for (const auto &[directory, source_node] : source) {
      auto *const parent = by_source.at(directory);
      std::vector<StartupWindowHierarchySnapshotNode *> containers;
      std::vector<StartupWindowHierarchySnapshotNode *> leaves;
      containers.reserve(source_node->children_in_directory_order.size());
      leaves.reserve(source_node->children_in_directory_order.size());
      for (std::size_t ordinal = 0; ordinal < source_node->children_in_directory_order.size();
           ++ordinal) {
        auto *const child = by_source.at(source_node->children_in_directory_order[ordinal]);
        if (!child->parent_identity || *child->parent_identity != parent->identity)
          throw std::runtime_error("Startup window hierarchy runtime parent disagrees with source");
        child->source_sibling_ordinal = ordinal;
        (child->family == StartupWindowNodeFamily::container ? containers : leaves)
            .push_back(child);
      }
      std::reverse(containers.begin(), containers.end());
      containers.insert(containers.end(), leaves.begin(), leaves.end());
      const auto expected_first = containers.empty()
                                      ? std::optional<std::uint64_t>{}
                                      : std::optional<std::uint64_t>{containers.front()->identity};
      if (parent->first_child_identity != expected_first)
        throw std::runtime_error("Startup window hierarchy live child head disagrees with attachment policy");
      for (std::size_t index = 0; index < containers.size(); ++index) {
        const auto expected_next = index + 1U == containers.size()
                                       ? std::optional<std::uint64_t>{}
                                       : std::optional<std::uint64_t>{containers[index + 1U]->identity};
        if (containers[index]->next_sibling_identity != expected_next)
          throw std::runtime_error("Startup window hierarchy live sibling chain disagrees with attachment policy");
      }
    }
    return root_node->second->identity;
  }
};

} // namespace off::runtime
