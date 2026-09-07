#pragma once

#include "off/graphics/startup_graphics_expanded_plan.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace off::graphics {

// Stable live identities captured by the scene/window owner. These are not
// parsed directory offsets and cannot be synthesized from a picture name.
enum class StartupLiveWindowKind : std::uint8_t { container, leaf };

struct StartupLiveWindowNode {
  std::uint64_t identity{};
  std::optional<std::size_t> picture_directory_index;
  StartupLiveWindowKind kind{StartupLiveWindowKind::leaf};
  // Ordinal in the source parent's child sequence. It validates the recovered
  // head-inserted container / tail-appended leaf construction policy.
  std::size_t source_sibling_ordinal{};
  bool linked{};
  bool candidate_eligible{};
  bool has_render_component{};
  std::uint32_t hide_control_bits{};
  std::optional<std::uint64_t> first_child;
  std::optional<std::uint64_t> next_sibling;
};

struct StartupPictureOwnerView {
  std::size_t picture_directory_index{};
  std::uint64_t owner_identity{};
  std::uint64_t admitted_view_identity{};
};

struct StartupSourcePictureTraversalSnapshot {
  std::uint64_t root_identity{};
  std::uint64_t pass_context_identity{};
  std::uint32_t pass_value{};
  std::span<const StartupLiveWindowNode> nodes;
  std::span<const StartupPictureOwnerView> owner_views;
};

struct StartupSourcePictureBackendHooks {
  // Every hook is a synchronous live-service boundary. A false result stops
  // before any later stage; callback exceptions retain their completed prefix.
  std::function<bool()> device_scene_admitted;
  std::function<bool()> matched_state_available;
  std::function<bool(const StartupGraphicsPreparedPicture&,
                     const StartupPictureOwnerView&,
                     std::uint64_t pass_context)> prepare_record;
  std::function<bool()> preselection_complete;
  std::function<bool(std::size_t resource_index, std::uint32_t texture_id)>
      texture_resident;
  // The complete source-backed ordered batch is supplied after all record
  // preparations and preselection. The hook owns state/key ordering policy.
  std::function<bool(std::span<const StartupGraphicsExpandedSubmission>)>
      ordered_draw_admitted;
  std::function<bool(const StartupGraphicsExpandedSubmission&,
                     const StartupPictureOwnerView&,
                     std::uint64_t pass_context)> submit;
};

struct StartupSourcePictureDrawResult {
  std::size_t prepared_picture_count{};
  std::size_t submitted_group_count{};
};

// Conditional source-backed submission only. This neither selects the startup
// root, creates a camera/viewport, derives material state nor presents pixels.
class StartupSourcePictureDrawAdmission final {
public:
  StartupSourcePictureDrawAdmission() = default;
  StartupSourcePictureDrawAdmission(const StartupSourcePictureDrawAdmission&) = delete;
  StartupSourcePictureDrawAdmission& operator=(const StartupSourcePictureDrawAdmission&) = delete;
  StartupSourcePictureDrawAdmission(StartupSourcePictureDrawAdmission&&) = delete;
  StartupSourcePictureDrawAdmission& operator=(StartupSourcePictureDrawAdmission&&) = delete;

  [[nodiscard]] StartupSourcePictureDrawResult submit(
      const StartupGraphicsAsset& asset, std::uint8_t requested_state,
      const StartupSourcePictureTraversalSnapshot& traversal,
      std::span<const StartupGraphicsPictureTransform> transforms,
      const StartupSourcePictureBackendHooks& hooks) {
    if (running_) throw std::runtime_error("startup source picture draw is reentrant");
    if (!hooks.device_scene_admitted || !hooks.matched_state_available ||
        !hooks.prepare_record || !hooks.preselection_complete ||
        !hooks.texture_resident || !hooks.ordered_draw_admitted || !hooks.submit)
      throw std::runtime_error("startup source picture draw requires complete backend hooks");
    if (traversal.root_identity == 0U || traversal.pass_context_identity == 0U)
      throw std::runtime_error("startup source picture draw requires live root and pass context");

    const auto prepared = prepare_startup_graphics_plan(asset, requested_state);
    const auto expanded = expand_startup_graphics_plan(prepared, transforms);
    const auto nodes = validate_nodes(traversal);
    const auto owner_views = validate_owner_views(traversal, nodes, prepared);
    const auto reached = preorder_picture_leaves(traversal, nodes);
    if (reached.size() != prepared.pictures().size())
      throw std::runtime_error("startup live traversal picture count disagrees with source plan");
    for (std::size_t index = 0; index < reached.size(); ++index)
      if (reached[index] != prepared.pictures()[index].picture_directory_index)
        throw std::runtime_error("startup live traversal order disagrees with source plan");

    // All pure/caller-supplied data is validated before querying the backend.
    for (const auto& submission : expanded.submissions()) {
      if (submission.resource_index >= expanded.resources().size())
        throw std::runtime_error("startup expanded submission has an unknown resource");
      if (!owner_views.contains(submission.picture_directory_index))
        throw std::runtime_error("startup submission lacks an admitted owner/view association");
    }

    struct Guard {
      bool& running;
      explicit Guard(bool& value) : running(value) { running = true; }
      ~Guard() { running = false; }
    } guard(running_);
    if (!hooks.device_scene_admitted())
      throw std::runtime_error("startup source picture device scene is not admitted");
    if (!hooks.matched_state_available())
      throw std::runtime_error("startup source picture matched state is unavailable");
    for (const auto& picture : prepared.pictures()) {
      const auto found = owner_views.find(picture.picture_directory_index);
      if (found == owner_views.end() ||
          !hooks.prepare_record(picture, found->second, traversal.pass_context_identity))
        throw std::runtime_error("startup source picture record preparation was rejected");
    }
    if (!hooks.preselection_complete())
      throw std::runtime_error("startup source picture preselection was rejected");
    for (const auto& resource : expanded.resources())
      if (!hooks.texture_resident(resource.resource_index, resource.texture_id))
        throw std::runtime_error("startup source picture texture is unavailable");
    if (!hooks.ordered_draw_admitted(expanded.submissions()))
      throw std::runtime_error("startup source picture ordered draw was rejected");
    for (const auto& submission : expanded.submissions()) {
      const auto found = owner_views.find(submission.picture_directory_index);
      if (found == owner_views.end() ||
          !hooks.submit(submission, found->second, traversal.pass_context_identity))
        throw std::runtime_error("startup source picture backend submission was rejected");
    }
    return {prepared.pictures().size(), expanded.submissions().size()};
  }

private:
  using NodeMap = std::unordered_map<std::uint64_t, const StartupLiveWindowNode*>;
  using OwnerViewMap = std::unordered_map<std::size_t, StartupPictureOwnerView>;

