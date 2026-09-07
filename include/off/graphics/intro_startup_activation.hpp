#pragma once

#include "off/graphics/intro_controller_initialization.hpp"
#include "off/graphics/intro_runtime.hpp"

#include <functional>
#include <stdexcept>

namespace off::graphics {

// Checked ordinary-startup orchestration after source-directory construction.
// It wires only already-separated loader and lifecycle boundaries.  It does
// not decode payloads itself, activate a cut, schedule an update, create a
// view, submit a frame, or make the scene current.
enum class IntroStartupActivationStage {
  not_started,
  reader_bracket_complete,
  outer_loader_tail_complete,
  global_lifecycle_complete,
  movie_control_phase_two_complete,
  failed,
};

struct IntroStartupActivationServices {
  std::uint64_t retained_saved_value{};
  IntroPostconstructionReaderServices reader;
  IntroOuterLoaderTailServices outer_loader_tail;
  // The caller owns the concrete position-queue transition and invokes the
  // retained ComponentLifecycle global-lifecycle API with real callbacks.
  // It must return only after root/additional pre- and post-hooks and both
  // reverse component phases succeeded.
  std::function<void()> enter_global_lifecycle;
  MovieControlPhaseTwoServices movie_control_phase_two;
};

// Use this form when the loader/runtime object is owned by a higher-level
// scene host. Each callback must bind the corresponding checked runtime API;
// it is not a replacement parser or an opportunity to collapse stages.
struct IntroStartupActivationBoundaries {
  std::function<void()> reader_bracket;
  std::function<void()> outer_loader_tail;
  std::function<void()> enter_global_lifecycle;
};

class IntroStartupActivation final {
public:
  explicit IntroStartupActivation(IntroRuntime& runtime, MovieControlFirstUpdate& movie_control)
      : runtime_(&runtime), movie_control_(movie_control) {}
  IntroStartupActivation(IntroStartupActivationBoundaries boundaries,
                         MovieControlFirstUpdate& movie_control)
      : boundaries_(std::move(boundaries)), movie_control_(movie_control) {}

  void run(const IntroStartupActivationServices& supplied) {
    if (running_ || stage_ != IntroStartupActivationStage::not_started)
      throw std::runtime_error("intro startup activation is reentrant or already attempted");
    const auto services = supplied;
    if ((!runtime_ && (!boundaries_.reader_bracket || !boundaries_.outer_loader_tail ||
                       !boundaries_.enter_global_lifecycle)) ||
        (runtime_ && !services.enter_global_lifecycle))
      throw std::runtime_error("intro startup activation requires global lifecycle service");
    running_ = true;
    try {
      if (runtime_) {
        runtime_->run_postconstruction_reader_bracket(services.retained_saved_value, services.reader);
        if (runtime_->reader_bracket_stage() != IntroReaderBracketStage::ordinary_reader_boundary_complete)
          throw std::runtime_error("ordinary startup activation cannot continue through restore mode");
      } else {
        boundaries_.reader_bracket();
      }
      stage_ = IntroStartupActivationStage::reader_bracket_complete;

      if (runtime_) {
        runtime_->run_outer_loader_tail_through_saved_services(services.outer_loader_tail);
        if (runtime_->outer_loader_tail_stage() != IntroOuterLoaderTailStage::second_saved_pass_complete)
          throw std::runtime_error("outer loader tail did not reach saved-resource completion");
      } else {
        boundaries_.outer_loader_tail();
      }
      stage_ = IntroStartupActivationStage::outer_loader_tail_complete;

      if (runtime_) services.enter_global_lifecycle();
      else boundaries_.enter_global_lifecycle();
      stage_ = IntroStartupActivationStage::global_lifecycle_complete;

      movie_control_.run_phase_two(services.movie_control_phase_two);
      if (!movie_control_.phase_two_callback_returned())
        throw std::runtime_error("MovieControl phase two returned without completion");
      stage_ = IntroStartupActivationStage::movie_control_phase_two_complete;
      running_ = false;
    } catch (...) {
      running_ = false;
      stage_ = IntroStartupActivationStage::failed;
      throw;
    }
  }

  [[nodiscard]] IntroStartupActivationStage stage() const noexcept { return stage_; }
  [[nodiscard]] bool failed() const noexcept { return stage_ == IntroStartupActivationStage::failed; }
  // Deliberately not `active`: phase two only assigns a deadline and renderer
  // service boundary. An admitted later update is still required.
  [[nodiscard]] bool awaits_first_update() const noexcept {
    return stage_ == IntroStartupActivationStage::movie_control_phase_two_complete;
  }

private:
  IntroRuntime* runtime_{};
  IntroStartupActivationBoundaries boundaries_{};
  MovieControlFirstUpdate& movie_control_;
  IntroStartupActivationStage stage_{IntroStartupActivationStage::not_started};
  bool running_{};
};

} // namespace off::graphics
