#pragma once
#include "off/graphics/intro_runtime.hpp"
#include <limits>
#include <memory>
#include <stdexcept>

namespace off::graphics {
// Project-owned mode child. A write advances its generation even if the
// value is unchanged, so an in-flight selection cannot survive replacement.
class PostloadModeState final {
public:
  [[nodiscard]] std::uint8_t mode() const noexcept {return mode_;}
  [[nodiscard]] std::uint64_t generation() const noexcept {return generation_;}
  void set_mode(std::uint8_t value) {
    if(generation_==std::numeric_limits<std::uint64_t>::max())
      throw std::runtime_error("postload mode generation exhausted");
    mode_=value; ++generation_;
  }
private:
  std::uint8_t mode_{};
  std::uint64_t generation_{1};
};

class PostloadPackedChannel final {
public:
  explicit PostloadPackedChannel(std::uint16_t initial) : state_(initial) {}
  [[nodiscard]] std::uint16_t state() const noexcept {return state_;}
  [[nodiscard]] std::uint64_t generation() const noexcept {return generation_;}
  [[nodiscard]] static constexpr std::uint16_t pack(std::uint32_t argb) noexcept {
    return static_cast<std::uint16_t>(((argb>>16U)&0x8000U) | ((argb>>9U)&0x7c00U) |
                                      ((argb>>6U)&0x03e0U) | ((argb>>3U)&0x001fU));
  }
  [[nodiscard]] static constexpr std::uint32_t expand(std::uint16_t value) noexcept {
    return ((std::uint32_t(value)&0x8000U)<<16U) | ((std::uint32_t(value)&0x7c00U)<<9U) |
           ((std::uint32_t(value)&0x03e0U)<<6U) | ((std::uint32_t(value)&0x001fU)<<3U);
  }
  [[nodiscard]] std::uint32_t exchange(std::uint32_t argb) {
    if(generation_==std::numeric_limits<std::uint64_t>::max())
      throw std::runtime_error("postload channel generation exhausted");
    const auto previous=expand(state_);
    state_=pack(argb); ++generation_;
    return previous;
  }
private:
  std::uint16_t state_{};
  std::uint64_t generation_{1};
};

// The caller supplies the project-selected ARGB value. It is not a claim that
// the original loader uses that value. The same live objects are required for
// select and restore; external writes invalidate an in-flight selection.
class PackedPostloadModeProvider final : public IntroPostloadModeService {
public:
  PackedPostloadModeProvider(std::shared_ptr<PostloadModeState> mode,
                             std::shared_ptr<PostloadPackedChannel> channel,
                             std::uint32_t selected)
      : mode_(std::move(mode)),channel_(std::move(channel)),selected_(selected) {
    if(!mode_ || !channel_) throw std::invalid_argument("postload mode and channel are required");
  }
  [[nodiscard]] bool postload_enabled() const override {return mode_->mode()==0U && !selected_active_;}
  [[nodiscard]] std::uint64_t select() override {
    if(!postload_enabled()) throw std::runtime_error("postload selection is unavailable");
    const auto mode_generation=mode_->generation();
    const auto saved=channel_->exchange(selected_);
    mode_generation_=mode_generation; channel_generation_=channel_->generation();
    saved_=saved; selected_active_=true;
    return saved;
  }
  void restore(std::uint64_t saved) override {
    if(!selected_active_ || saved!=saved_ || mode_->generation()!=mode_generation_ ||
        channel_->generation()!=channel_generation_)
      throw std::runtime_error("postload selection changed before restore");
    selected_active_=false;
    (void)channel_->exchange(static_cast<std::uint32_t>(saved));
  }
private:
  std::shared_ptr<PostloadModeState> mode_;
  std::shared_ptr<PostloadPackedChannel> channel_;
  std::uint32_t selected_{};
  std::uint64_t mode_generation_{},channel_generation_{},saved_{};
  bool selected_active_{};
};
}