  [[nodiscard]] static NodeMap validate_nodes(
      const StartupSourcePictureTraversalSnapshot& traversal) {
    NodeMap result;
    result.reserve(traversal.nodes.size());
    for (const auto& node : traversal.nodes) {
      if (node.identity == 0U || !result.emplace(node.identity, &node).second)
        throw std::runtime_error("startup live traversal node identity is invalid");
      if (node.picture_directory_index && node.kind != StartupLiveWindowKind::leaf)
        throw std::runtime_error("startup picture node is not a leaf");
    }
    if (!result.contains(traversal.root_identity))
      throw std::runtime_error("startup live traversal root is absent");
    for (const auto& node : traversal.nodes) {
      for (const auto link : {node.first_child, node.next_sibling})
        if (link && !result.contains(*link))
          throw std::runtime_error("startup live traversal link is absent");
    }
    return result;
  }

  [[nodiscard]] static OwnerViewMap validate_owner_views(
      const StartupSourcePictureTraversalSnapshot& traversal, const NodeMap& nodes,
      const StartupGraphicsPreparedPlan& prepared) {
    OwnerViewMap result;
    result.reserve(traversal.owner_views.size());
    for (const auto& association : traversal.owner_views) {
      const auto node = nodes.find(association.owner_identity);
      if (association.owner_identity == 0U || association.admitted_view_identity == 0U ||
          node == nodes.end() || !node->second->picture_directory_index ||
          *node->second->picture_directory_index != association.picture_directory_index ||
          !result.emplace(association.picture_directory_index, association).second)
        throw std::runtime_error("startup owner/view association is invalid");
    }
    if (result.size() != prepared.pictures().size())
      throw std::runtime_error("startup owner/view association set is incomplete");
    for (const auto& picture : prepared.pictures())
      if (!result.contains(picture.picture_directory_index))
        throw std::runtime_error("startup picture has no owner/view association");
    return result;
  }

  [[nodiscard]] static std::vector<std::size_t> preorder_picture_leaves(
      const StartupSourcePictureTraversalSnapshot& traversal, const NodeMap& nodes) {
    constexpr std::uint32_t hide_mask = 0x2c00U;
    std::vector<std::size_t> result;
    std::unordered_set<std::uint64_t> reached;
    const auto visit = [&](auto&& self, std::uint64_t identity) -> void {
      const auto node = nodes.at(identity);
      if (!reached.insert(identity).second)
        throw std::runtime_error("startup live traversal contains a cycle or shared child");
      std::optional<StartupLiveWindowKind> previous_kind;
      std::size_t previous_ordinal{};
      std::unordered_set<std::uint64_t> sibling_seen;
      for (auto child = node->first_child; child; child = nodes.at(*child)->next_sibling) {
        if (!sibling_seen.insert(*child).second)
          throw std::runtime_error("startup live traversal sibling chain is cyclic");
        const auto child_node = nodes.at(*child);
        if (previous_kind) {
          if (*previous_kind == StartupLiveWindowKind::leaf &&
              child_node->kind == StartupLiveWindowKind::container)
            throw std::runtime_error("startup live sibling partition is invalid");
          if (child_node->kind == *previous_kind &&
              ((child_node->kind == StartupLiveWindowKind::container &&
                child_node->source_sibling_ordinal >= previous_ordinal) ||
               (child_node->kind == StartupLiveWindowKind::leaf &&
                child_node->source_sibling_ordinal <= previous_ordinal)))
            throw std::runtime_error("startup live sibling order disagrees with construction policy");
        }
        previous_kind = child_node->kind;
        previous_ordinal = child_node->source_sibling_ordinal;
      }
      const bool accepted = node->linked && node->candidate_eligible &&
          node->has_render_component && (node->hide_control_bits & hide_mask) == 0U;
      if (accepted && node->picture_directory_index)
        result.push_back(*node->picture_directory_index);
      for (auto child = node->first_child; child; child = nodes.at(*child)->next_sibling)
        self(self, *child);
    };
    visit(visit, traversal.root_identity);
    return result;
  }

  bool running_{false};
};

}  // namespace off::graphics
