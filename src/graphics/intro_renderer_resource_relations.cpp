#include "off/graphics/intro_renderer_resource_relations.hpp"

#include <limits>
#include <stdexcept>

namespace off::graphics {
IntroRendererResourceRelations::IntroRendererResourceRelations(
    std::span<const std::byte> payload)
    : workspace_(IntroRendererPayloadWorkspace::from_prepared(
          {{payload.begin(), payload.end()}, parse_intro_renderer_relocation_prefix(payload)})) {}

void IntroRendererResourceRelations::read(const Resolve& resolve, std::size_t max_members) {
  if (loaded_ || reading_ || !resolve)
    throw std::runtime_error("Renderer relations require an unused container and resource resolver");
  struct Guard {
    bool& flag;
    explicit Guard(bool& value) : flag(value) { flag = true; }
    ~Guard() { flag = false; }
  } guard(reading_);
  std::map<std::uint32_t, std::vector<std::uint64_t>> staged;
  std::size_t count{};
  constexpr std::uint32_t domain = 0x40000000U, bias = 0x60U, stride = 112U;
  const auto resolve_address = [&](std::uint32_t address) {
    if (!address || address >= domain - bias)
      throw std::runtime_error("Renderer relation reference is outside the source domain");
    if (count == max_members)
      throw std::runtime_error("Renderer relation expansion exceeds the native member limit");
    const auto resource = resolve((address + bias) | domain);
    if (!resource || !*resource)
      throw std::runtime_error("Renderer relation reference has no live resource");
    ++count;
    return *resource;
  };
  for (const auto& group : workspace_.prefix().groups) {
    const auto head = group.references.front().word_offset - 1U;
    if (head > std::numeric_limits<std::uint32_t>::max())
      throw std::runtime_error("Renderer relation selector overflows its word domain");
    std::vector<std::uint64_t> members;
    for (std::size_t index = 0; index < group.references.size(); ++index) {
      const auto& reference = group.references[index];
      const auto address = reference.address();
      if (!address) {
        if (group.references.size() != 1U || !reference.terminal() || (reference.tag() & 2U))
          throw std::runtime_error("Renderer relation mixed or ranged null form is unsupported");
        continue;
      }
      if (reference.tag() & 2U) {
        if (reference.terminal() || index + 1U >= group.references.size())
          throw std::runtime_error("Renderer relation range has no endpoint");
        const auto end = group.references[index + 1U].address();
        if (!end || end <= address || (end - address) % stride != 0U || end >= domain - bias)
          throw std::runtime_error("Renderer relation range is reversed, unaligned or outside its source domain");
        const auto expanded = static_cast<std::size_t>((end - address) / stride);
        if (expanded > max_members - count)
          throw std::runtime_error("Renderer relation range exceeds the native member limit");
        for (auto current = address; current < end; current += stride)
          members.push_back(resolve_address(current));
      } else {
        members.push_back(resolve_address(address));
      }
    }
    staged.emplace(static_cast<std::uint32_t>(head), std::move(members));
  }
  groups_.swap(staged);
  member_count_ = count;
  loaded_ = true;
}

bool IntroRendererResourceRelations::has_selector(std::uint32_t selector) const noexcept {
  return loaded_ && (selector == 0U || groups_.contains(selector));
}

std::optional<std::vector<std::uint64_t>> IntroRendererResourceRelations::members(
    std::uint32_t selector, const IsLive& live) const {
  if (!loaded_ || reading_ || !live || !has_selector(selector))
    throw std::runtime_error("Renderer relation query requires loaded storage and a valid live selector");
  if (!selector) return std::nullopt;
  const auto& members = groups_.at(selector);
  for (const auto resource : members)
    if (!live(resource)) throw std::runtime_error("Renderer relation member is no longer live");
  return members;
}
} // namespace off::graphics
