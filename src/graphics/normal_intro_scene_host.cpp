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
    if (result == FirstCutViewAdmissionResult::view_admitted)
      stage_ = NormalIntroSceneHostStage::view_admitted;
    return result;
  } catch (...) { stage_ = NormalIntroSceneHostStage::failed; throw; }
}

platform::FirstCutPictureFrameResult NormalIntroSceneHost::assemble_first_cut_frame(
    const platform::FirstCutPictureFrameInput& evidence) {
  if (stage_ != NormalIntroSceneHostStage::view_admitted ||
      evidence.view_admission != FirstCutViewAdmissionResult::view_admitted)
    throw std::runtime_error("normal intro scene host frame assembly is unavailable");
  try {
    const auto result = frame_.assemble(evidence);
    if (result == platform::FirstCutPictureFrameResult::assembled)
      stage_ = NormalIntroSceneHostStage::frame_assembled;
    return result;
  } catch (...) { stage_ = NormalIntroSceneHostStage::failed; throw; }
}
}  // namespace off::graphics
