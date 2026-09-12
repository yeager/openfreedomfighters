#include "off/graphics/movie_control_host_evidence.hpp"

#include "off/graphics/intro_runtime.hpp"

#include <stdexcept>

namespace off::graphics {
namespace {
[[nodiscard]] MovieControlHostReaderReceipt owner_receipt(
    const IntroMovieControllerReaderState& state) {
  return {.owner_handle = state.owner.value,
          .resource_handle = state.resource.value,
          .source_directory_index = state.source_directory_index,
          .source_offset = state.source_offset,
          .component_index = state.component_index};
}
[[nodiscard]] MovieControlHostReaderReceipt component_receipt(
    const IntroMovieControllerComponentReaderState& state) {
  return {.owner_handle = state.owner.value,
          .resource_handle = state.resource.value,
          .source_directory_index = state.source_directory_index,
          .source_offset = state.source_offset,
          .component_index = state.component_index,
          .class_ordinal = state.class_ordinal,
          .requested_mask = state.requested_mask,
          .priority = state.priority,
          .events = state.events};
}
}

MovieControlHostEvidence MovieControlHostEvidence::from_reader_receipts(
    const MovieControlHostReaderReceipt& owner_reader,
    const MovieControlHostReaderReceipt& component_reader,
    std::uint64_t live_component_handle) {
  if (!live_component_handle || !owner_reader.owner_handle ||
      !owner_reader.resource_handle || owner_reader.source_offset == 0 ||
      owner_reader.owner_handle != component_reader.owner_handle ||
      owner_reader.resource_handle != component_reader.resource_handle ||
      owner_reader.source_directory_index != component_reader.source_directory_index ||
      owner_reader.source_offset != component_reader.source_offset ||
      owner_reader.component_index != component_reader.component_index)
    throw std::runtime_error("MovieControl host evidence requires matching reader receipts");
  return {live_component_handle, owner_reader.owner_handle,
          owner_reader.resource_handle, owner_reader.source_directory_index};
}

MovieControlHostEvidence MovieControlHostEvidence::from_runtime(
    const IntroRuntime& runtime) {
  if (runtime.reader_bracket_stage() !=
      IntroReaderBracketStage::ordinary_reader_boundary_complete)
    throw std::runtime_error("MovieControl host evidence requires the complete reader bracket");
  const auto* owner = runtime.movie_controller_reader_state();
  const auto* component = runtime.movie_controller_component_reader_state();
  if (!owner || !component ||
      owner->component_index != runtime.controller_component_index())
    throw std::runtime_error("MovieControl host evidence requires both controller reader receipts");
  return from_reader_receipts(owner_receipt(*owner), component_receipt(*component),
                              runtime.component_handle(owner->component_index));
}
}  // namespace off::graphics
