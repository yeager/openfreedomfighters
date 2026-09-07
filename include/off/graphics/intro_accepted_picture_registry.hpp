#pragma once

#include "off/graphics/intro_runtime.hpp"
#include "off/graphics/picture_draw_order.hpp"

#include <map>
#include <stdexcept>

namespace off::graphics {

// Frame-local typed provenance for records accepted by the generic picture
// route. The producer validates the live IntroRuntime owner/resource before
// registration; this class never equates its generic provenance fields with
// either typed handle domain.
struct IntroAcceptedPictureRecord {
  std::uint64_t record_identity{},owner_context_identity{},runtime_resource{};
  IntroRuntimeHandle owner;
  IntroRuntimeResourceHandle resource;
};

class IntroAcceptedPictureRecordRegistry final {
public:
  static constexpr std::size_t capacity=1200;
  void begin_frame(std::uint64_t token) {
    if(!token || active_ || poisoned_) throw std::runtime_error("Intro accepted-picture registry frame is unavailable");
    token_=token;active_=true;
  }
  void register_accepted(const PictureQueuedDrawRecord& record,
                         IntroRuntimeHandle owner,IntroRuntimeResourceHandle resource) {
    if(!active_ || poisoned_ || !record.record_identity || !record.owner_context_identity ||
        !record.runtime_resource || !owner.value || !resource.value || records_.size()>=capacity) {
      poisoned_=true;
      throw std::runtime_error("Intro accepted-picture record registration is invalid");
    }
    const IntroAcceptedPictureRecord value{record.record_identity,record.owner_context_identity,
        record.runtime_resource,owner,resource};
    if(!records_.emplace(record.record_identity,value).second) {
      poisoned_=true;
      throw std::runtime_error("Intro accepted-picture record identity is duplicated");
    }
  }
  [[nodiscard]] const IntroAcceptedPictureRecord* resolve(const PictureQueuedDrawRecord& record) const noexcept {
    if(!active_ || poisoned_) return nullptr;
    const auto found=records_.find(record.record_identity);
    if(found==records_.end()) return nullptr;
    const auto& value=found->second;
    return value.owner_context_identity==record.owner_context_identity &&
        value.runtime_resource==record.runtime_resource?&value:nullptr;
  }
  void end_frame(std::uint64_t token) {
    if(!active_ || poisoned_ || token_!=token) {
      poisoned_=true;
      throw std::runtime_error("Intro accepted-picture registry frame end is invalid");
    }
    records_.clear();token_={};active_=false;
  }
  [[nodiscard]] bool active() const noexcept {return active_;}
  [[nodiscard]] bool poisoned() const noexcept {return poisoned_;}
  [[nodiscard]] std::size_t size() const noexcept {return records_.size();}
private:
  std::map<std::uint64_t,IntroAcceptedPictureRecord> records_;
  std::uint64_t token_{};
  bool active_{},poisoned_{};
};

} // namespace off::graphics
