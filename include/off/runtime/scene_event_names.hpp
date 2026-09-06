#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace off::runtime {
// Scene-owned declarations only: this registry does not deliver events.
// Fresh scene construction produces counter zero and an absent lazy registry.
class SceneEventNames final {
public:
  static constexpr std::size_t capacity=0x402;
  SceneEventNames();
  SceneEventNames(const SceneEventNames&)=delete;
  SceneEventNames& operator=(const SceneEventNames&)=delete;
  SceneEventNames(SceneEventNames&&)=delete;
  SceneEventNames& operator=(SceneEventNames&&)=delete;
  [[nodiscard]] std::uint16_t declare(std::string_view name,std::uint16_t requested=0);
  [[nodiscard]] std::optional<std::uint16_t> find(std::string_view name) const;
  [[nodiscard]] std::optional<std::string_view> name(std::uint16_t identity) const;
  [[nodiscard]] std::uint32_t counter() const noexcept {return counter_;}
  [[nodiscard]] bool initialized() const noexcept {return initialized_;}
  // Explicit scene event-clear, never an implicit GMS-load or factory reset.
  void clear() noexcept;
private:
  static std::string canonicalize(std::string_view name);
  std::uint16_t insert(std::string canonical,std::uint16_t requested);
  std::map<std::string,std::uint16_t,std::less<>> by_name_;
  // Keep the fixed reverse-name table off the caller's stack. A scene host is
  // commonly a local object, including on Windows' smaller default stack.
  using ReverseNames=std::array<std::optional<std::string>,capacity>;
  std::unique_ptr<ReverseNames> by_identity_;
  std::uint32_t counter_{};
  bool initialized_{};
};
} // namespace off::runtime
