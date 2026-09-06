#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace off::runtime {
enum class InputMapIdentity { root_control };
class InputMapRegistry;
struct InputMapHandle {
  std::uint64_t value{};
  const InputMapRegistry* registry{};
  friend bool operator==(InputMapHandle,InputMapHandle)=default;
};
struct InputMapRecord {
  InputMapIdentity identity;
  std::string display_name;
  std::uint32_t option;
  std::uint64_t references;
};
// Application lifetime retained registrations. No unproved release or action
// processing is implied; destruction clears the native registry only.
class InputMapRegistry final {
public:
  InputMapRegistry()=default;
  InputMapRegistry(const InputMapRegistry&)=delete;
  InputMapRegistry& operator=(const InputMapRegistry&)=delete;
  [[nodiscard]] InputMapHandle acquire(InputMapIdentity identity,std::string display_name,std::uint32_t option);
  [[nodiscard]] const InputMapRecord& at(InputMapHandle handle) const;
  [[nodiscard]] std::size_t size() const noexcept {return records_.size();}
private:
  std::vector<InputMapRecord> records_;
};
}
