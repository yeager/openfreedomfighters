#pragma once

#include <cstdint>
#include <memory>
#include <utility>

namespace off::graphics {

class IntroRuntime;

// Opaque identity for one retained IntroRuntime. Equality is ownership
// identity of the shared control block, not the token's payload or address.
class MovieControlRuntimeIdentity final {
public:
  MovieControlRuntimeIdentity() noexcept = default;

  [[nodiscard]] bool valid() const noexcept { return static_cast<bool>(token_); }

  [[nodiscard]] bool same_owner(
      const MovieControlRuntimeIdentity& other) const noexcept {
    return token_ && other.token_ && !token_.owner_before(other.token_) &&
           !other.token_.owner_before(token_);
  }

private:
  friend class IntroRuntime;
  explicit MovieControlRuntimeIdentity(
      std::shared_ptr<const std::uint8_t> token) noexcept
      : token_(std::move(token)) {}

  std::shared_ptr<const std::uint8_t> token_;
};

}  // namespace off::graphics
