#include "off/graphics/normal_intro_scene_session.hpp"
#include "off/graphics/intro_runtime.hpp"
#include "off/data/basic_group_deferred_reader_shape.hpp"

#include <stdexcept>
#include <utility>

namespace off::graphics {
NormalIntroSceneSession::NormalIntroSceneSession(
    std::unique_ptr<IntroRuntime> runtime)
    : runtime_(std::move(runtime)) {
  if (!runtime_)
    throw std::invalid_argument("intro runtime is required");
}
NormalIntroSceneSession::~NormalIntroSceneSession() = default;

void NormalIntroSceneSession::complete_postconstruction_reader_bracket(
    std::uint64_t saved) {
  if (stage_ != NormalIntroSceneSessionStage::postconstructed)
    throw std::runtime_error(
        "normal intro scene reader bracket is unavailable");
  try {
    runtime_->run_postconstruction_reader_bracket(
        saved,
        {.external_loader_service = [this](std::uint64_t value) {
           reader_bracket_observation_.external_loader_values.push_back(value);
         },
         .source_script_work = [this](const IntroSourceScriptWork &work) {
           reader_bracket_observation_.source_scripts.push_back(work);
         },
         .pre_reader_service = [this] { ++reader_bracket_observation_.pre_reader_calls; },
         .prepare_deferred_reader = [this](const IntroDeferredReaderWork &) {
           ++reader_bracket_observation_.prepared_reader_calls;
         },
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
               if (work.source_directory_index == r.camera_index())
                 runtime_->apply_supported_first_cut_camera_deferred_reader(work);
               if (work.source_directory_index >= 43U && work.source_directory_index <= 47U)
                 runtime_->apply_supported_following_visual_owner_deferred_reader(work);
               const auto &source = r.sources().directory().at(work.source_directory_index);
               if (source.source_type == 0x00100001U && source.attachments.empty()) {
                 const auto block = r.sources().deferred_source_block(work.source_directory_index);
                 if (block.size() > sizeof(std::uint32_t) &&
                     data::BasicGroupDeferredReaderShape::matches(
                         block.subspan(sizeof(std::uint32_t))))
                   runtime_->apply_supported_basic_group_owner_deferred_reader(work);
               }
               // A strict reader must not be used as an ordinary MatPos
               // classifier.  The runtime-owned predicate includes its full
               // no-side-effect source, component, KEYS and payload gates.
               if (runtime_->supports_matpos_deferred_reader(work))
                 runtime_->apply_supported_matpos_deferred_reader(work);
               // VertAnim has its own fixed source-backed reader gate. Other
               // records with the same component name remain unclassified.
               if (runtime_->supports_vert_anim_deferred_reader(work))
                 runtime_->apply_supported_vert_anim_deferred_reader(work);
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
               if (work.source_directory_index ==
                   runtime_->resources().first_cut_index() &&
                   runtime_->resources().sources().deferred_source_block(work.source_directory_index).size()==171U)
                 runtime_->apply_supported_first_cut_component_reader(work);
               if (work.source_directory_index == runtime_->resources().member_index())
                 runtime_->apply_supported_first_cut_sequence_component_reader(work);
               if (runtime_->fade_picture_reader_states().contains(work.source_directory_index))
                 runtime_->apply_supported_first_cut_fade_picture_component_reader(work);
               if (runtime_->legal_picture_reader_state() &&
                   runtime_->legal_picture_reader_state()->owner==runtime_->source_handle(work.source_directory_index))
                 runtime_->apply_supported_first_cut_legal_picture_component_reader(work);
             },
         .end_reader_service = [this] { ++reader_bracket_observation_.end_reader_calls; }});
    stage_ = NormalIntroSceneSessionStage::reader_bracket_complete;
  } catch (...) {
    stage_ = NormalIntroSceneSessionStage::failed;
    throw;
  }
}

void NormalIntroSceneSession::prepare_supported_first_cut_player() {
  if (stage_ != NormalIntroSceneSessionStage::reader_bracket_complete ||
      first_cut_player_)
    throw std::runtime_error(
        "normal intro scene first-cut preparation is unavailable");
  try {
    runtime_->prepare_supported_first_cut_player();
    first_cut_player_.emplace(runtime_->first_cut_player_session());
  } catch (...) {
    stage_ = NormalIntroSceneSessionStage::failed;
    throw;
  }
}

void NormalIntroSceneSession::prepare_first_cut_command_runner(
    float derived_end, runtime::IntroLiveTargetRegistry::DispatchServices dispatch) {
  if (stage_ != NormalIntroSceneSessionStage::reader_bracket_complete ||
      !first_cut_player_ || first_cut_command_router_ || first_cut_command_session_ ||
      first_cut_command_runner_) {
    throw std::runtime_error(
        "normal intro scene first-cut command runner is unavailable");
  }
  try {
    auto router = std::make_unique<cutscene::FirstCutRuntimeCommandRouter>(*runtime_);
    auto command_session = std::make_unique<cutscene::FirstCutCommandSession>(
        *runtime_, derived_end, router->command_session_services(std::move(dispatch)),
        router->sender());
    auto runner = std::make_unique<cutscene::FirstCutClockedCommandRunner>(*command_session);
    first_cut_command_router_ = std::move(router);
    first_cut_command_session_ = std::move(command_session);
    first_cut_command_runner_ = std::move(runner);
  } catch (...) {
    first_cut_command_runner_.reset();
    first_cut_command_session_.reset();
    first_cut_command_router_.reset();
    throw;
  }
}

void NormalIntroSceneSession::complete_outer_loader_tail(
    const IntroOuterLoaderTailServices &services) {
  if (stage_ != NormalIntroSceneSessionStage::reader_bracket_complete)
    throw std::runtime_error("normal intro scene loader tail is unavailable");
  try {
    runtime_->run_outer_loader_tail_through_saved_services(services);
    stage_ = NormalIntroSceneSessionStage::outer_loader_tail_complete;
  } catch (...) {
    stage_ = NormalIntroSceneSessionStage::failed;
    throw;
  }
}

IntroOuterLoaderTailReadiness
NormalIntroSceneSession::outer_loader_tail_readiness() const {
  return inspect_intro_outer_loader_tail_readiness(
      runtime_->resources().outer_loader_sources());
}
std::unique_ptr<NormalIntroSceneSession>
make_normal_intro_scene_session(std::unique_ptr<IntroRuntime> runtime) {
  return std::make_unique<NormalIntroSceneSession>(std::move(runtime));
}
} // namespace off::graphics
