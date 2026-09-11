#pragma once

#include "off/graphics/intro_renderer_payload_workspace.hpp"
#include <functional>
#include <map>

namespace off::graphics {

// Scene-owned initial renderer-container relation storage. Members are borrowed
// canonical resource identities, not owner handles or generic draw records.
// Dynamic relation mutations and the separate outer-tail List associations are
// not implemented by this initial-load/query boundary.
class IntroRendererResourceRelations final {
public:
  using Resolve = std::function<std::optional<std::uint64_t>(std::uint32_t)>;
  using IsLive = std::function<bool(std::uint64_t)>;

  explicit IntroRendererResourceRelations(std::span<const std::byte> payload);
  // The resolver accepts original marked source-slot keys. Native identities
  // are never incremented to expand ranges. Publish all groups atomically.
  void read(const Resolve& resolve, std::size_t max_members = 1'000'000U);
  [[nodiscard]] bool loaded() const noexcept { return loaded_; }
  [[nodiscard]] bool has_selector(std::uint32_t selector) const noexcept;
  // Caller supplies the resource's CURRENT selector and live-resource lookup.
  // The returned copy survives another query, unlike the original scratch view.
  // Selector zero is unassigned (nullopt), distinct from a bound empty group.
  [[nodiscard]] std::optional<std::vector<std::uint64_t>> members(
      std::uint32_t selector, const IsLive& live) const;
  [[nodiscard]] std::size_t group_count() const noexcept { return groups_.size(); }
  [[nodiscard]] std::size_t member_count() const noexcept { return member_count_; }
  [[nodiscard]] const IntroRendererPayloadWorkspace& workspace() const noexcept {
    return workspace_;
  }

private:
  IntroRendererPayloadWorkspace workspace_;
  std::map<std::uint32_t, std::vector<std::uint64_t>> groups_;
  std::size_t member_count_{};
  bool loaded_{}, reading_{};
};
} // namespace off::graphics
