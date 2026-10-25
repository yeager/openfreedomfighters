#include "off/graphics/first_cut_fade_phase_one_session.hpp"

#include "off/graphics/intro_runtime.hpp"

#include <algorithm>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace off::graphics {

FirstCutFadePhaseOneSession::FirstCutFadePhaseOneSession(IntroRuntime& runtime)
    : runtime_(std::addressof(runtime)) {
  constexpr std::uint32_t picture_source_type = 0x00200046U;
  const auto& sources = runtime.resources().sources();
  const auto& directory = sources.directory();
  std::vector<std::size_t> expected_sources;
  for (const auto& command : runtime.resources().first_cut().commands) {
    const auto source =
        sources.local_source_for_authored_reference(command.target_reference);
    if (!source || *source >= directory.size())
      throw std::runtime_error(
          "first-cut fade diagnostic has an unresolved command target");
    const auto& row = directory[*source];
    if (row.source_type == picture_source_type && row.attachments.size() == 1U &&
        row.attachments[0].parameter == 0.0F &&
        sources.attachment_identifier(*source, 0U) == "ZWINPIC_FadeToBlack")
      expected_sources.push_back(*source);
  }
  std::ranges::sort(expected_sources);
  expected_sources.erase(
      std::unique(expected_sources.begin(), expected_sources.end()),
      expected_sources.end());
  const auto& readers = runtime.fade_picture_reader_states();
  const auto& components = runtime.fade_picture_component_reader_states();
  if (expected_sources.empty() || readers.size() != expected_sources.size() ||
      components.size() != expected_sources.size())
    throw std::runtime_error(
        "first-cut fade diagnostic receipt set is incomplete");

  // Component construction is reverse directory order in the reviewed path.
  // Preserve that retained factory order explicitly instead of treating the
  // map's ascending key order as a new lifecycle rule.
  std::unordered_set<std::size_t> seen;
  for (const auto& [source, reader] : readers) {
    if (!std::ranges::binary_search(expected_sources, source) ||
        !seen.insert(source).second)
      throw std::runtime_error(
          "first-cut fade diagnostic has an unexpected receipt source");
    const auto attachment = components.find(source);
    if (attachment == components.end() ||
        attachment->second.component_index != reader.component_index ||
        attachment->second.owner != reader.owner ||
        attachment->second.resource != reader.resource ||
        attachment->second.source_directory_index != source ||
        attachment->second.authored.authored_state_exponent !=
            reader.authored.authored_state_exponent ||
        attachment->second.authored.base_render_property !=
            reader.authored.base_render_property ||
        attachment->second.authored.authored_alpha !=
            reader.authored.authored_alpha ||
        attachment->second.authored.alignment_enum !=
            reader.authored.alignment_enum ||
        attachment->second.authored.extension_control !=
            reader.authored.extension_control ||
        attachment->second.authored.picture_asset_reference !=
            reader.authored.picture_asset_reference ||
        attachment->second.picture_asset_reference !=
            reader.picture_asset_reference ||
        reader.owner != runtime.source_handle(source))
      throw std::runtime_error("first-cut fade diagnostic requires matching retained receipts");
    targets_.push_back({source, reader.component_index, reader.owner, reader.resource});
  }
  std::ranges::sort(targets_, std::greater{}, &FirstCutFadePhaseOneReceipt::source_directory_index);
  if (targets_.size() != expected_sources.size())
    throw std::runtime_error(
        "first-cut fade diagnostic receipt set is incomplete");
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
