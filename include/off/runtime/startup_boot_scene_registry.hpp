#pragma once

#include "off/runtime/startup_boot_scene_directory_source.hpp"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace off::runtime {

// Source-backed construction links for one FF-StartUp transaction. This is
// deliberately before component readers, active-root selection, input,
// visibility, camera, rendering and scene transitions.
struct StartupBootSceneRegistryNode final {
  std::uint64_t handle{};
  std::size_t source_directory_index{};
  std::optional<std::uint64_t> parent;
  std::vector<std::uint64_t> children_in_directory_order;
};

class StartupBootSceneRegistry final {
public:
  StartupBootSceneRegistry(const StartupBootSceneRegistry &) = delete;
  StartupBootSceneRegistry &operator=(const StartupBootSceneRegistry &) = delete;
  StartupBootSceneRegistry(StartupBootSceneRegistry &&) noexcept = default;
  StartupBootSceneRegistry &operator=(StartupBootSceneRegistry &&) noexcept = default;

  [[nodiscard]] bool valid() const noexcept {
    return package_ && lifetime_ && generation_ != 0U && epoch_ != 0U &&
           root_ != 0U && boot_menu_owner_ != 0U && !nodes_.empty();
  }
  [[nodiscard]] std::uint64_t generation() const noexcept { return generation_; }
  [[nodiscard]] std::uint64_t epoch() const noexcept { return epoch_; }
  [[nodiscard]] std::uint64_t root() const noexcept { return root_; }
  [[nodiscard]] std::uint64_t boot_menu_owner() const noexcept {
    return boot_menu_owner_;
  }
  [[nodiscard]] float boot_menu_parameter() const noexcept {
    return boot_menu_parameter_;
  }
  [[nodiscard]] std::span<const StartupBootSceneRegistryNode> nodes() const noexcept {
    return nodes_;
  }
  [[nodiscard]] bool contains(std::uint64_t handle) const noexcept {
    return by_handle_.contains(handle);
  }
  [[nodiscard]] std::optional<std::uint64_t>
  handle_for_source_directory(std::size_t source_directory_index) const noexcept {
    const auto found = by_source_.find(source_directory_index);
    return found == by_source_.end() ? std::nullopt
                                     : std::optional<std::uint64_t>{found->second};
  }
  [[nodiscard]] const StartupBootSceneRegistryNode &node(std::uint64_t handle) const {
    const auto found = by_handle_.find(handle);
    if (found == by_handle_.end())
      throw std::runtime_error("startup boot scene registry handle is absent");
    return nodes_.at(found->second);
  }

private:
  friend class StartupBootSceneRegistryFactory;
  StartupBootSceneRegistry(std::shared_ptr<const StartupSceneLoadPackage> package,
                           std::shared_ptr<const void> lifetime,
                           std::uint64_t generation, std::uint64_t epoch,
                           std::uint64_t root, std::uint64_t boot_menu_owner,
                           float boot_menu_parameter,
                           std::vector<StartupBootSceneRegistryNode> nodes,
                           std::unordered_map<std::size_t, std::uint64_t> by_source,
                           std::unordered_map<std::uint64_t, std::size_t> by_handle)
      : package_(std::move(package)), lifetime_(std::move(lifetime)),
        generation_(generation), epoch_(epoch), root_(root),
        boot_menu_owner_(boot_menu_owner), boot_menu_parameter_(boot_menu_parameter),
        nodes_(std::move(nodes)), by_source_(std::move(by_source)),
        by_handle_(std::move(by_handle)) {}

  std::shared_ptr<const StartupSceneLoadPackage> package_;
  std::shared_ptr<const void> lifetime_;
  std::uint64_t generation_{};
  std::uint64_t epoch_{};
  std::uint64_t root_{};
  std::uint64_t boot_menu_owner_{};
  float boot_menu_parameter_{};
  std::vector<StartupBootSceneRegistryNode> nodes_;
  std::unordered_map<std::size_t, std::uint64_t> by_source_;
  std::unordered_map<std::uint64_t, std::size_t> by_handle_;
};

class StartupBootSceneRegistryFactory final {
public:
  [[nodiscard]] StartupBootSceneRegistry construct(
      std::shared_ptr<const StartupSceneLoadPackage> package,
      const StartupBootSceneDirectorySource &directory,
      const StartupBootSceneLease &scene, std::uint64_t generation) const {
    if (!package || !package->factory_inputs() || !scene.lifetime_ || generation == 0U ||
        !directory.matches_checked_gms(package->factory_inputs()->gms()))
      throw std::runtime_error("startup boot scene registry requires checked provenance");
    const auto scope = directory.hierarchy_scope();
    if (!scope.complete_directory_mapping || scope.nodes.empty())
      throw std::runtime_error("startup boot scene registry hierarchy is incomplete");
    const auto epoch = next_epoch_.fetch_add(1U);
    if (epoch == 0U)
      throw std::runtime_error("startup boot scene registry epoch exhausted");
    std::unordered_map<std::size_t, std::uint64_t> by_source;
    by_source.reserve(scope.nodes.size());
    std::vector<StartupBootSceneRegistryNode> nodes;
    nodes.reserve(scope.nodes.size());
    for (const auto &source : scope.nodes) {
      const auto handle = next_handle_.fetch_add(1U);
      if (handle == 0U || handle == source.directory_index ||
          !by_source.emplace(source.directory_index, handle).second)
        throw std::runtime_error("startup boot scene registry source mapping is invalid");
      nodes.push_back({handle, source.directory_index, std::nullopt, {}});
    }
    const auto source_handle = [&](std::size_t index) -> std::uint64_t {
      const auto found = by_source.find(index);
      if (found == by_source.end())
        throw std::runtime_error("startup boot scene registry link escapes scope");
      return found->second;
    };
    std::unordered_map<std::uint64_t, std::size_t> by_handle;
    by_handle.reserve(nodes.size());
    for (std::size_t index{}; index < nodes.size(); ++index)
      by_handle.emplace(nodes[index].handle, index);
    for (std::size_t index{}; index < scope.nodes.size(); ++index) {
      const auto &source = scope.nodes[index];
      auto &node = nodes[index];
      if (source.parent_directory_index)
        node.parent = source_handle(*source.parent_directory_index);
      node.children_in_directory_order.reserve(source.children_in_directory_order.size());
      for (const auto child : source.children_in_directory_order)
        if (const auto child_node = std::find_if(
                scope.nodes.begin(), scope.nodes.end(), [&](const auto &candidate) {
                  return candidate.directory_index == child;
                });
            child_node == scope.nodes.end() ||
            child_node->parent_directory_index != source.directory_index)
          throw std::runtime_error("startup boot scene registry child link is invalid");
      for (const auto child : source.children_in_directory_order)
        node.children_in_directory_order.push_back(source_handle(child));
    }
    const auto root = source_handle(scope.root_directory_index);
    if (nodes.at(by_handle.at(root)).parent ||
        !by_source.contains(directory.boot_owner_directory_index()))
      throw std::runtime_error("startup boot scene registry root or BootMenu is invalid");
    const auto boot_menu_owner =
        source_handle(directory.boot_owner_directory_index());
    return StartupBootSceneRegistry(std::move(package), scene.lifetime_, generation,
                                    epoch, root, boot_menu_owner,
                                    directory.proof().component_parameter,
                                    std::move(nodes), std::move(by_source),
                                    std::move(by_handle));
  }

private:
  inline static std::atomic<std::uint64_t> next_handle_{0x100000000ULL};
  inline static std::atomic<std::uint64_t> next_epoch_{1U};
};

} // namespace off::runtime
