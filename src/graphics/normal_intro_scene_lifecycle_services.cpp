#include "off/graphics/normal_intro_scene_lifecycle_services.hpp"

#include <stdexcept>
#include <string>
#include <utility>

namespace off::graphics {
namespace {

[[nodiscard]] bool same_payload(
    const std::optional<IntroNamedGlobalPayload>& expected,
    const std::optional<IntroNamedGlobalPayload>& supplied) {
  return expected.has_value() == supplied.has_value() &&
      (!expected || (expected->bytes.data() == supplied->bytes.data() &&
                     expected->bytes.size() == supplied->bytes.size()));
}

[[nodiscard]] bool same_payload(
    const std::optional<IntroRendererResourcePayload>& expected,
    const std::optional<IntroRendererResourcePayload>& supplied) {
  return expected.has_value() == supplied.has_value() &&
      (!expected || (expected->bytes.data() == supplied->bytes.data() &&
                     expected->bytes.size() == supplied->bytes.size()));
}

[[nodiscard]] bool same_associations(
    const std::vector<IntroResourceAssociationRecord>& expected,
    const std::vector<IntroResourceAssociationRecord>& supplied) {
  if (expected.size() != supplied.size()) return false;
  for (std::size_t index = 0; index < expected.size(); ++index)
    if (expected[index].first_reference != supplied[index].first_reference ||
        expected[index].second_reference != supplied[index].second_reference)
      return false;
  return true;
}

class BoundLifecycleServices final {
 public:
  BoundLifecycleServices(std::shared_ptr<NormalIntroSceneSession> session,
                         NormalIntroSceneLifecycleAdapterConfig config)
      : session_(std::move(session)), config_(std::move(config)) {
    if (!session_ || !config_.enter_global_lifecycle)
      throw std::runtime_error(
          "normal intro lifecycle adapter requires an owned session and global service");
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
      if (!same_payload(inputs.named_global_payload, config_.outer_loader_tail.named_global_payload) ||
          !same_payload(inputs.renderer_resource_payload,
                        config_.outer_loader_tail.renderer_resource_payload) ||
          !same_associations(inputs.resource_associations,
                             config_.outer_loader_tail.resource_associations))
        throw std::runtime_error("normal intro lifecycle adapter outer tail lacks source receipts");
      session_->complete_outer_loader_tail(config_.outer_loader_tail);
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

}  // namespace off::graphics
