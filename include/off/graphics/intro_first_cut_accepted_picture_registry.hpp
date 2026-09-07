#pragma once

#include "off/graphics/intro_accepted_picture_registry.hpp"

#include <stdexcept>

namespace off::graphics {

// The first-cut bridge may only retain an already accepted generic record when
// its separately supplied typed binding is the live legal picture recovered
// from the intro directory. Generic record fields remain opaque throughout.
class IntroFirstCutAcceptedPictureRegistry final {
public:
  explicit IntroFirstCutAcceptedPictureRegistry(const IntroRuntime& runtime) noexcept
      : runtime_(runtime) {}
  IntroFirstCutAcceptedPictureRegistry(const IntroFirstCutAcceptedPictureRegistry&) = delete;
  IntroFirstCutAcceptedPictureRegistry& operator=(const IntroFirstCutAcceptedPictureRegistry&) = delete;
  IntroFirstCutAcceptedPictureRegistry(IntroFirstCutAcceptedPictureRegistry&&) = delete;
  IntroFirstCutAcceptedPictureRegistry& operator=(IntroFirstCutAcceptedPictureRegistry&&) = delete;

  void begin_frame(std::uint64_t token) { records_.begin_frame(token); }

  // Call only after PictureRecordRebuild::accept has returned successfully for
  // `record`. This validates the typed source binding before delegating, but
  // cannot establish generic admission on behalf of its caller.
  void register_after_generic_accept(const PictureQueuedDrawRecord& record,
                                     IntroRuntimeHandle owner,
                                     IntroRuntimeResourceHandle resource) {
    validate_live_legal_picture(owner, resource);
    records_.register_accepted(record, owner, resource);
  }

  [[nodiscard]] const IntroAcceptedPictureRecord* resolve(
      const PictureQueuedDrawRecord& record) const noexcept {
    return records_.resolve(record);
  }
  void end_frame(std::uint64_t token) { records_.end_frame(token); }
  [[nodiscard]] bool active() const noexcept { return records_.active(); }
  [[nodiscard]] bool poisoned() const noexcept { return records_.poisoned(); }
  [[nodiscard]] std::size_t size() const noexcept { return records_.size(); }

private:
  void validate_live_legal_picture(IntroRuntimeHandle owner,
                                   IntroRuntimeResourceHandle resource) const {
    if(runtime_.resource_load_stage()!=IntroResourceLoadStage::directory_construction_complete)
      throw std::runtime_error("First-cut accepted-picture runtime is not constructed");
    const auto source=runtime_.resources().sources().local_source_for_authored_reference(
        runtime_.resources().member().references[1]);
    if(!source || *source>=runtime_.resources().sources().directory().size())
      throw std::runtime_error("First-cut accepted-picture legal source is unavailable");
    const auto expected_owner=runtime_.source_handle(*source);
    if(owner!=expected_owner || runtime_.picture_for_source(*source).handle()!=expected_owner)
      throw std::runtime_error("First-cut accepted-picture owner is not the legal picture");
    const auto expected_resource=runtime_.resource_handle(expected_owner);
    const auto associated_owner=runtime_.associated_resource_owner(resource);
    if(resource!=expected_resource || !associated_owner || *associated_owner!=expected_owner)
      throw std::runtime_error("First-cut accepted-picture resource is not the legal picture resource");
  }

  const IntroRuntime& runtime_;
  IntroAcceptedPictureRecordRegistry records_;
};

} // namespace off::graphics
