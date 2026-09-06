#pragma once
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace off::runtime {
class LiveVariableRegistry;
struct LiveVariableHandle {
  std::uint64_t identity{};
  const LiveVariableRegistry* registry{};
  friend bool operator==(LiveVariableHandle,LiveVariableHandle) = default;
};
enum class LiveVariableType { boolean, floating };
class LiveVariableLease final {
public:
  LiveVariableLease() = default;
  ~LiveVariableLease();
  LiveVariableLease(LiveVariableLease&&) noexcept;
  LiveVariableLease& operator=(LiveVariableLease&&) noexcept;
  LiveVariableLease(const LiveVariableLease&) = delete;
  LiveVariableLease& operator=(const LiveVariableLease&) = delete;
  [[nodiscard]] LiveVariableHandle handle() const noexcept {return handle_;}
  void reset() noexcept;
private:
  friend class LiveVariableRegistry;
  LiveVariableLease(LiveVariableRegistry& registry,LiveVariableHandle handle)
    :registry_(&registry),handle_(handle) {}
  LiveVariableRegistry* registry_{};
  LiveVariableHandle handle_{};
};
// One controlling thread. Registry must outlive its leases; bound storage must
// outlive its lease and retain a stable address. IDs are native identities, not
// original variable tokens. Duplicate names are retained without a winner rule.
class LiveVariableRegistry final {
public:
  LiveVariableRegistry() = default;
  LiveVariableRegistry(const LiveVariableRegistry&) = delete;
  LiveVariableRegistry& operator=(const LiveVariableRegistry&) = delete;
  [[nodiscard]] LiveVariableLease bind(std::string name,bool& storage);
  [[nodiscard]] LiveVariableLease bind(std::string name,float& storage);
  // Registration never reads this not-yet-initialized field.
  [[nodiscard]] LiveVariableLease bind(std::string name,std::optional<float>& storage);
  [[nodiscard]] std::vector<LiveVariableHandle> enumerate(std::string_view name) const;
  [[nodiscard]] bool contains(LiveVariableHandle handle) const noexcept;
  [[nodiscard]] LiveVariableType type(LiveVariableHandle handle) const;
  [[nodiscard]] bool read_bool(LiveVariableHandle handle) const;
  [[nodiscard]] float read_float(LiveVariableHandle handle) const;
  void write_bool(LiveVariableHandle handle,bool value);
  void write_float(LiveVariableHandle handle,float value);
private:
  friend class LiveVariableLease;
  using Storage=std::variant<bool*,float*,std::optional<float>*>;
  struct Entry {std::string name;Storage storage;};
  std::map<std::uint64_t,Entry> entries_;
  std::uint64_t next_{1};
  [[nodiscard]] const Entry& lookup(LiveVariableHandle handle) const;
  [[nodiscard]] LiveVariableLease insert(std::string name,Storage storage);
  void release(LiveVariableHandle handle) noexcept;
};
} // namespace off::runtime
