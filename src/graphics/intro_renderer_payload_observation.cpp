#include "off/graphics/intro_renderer_payload_observation.hpp"

#include <stdexcept>

namespace off::graphics {

IntroRendererPayloadObservation observe_intro_renderer_payload(
    std::span<const std::byte> payload,
    const std::function<std::optional<std::uint32_t>(std::uint32_t)>& resolver) {
  if (!resolver)
    throw std::runtime_error("renderer payload observation requires a source-reference resolver");
  auto prepared = prepare_intro_renderer_relocation_payload(payload, resolver);
  IntroRendererPayloadObservation observation;
  observation.relocation_groups = prepared.prefix.groups.size();
  for (const auto& group : prepared.prefix.groups) {
    observation.relocation_references += group.references.size();
    for (const auto& reference : group.references)
      observation.resolved_references += reference.address() != 0U ? 1U : 0U;
  }
  const auto workspace = IntroRendererPayloadWorkspace::from_prepared(std::move(prepared));
  observation.eight_byte_slots = workspace.layout().eight_byte_slot_count;
  observation.sixteen_byte_slots = workspace.layout().sixteen_byte_slot_count;
  observation.trailing_bytes = workspace.layout().sixteen_byte_trailing_bytes;
  return observation;
}

} // namespace off::graphics
