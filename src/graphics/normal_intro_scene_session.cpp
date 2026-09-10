#include "off/graphics/normal_intro_scene_session.hpp"
#include "off/graphics/intro_runtime.hpp"

#include <stdexcept>
#include <utility>

namespace off::graphics {
namespace {
class RuntimeBoundary final : public NormalIntroSceneReaderBoundary {
public:
  explicit RuntimeBoundary(std::unique_ptr<IntroRuntime> runtime)
      : runtime_(std::move(runtime)) {
    if (!runtime_)
      throw std::invalid_argument("intro runtime is required");
  }
  void complete_postconstruction_reader_bracket(std::uint64_t saved) override {
    runtime_->run_postconstruction_reader_bracket(
        saved,
        {.external_loader_service = [](std::uint64_t) {},
         .source_script_work = [](const IntroSourceScriptWork &) {},
         .pre_reader_service = [] {},
         .prepare_deferred_reader = [](const IntroDeferredReaderWork &) {},
         .owner_reader_boundary =
             [this](const IntroDeferredReaderWork &work) {
               const auto &r = runtime_->resources();
               if (work.source_directory_index == r.controller_index())
                 runtime_->apply_supported_movie_control_deferred_reader(work);
               if (work.source_directory_index == r.member_index())
                 runtime_->apply_supported_first_cut_sequence_deferred_reader(
                     work);
               if (work.source_directory_index == r.first_cut_index())
                 runtime_->apply_supported_first_cut_list_deferred_reader(work);
               const auto legal =
                   r.sources().local_source_for_authored_reference(
                       r.member().references[1]);
               if (legal && *legal == work.source_directory_index)
                 runtime_
                     ->apply_supported_first_cut_legal_picture_deferred_reader(
                         work);
               if (r.sources()
                       .directory()
                       .at(work.source_directory_index)
                       .source_type == 0x00200012U)
                 runtime_->apply_supported_sound_owner_deferred_reader(work);
               if (work.source_directory_index == r.window_index())
                 runtime_->apply_supported_window_deferred_reader(work);
               if (work.source_directory_index == 466U)
                 runtime_
                     ->apply_supported_external_cut_commands_deferred_reader(
                         work);
               for (const auto &command : r.first_cut().commands) {
                 const auto target =
                     r.sources().local_source_for_authored_reference(
                         command.target_reference);
                 if (target && *target == work.source_directory_index) {
                   runtime_
                       ->apply_supported_first_cut_fade_picture_deferred_reader(
                           work);
                   break;
                 }
               }
             },
         .component_reader_boundary =
             [this](const IntroDeferredReaderWork &work) {
               if (work.source_directory_index ==
                   runtime_->resources().controller_index())
                 runtime_->apply_supported_movie_control_component_reader(work);
             },
         .end_reader_service = [] {}});
  }
  IntroRuntime *runtime() noexcept override { return runtime_.get(); }

private:
  std::unique_ptr<IntroRuntime> runtime_;
};
} // namespace

NormalIntroSceneSession::NormalIntroSceneSession(
    std::unique_ptr<NormalIntroSceneReaderBoundary> boundary)
    : boundary_(std::move(boundary)) {
  if (!boundary_ || !boundary_->runtime())
    throw std::invalid_argument(
        "normal intro scene reader boundary is required");
}
void NormalIntroSceneSession::complete_postconstruction_reader_bracket(
    std::uint64_t saved) {
  if (stage_ != NormalIntroSceneSessionStage::postconstructed)
    throw std::runtime_error(
        "normal intro scene reader bracket is unavailable");
  try {
    boundary_->complete_postconstruction_reader_bracket(saved);
    stage_ = NormalIntroSceneSessionStage::reader_bracket_complete;
  } catch (...) {
    stage_ = NormalIntroSceneSessionStage::failed;
    throw;
  }
}
IntroRuntime &NormalIntroSceneSession::runtime() const {
  auto *value = boundary_->runtime();
  if (!value)
    throw std::runtime_error("normal intro scene runtime was lost");
  return *value;
}
std::unique_ptr<NormalIntroSceneSession>
make_normal_intro_scene_session(std::unique_ptr<IntroRuntime> runtime) {
  return std::make_unique<NormalIntroSceneSession>(
      std::make_unique<RuntimeBoundary>(std::move(runtime)));
}
} // namespace off::graphics
