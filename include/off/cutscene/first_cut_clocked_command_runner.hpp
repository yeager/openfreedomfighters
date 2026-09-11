#pragma once

#include "off/cutscene/first_cut_command_session.hpp"
#include "off/cutscene/timeline_position.hpp"

#include <cstdint>
#include <stdexcept>

namespace off::cutscene {

// Binds an already-admitted command session to the recovered scene-clock
// conversion. The host owns cut activation and sampling; this type neither
// creates a clock nor decides when the cut begins or ends.
class FirstCutClockedCommandRunner final {
public:
  explicit FirstCutClockedCommandRunner(FirstCutCommandSession& session)
      : session_(&session) {}
  FirstCutClockedCommandRunner(const FirstCutClockedCommandRunner&) = delete;
  FirstCutClockedCommandRunner& operator=(const FirstCutClockedCommandRunner&) = delete;
  FirstCutClockedCommandRunner(FirstCutClockedCommandRunner&&) = delete;
  FirstCutClockedCommandRunner& operator=(FirstCutClockedCommandRunner&&) = delete;

  void start(std::uint32_t scene_clock_start) {
    if(active_)
      throw std::runtime_error("first-cut clocked command runner is already active");
    scene_clock_start_=scene_clock_start;
    session_->reset_start();
    active_=true;
  }

  void update(std::uint32_t sampled_scene_clock) {
    if(!active_)
      throw std::runtime_error("first-cut clocked command runner is inactive");
    session_->run(timeline_position(sampled_scene_clock,scene_clock_start_));
  }

  void stop() noexcept { active_=false; }
  [[nodiscard]] bool active() const noexcept { return active_; }
  [[nodiscard]] std::uint32_t scene_clock_start() const noexcept {
    return scene_clock_start_;
  }

private:
  FirstCutCommandSession* session_{};
  std::uint32_t scene_clock_start_{};
  bool active_{};
};

} // namespace off::cutscene
