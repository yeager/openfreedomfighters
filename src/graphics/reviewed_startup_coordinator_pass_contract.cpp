#include "off/graphics/reviewed_startup_coordinator_pass_contract.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <concepts>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>

namespace off::graphics {
namespace {
constexpr std::string_view filename{"reviewed-startup-coordinator-pass.json"};
constexpr std::size_t maximum_file_bytes = 64U << 10U;
constexpr std::size_t maximum_depth = 8U;

struct Json final {
  using Object = std::map<std::string, Json, std::less<>>;
  std::variant<bool, std::uint64_t, std::string, Object> value;
};

class Parser final {
 public:
  explicit Parser(std::string_view source) : source_(source) {}

  [[nodiscard]] std::optional<Json> parse() {
    auto result = value(0U);
    space();
    return result && cursor_ == source_.size() ? result : std::nullopt;
  }

 private:
  void space() {
    while (cursor_ < source_.size() && std::isspace(static_cast<unsigned char>(source_[cursor_]))) {
      ++cursor_;
    }
  }
  [[nodiscard]] bool consume(char expected) {
    space();
    if (cursor_ == source_.size() || source_[cursor_] != expected) return false;
    ++cursor_;
    return true;
  }
  [[nodiscard]] std::optional<std::string> string() {
    space();
    if (cursor_ == source_.size() || source_[cursor_++] != '"') return std::nullopt;
    std::string result;
    while (cursor_ < source_.size()) {
      const char character = source_[cursor_++];
      if (character == '"') return result;
      const auto byte = static_cast<unsigned char>(character);
      if (character == '\\' || byte < 0x20U || byte > 0x7EU || result.size() >= 64U) return std::nullopt;
      result.push_back(character);
    }
    return std::nullopt;
  }
  [[nodiscard]] std::optional<Json> number() {
    space();
    const auto begin = cursor_;
    while (cursor_ < source_.size() && source_[cursor_] >= '0' && source_[cursor_] <= '9') ++cursor_;
    if (begin == cursor_ || (cursor_ - begin > 1U && source_[begin] == '0')) return std::nullopt;
    std::uint64_t result{};
    for (std::size_t index = begin; index < cursor_; ++index) {
      const auto digit = static_cast<std::uint64_t>(source_[index] - '0');
      if (result > (std::numeric_limits<std::uint64_t>::max() - digit) / 10U) return std::nullopt;
      result = result * 10U + digit;
    }
    return Json{result};
  }
  [[nodiscard]] std::optional<Json> object(std::size_t depth) {
    if (!consume('{')) return std::nullopt;
    Json::Object result;
    space();
    if (cursor_ < source_.size() && source_[cursor_] == '}') {
      ++cursor_;
      return Json{std::move(result)};
    }
    for (;;) {
      const auto key = string();
      if (!key || !consume(':')) return std::nullopt;
      const auto entry = value(depth + 1U);
      if (!entry || !result.emplace(*key, *entry).second) return std::nullopt;
      space();
      if (cursor_ < source_.size() && source_[cursor_] == '}') {
        ++cursor_;
        return Json{std::move(result)};
      }
      if (!consume(',')) return std::nullopt;
    }
  }
  [[nodiscard]] std::optional<Json> value(std::size_t depth) {
    if (depth > maximum_depth) return std::nullopt;
    space();
    if (cursor_ == source_.size()) return std::nullopt;
    if (source_[cursor_] == '{') return object(depth);
    if (source_[cursor_] == '"') {
      const auto result = string();
      return result ? std::optional<Json>{Json{*result}} : std::nullopt;
    }
    if (source_.substr(cursor_).starts_with("true")) {
      cursor_ += 4U;
      return Json{true};
    }
    if (source_.substr(cursor_).starts_with("false")) {
      cursor_ += 5U;
      return Json{false};
    }
    return number();
  }

