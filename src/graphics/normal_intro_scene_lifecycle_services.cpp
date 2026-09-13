#include "off/graphics/normal_intro_scene_lifecycle_services.hpp"

#include <stdexcept>
#include <string>
#include <utility>

namespace off::graphics {
namespace {

class BoundLifecycleServices final {
 public:
  BoundLifecycleServices(std::shared_ptr<NormalIntroSceneSession> session,
                         NormalIntroSceneLifecycleAdapterConfig config,
                         bool reader_already_complete = false)
      : session_(std::move(session)), config_(std::move(config)) {
    if (!session_ || !config_.enter_global_lifecycle)
      throw std::runtime_error(
          "normal intro lifecycle adapter requires an owned session and global service");
    if (reader_already_complete) {
      if (session_->stage() != NormalIntroSceneSessionStage::reader_bracket_complete ||
          session_->runtime().reader_bracket_stage() !=
              IntroReaderBracketStage::ordinary_reader_boundary_complete)
        throw std::runtime_error(
            "normal intro lifecycle adapter has no completed reader receipt");
      stage_ = Stage::reader_complete;
    }
  }

  void reader_bracket() {
    try {
      require(Stage::awaiting_reader, "reader bracket");
      if (session_->stage() != NormalIntroSceneSessionStage::postconstructed)
        throw std::runtime_error("normal intro lifecycle adapter has no postconstruction receipt");
      session_->complete_postconstruction_reader_bracket(config_.retained_saved_value);
      if (session_->stage() != NormalIntroSceneSessionStage::reader_bracket_complete ||
          session_->runtime().reader_bracket_stage() !=
              IntroReaderBracketStage::ordinary_reader_boundary_complete)
        throw std::runtime_error("normal intro lifecycle adapter reader receipt was not completed");
      stage_ = Stage::reader_complete;
    } catch (...) {
      stage_ = Stage::failed;
      throw;
    }
  }

  void outer_loader_tail() {
    try {
      require(Stage::reader_complete, "outer loader tail");
      if (session_->stage() != NormalIntroSceneSessionStage::reader_bracket_complete)
        throw std::runtime_error("normal intro lifecycle adapter reader receipt was lost");
      const auto inputs = session_->runtime().outer_loader_source_inputs();
      // The runtime owns these source-backed receipts. A production operation
      // table cannot replace a valid scene's List pairs with raw references,
      // or make an absent section appear present. Copy first so a synchronous
      // callback cannot change this invocation through the caller's table.
      auto tail = config_.outer_loader_tail;
      tail.named_global_payload = inputs.named_global_payload;
      tail.renderer_resource_payload = inputs.renderer_resource_payload;
      tail.resource_associations = inputs.resource_associations;
      session_->complete_outer_loader_tail(tail);
      if (session_->stage() != NormalIntroSceneSessionStage::outer_loader_tail_complete ||
          session_->runtime().outer_loader_tail_stage() !=
              IntroOuterLoaderTailStage::second_saved_pass_complete)
        throw std::runtime_error("normal intro lifecycle adapter outer-tail receipt was not completed");
      stage_ = Stage::tail_complete;
    } catch (...) {
      stage_ = Stage::failed;
      throw;
    }
  }

  void enter_global_lifecycle() {
    try {
      require(Stage::tail_complete, "global lifecycle");
      if (session_->stage() != NormalIntroSceneSessionStage::outer_loader_tail_complete)
        throw std::runtime_error("normal intro lifecycle adapter outer-tail receipt was lost");
      if (!session_->runtime().preflight_global_lifecycle().ready())
        throw std::runtime_error("normal intro lifecycle adapter global lifecycle is not admitted");
      config_.enter_global_lifecycle(*session_);
      if (session_->runtime().components().failed() ||
          !session_->runtime().components().phases_completed())
        throw std::runtime_error("normal intro lifecycle adapter global lifecycle did not complete");
      stage_ = Stage::global_complete;
    } catch (...) {
      stage_ = Stage::failed;
      throw;
    }
  }

 private:
  enum class Stage { awaiting_reader, reader_complete, tail_complete,
                     global_complete, failed };
  void require(Stage expected, const char* boundary) const {
    if (stage_ != expected)
      throw std::runtime_error(std::string{"normal intro lifecycle adapter "} +
                               boundary + " is unavailable");
  }
  std::shared_ptr<NormalIntroSceneSession> session_;
  NormalIntroSceneLifecycleAdapterConfig config_;
  Stage stage_{Stage::awaiting_reader};
};

}  // namespace

NormalIntroSceneHostLifecycleServices
NormalIntroSceneLifecycleServiceAdapters::bind(
    std::shared_ptr<NormalIntroSceneSession> session,
    NormalIntroSceneLifecycleAdapterConfig config) {
  auto bound = std::make_shared<BoundLifecycleServices>(std::move(session),
                                                         std::move(config));
  return {.reader_bracket = [bound] { bound->reader_bracket(); },
          .outer_loader_tail = [bound] { bound->outer_loader_tail(); },
          .enter_global_lifecycle = [bound] { bound->enter_global_lifecycle(); }};
}

NormalIntroSceneHostLifecycleServices
NormalIntroSceneLifecycleServiceAdapters::bind_after_reader_bracket(
    std::shared_ptr<NormalIntroSceneSession> session,
    NormalIntroSceneLifecycleAdapterConfig config) {
  auto bound = std::make_shared<BoundLifecycleServices>(std::move(session),
                                                         std::move(config), true);
  return {.reader_bracket = {},
          .outer_loader_tail = [bound] { bound->outer_loader_tail(); },
          .enter_global_lifecycle = [bound] { bound->enter_global_lifecycle(); }};
}

}  // namespace off::graphics
