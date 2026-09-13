#include "off/graphics/first_cut_fade_phase_one_session.hpp"

#include "off/graphics/intro_runtime.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace off::graphics {

FirstCutFadePhaseOneSession::FirstCutFadePhaseOneSession(IntroRuntime& runtime)
    : runtime_(std::addressof(runtime)) {
  // Component construction is reverse directory order in the reviewed path.
  // Preserve that retained factory order explicitly instead of treating the
  // map's ascending key order as a new lifecycle rule.
  for (const auto& [source, reader] : runtime.fade_picture_reader_states()) {
    const auto attachment = runtime.fade_picture_component_reader_states().find(source);
    if (attachment == runtime.fade_picture_component_reader_states().end() ||
        attachment->second.component_index != reader.component_index ||
        attachment->second.owner != reader.owner || attachment->second.resource != reader.resource)
      throw std::runtime_error("first-cut fade diagnostic requires matching retained receipts");
    targets_.push_back({source, reader.component_index, reader.owner, reader.resource});
  }
  std::ranges::sort(targets_, std::greater{}, &FirstCutFadePhaseOneReceipt::source_directory_index);
  if (targets_.size() != 3U)
    throw std::runtime_error("first-cut fade diagnostic requires exactly three retained receipts");
}

void FirstCutFadePhaseOneSession::execute(IntroFadePicturePhaseOneServices services) {
  if (stage_ != FirstCutFadePhaseOneSessionStage::cold || !runtime_ ||
      !services.engine_dimensions || !services.invalidate_resource)
    throw std::runtime_error("first-cut fade diagnostic pass is unavailable");
  stage_ = FirstCutFadePhaseOneSessionStage::executing;
  try {
    for (const auto& target : targets_) {
      runtime_->run_isolated_first_cut_fade_phase_one(
          target.component_index, services);
      receipts_.push_back(target);
    }
    stage_ = FirstCutFadePhaseOneSessionStage::complete;
  } catch (...) {
    stage_ = FirstCutFadePhaseOneSessionStage::failed;
    throw;
  }
}

} // namespace off::graphics