  std::string_view source_;
  std::size_t cursor_{};
};

[[nodiscard]] bool regular_non_link(const std::filesystem::path& path) noexcept {
  std::error_code error;
  const auto status = std::filesystem::symlink_status(path, error);
  return !error && std::filesystem::is_regular_file(status) && !std::filesystem::is_symlink(status);
}
[[nodiscard]] bool directory_non_link(const std::filesystem::path& path) noexcept {
  std::error_code error;
  const auto status = std::filesystem::symlink_status(path, error);
  return !error && std::filesystem::is_directory(status) && !std::filesystem::is_symlink(status);
}
[[nodiscard]] const Json::Object* object(const Json& value) {
  return std::get_if<Json::Object>(&value.value);
}
[[nodiscard]] const Json* member(const Json::Object& value, std::string_view key) {
  const auto found = value.find(key);
  return found == value.end() ? nullptr : &found->second;
}
template <std::ranges::input_range Keys>
[[nodiscard]] bool exact_keys(const Json::Object& value, const Keys& keys) {
  return value.size() == keys.size() &&
      std::ranges::all_of(keys, [&value](const auto& key) { return value.contains(key); });
}
[[nodiscard]] bool equals(const Json::Object& value, std::string_view key,
                          std::string_view expected) {
  const auto item = member(value, key);
  const auto actual = item ? std::get_if<std::string>(&item->value) : nullptr;
  return actual && *actual == expected;
}
template <typename T>
  requires std::same_as<std::remove_cvref_t<T>, bool>
[[nodiscard]] bool equals(const Json::Object& value, std::string_view key, T expected) {
  const auto item = member(value, key);
  const auto actual = item ? std::get_if<bool>(&item->value) : nullptr;
  return actual && *actual == expected;
}
template <std::integral T>
  requires(!std::same_as<std::remove_cvref_t<T>, bool>)
[[nodiscard]] bool equals(const Json::Object& value, std::string_view key, T expected) {
  const auto item = member(value, key);
  const auto actual = item ? std::get_if<std::uint64_t>(&item->value) : nullptr;
  return actual && *actual == static_cast<std::uint64_t>(expected);
}

[[nodiscard]] bool record(const Json::Object& value, bool success) {
  constexpr std::array keys{"pass_order", "coordinator_constructed", "manager_constructed",
                            "pass_entered", "work_list_ready", "selected_work",
                            "work_admission", "pass_completed", "outcome", "external_service"};
  return exact_keys(value, keys) && equals(value, "pass_order", 0U) &&
      equals(value, "coordinator_constructed", true) && equals(value, "manager_constructed", true) &&
      equals(value, "pass_entered", true) && equals(value, "work_list_ready", true) &&
      equals(value, "selected_work", "startup_picture") &&
      equals(value, "work_admission", success ? "admitted" : "rejected") &&
      equals(value, "pass_completed", success) &&
      equals(value, "outcome", success ? "success" : "failure") &&
      equals(value, "external_service", "entered");
}

[[nodiscard]] bool validated(const Json& root) {
  const auto top = object(root);
  if (!top || !exact_keys(*top, std::array{"format", "candidate", "repeat", "failure"}) ||
      !equals(*top, "format", "off.startup-coordinator-pass-contract/v1")) {
    return false;
  }
  const auto candidate_value = member(*top, "candidate");
  const auto repeat_value = member(*top, "repeat");
  const auto failure_value = member(*top, "failure");
  const auto candidate = candidate_value ? object(*candidate_value) : nullptr;
  const auto repeat = repeat_value ? object(*repeat_value) : nullptr;
  const auto failure = failure_value ? object(*failure_value) : nullptr;
  return candidate && repeat && failure && record(*candidate, true) && record(*repeat, true) &&
      record(*failure, false);
}
}  // namespace

std::optional<ReviewedStartupCoordinatorPassContract>
ReviewedStartupCoordinatorPassContract::load_local(const std::filesystem::path& local_directory) {
  if (local_directory.empty() || !directory_non_link(local_directory)) return std::nullopt;
  const auto path = local_directory / std::string{filename};
  if (path.parent_path() != local_directory || !regular_non_link(path)) return std::nullopt;
  std::error_code error;
  const auto size = std::filesystem::file_size(path, error);
  if (error || size == 0U || size > maximum_file_bytes) return std::nullopt;
  std::ifstream input(path, std::ios::binary);
  std::string bytes(static_cast<std::size_t>(size), '\0');
  input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  if (!input || input.gcount() != static_cast<std::streamsize>(bytes.size()) ||
      input.peek() != std::char_traits<char>::eof()) return std::nullopt;
  const auto parsed = Parser{bytes}.parse();
  if (!parsed || !validated(*parsed)) return std::nullopt;
  return ReviewedStartupCoordinatorPassContract{true};
}
}  // namespace off::graphics
