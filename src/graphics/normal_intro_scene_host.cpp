#include "off/graphics/normal_intro_scene_host.hpp"

#include <stdexcept>
#include <utility>

namespace off::graphics {
NormalIntroSceneHost::NormalIntroSceneHost(
    std::uint64_t movie_component_handle, std::uint64_t movie_owner_handle,
    std::int32_t movie_delay, IntroStartupActivationBoundaries boundaries,
    NormalIntroSceneHostFirstCutConfig first_cut)
    : movie_control_(movie_component_handle, movie_owner_handle, movie_delay),
      activation_(std::move(boundaries), movie_control_),
      route_(first_cut.first_option, first_cut.optional_renderer_flag,
             first_cut.requested_camera_reference) {}

void NormalIntroSceneHost::activate(const IntroStartupActivationServices& services) {
  if (stage_ != NormalIntroSceneHostStage::constructed)
    throw std::runtime_error("normal intro scene host activation is unavailable");
  try {
    activation_.run(services);
    if (!activation_.awaits_first_update())
      throw std::runtime_error("normal intro scene host did not reach event admission");
    stage_ = NormalIntroSceneHostStage::awaiting_event16;
  } catch (...) { stage_ = NormalIntroSceneHostStage::failed; throw; }
}

void NormalIntroSceneHost::activate_after_reader_bracket(
    const IntroPostReaderActivationServices& services) {
  if (stage_ != NormalIntroSceneHostStage::constructed)
    throw std::runtime_error("normal intro scene post-reader activation is unavailable");
  try {
    activation_.run_after_reader_bracket(services);
    if (!activation_.awaits_first_update())
      throw std::runtime_error("normal intro scene host did not reach event admission");
    stage_ = NormalIntroSceneHostStage::awaiting_event16;
  } catch (...) { stage_ = NormalIntroSceneHostStage::failed; throw; }
}

MovieControlEvent16Result NormalIntroSceneHost::dispatch_event16(
    const MovieControlEvent16Services& services) {
  if (stage_ != NormalIntroSceneHostStage::awaiting_event16 &&
      stage_ != NormalIntroSceneHostStage::event16_not_activated)
    throw std::runtime_error("normal intro scene host event 16 is unavailable");
  try {
    const auto result = movie_control_.dispatch_event16(services);
    event_ = result;
    stage_ = result == MovieControlEvent16Result::activated ||
                     result == MovieControlEvent16Result::already_active
                 ? NormalIntroSceneHostStage::first_cut_routed
                 : NormalIntroSceneHostStage::event16_not_activated;
    return result;
  } catch (...) { stage_ = NormalIntroSceneHostStage::failed; throw; }
}

FirstCutRequestedCameraResult NormalIntroSceneHost::route_first_cut(
    const FirstCutRequestedCameraServices& services) {
  if (stage_ != NormalIntroSceneHostStage::first_cut_routed)
    throw std::runtime_error("normal intro scene host camera route is unavailable");
  try {
    const auto result = route_.run(services);
    route_result_ = result;
    if (result != FirstCutRequestedCameraResult::requested_camera_selected)
      throw std::runtime_error("normal intro scene host did not select the requested camera");
    stage_ = NormalIntroSceneHostStage::view_pending;
    return result;
  } catch (...) { stage_ = NormalIntroSceneHostStage::failed; throw; }
}

FirstCutViewAdmissionResult NormalIntroSceneHost::admit_first_cut_view(
    const FirstCutViewAdmissionGateServices& services) {
  if (stage_ != NormalIntroSceneHostStage::view_pending || !event_ || !route_result_)
    throw std::runtime_error("normal intro scene host view admission is unavailable");
  try {
    const auto result = view_gate_.admit(activation_.stage(), *event_, *route_result_, route_, services);
    view_result_ = result;
    if (result == FirstCutViewAdmissionResult::pending_queued) {
      pending_view_lease_ = view_gate_.take_pending_lease();
      if (!pending_view_lease_)
        throw std::runtime_error("normal intro scene host pending view has no queue-issued lease");
      // Queuing transfers this camera to the renderer's later materialization
      // boundary. Re-entering the gate would append the same camera again.
      stage_ = NormalIntroSceneHostStage::view_queued;
    } else if (result == FirstCutViewAdmissionResult::view_admitted) {
      frame_admission_permit_.emplace(platform::FirstCutFrameAdmissionPermit::mint());
      stage_ = NormalIntroSceneHostStage::view_admitted;
    }
    return result;
  } catch (...) { stage_ = NormalIntroSceneHostStage::failed; throw; }
}

void NormalIntroSceneHost::materialize_queued_first_cut_view(
    RendererPendingCameraQueue& queue, RendererViewState state,
    const RendererPendingMaterializationServices& services) {
  if (stage_ != NormalIntroSceneHostStage::view_queued || !pending_view_lease_)
    throw std::runtime_error("normal intro scene queued view materialization is unavailable");
  try {
    materialized_view_ = queue.materialize(std::move(*pending_view_lease_), state, services);
    pending_view_lease_.reset();
    if (!materialized_view_)
      throw std::runtime_error("normal intro scene queued view did not produce a receipt");
    view_result_ = FirstCutViewAdmissionResult::view_admitted;
    frame_admission_permit_.emplace(platform::FirstCutFrameAdmissionPermit::mint());
    stage_ = NormalIntroSceneHostStage::view_admitted;
  } catch (...) { stage_ = NormalIntroSceneHostStage::failed; throw; }
}

platform::FirstCutPictureFrameResult NormalIntroSceneHost::assemble_first_cut_frame(
    const platform::FirstCutPictureFrameInput& evidence) {
  if (stage_ != NormalIntroSceneHostStage::view_admitted || !frame_admission_permit_ ||
      (view_result_ == FirstCutViewAdmissionResult::pending_queued && !materialized_view_))
    throw std::runtime_error("normal intro scene host frame assembly is unavailable");
  try {
    const auto result = frame_.assemble(std::move(*frame_admission_permit_), evidence);
    frame_admission_permit_.reset();
    materialized_view_.reset();
    if (result == platform::FirstCutPictureFrameResult::assembled)
      stage_ = NormalIntroSceneHostStage::frame_assembled;
    else
      stage_ = NormalIntroSceneHostStage::failed;
    return result;
  } catch (...) { stage_ = NormalIntroSceneHostStage::failed; throw; }
}
}  // namespace off::graphics
