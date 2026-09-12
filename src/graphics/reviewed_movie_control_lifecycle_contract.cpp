#include "off/graphics/reviewed_movie_control_lifecycle_contract.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <concepts>
#include <fstream>
#include <limits>
#include <map>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace off::graphics {
namespace {
constexpr std::string_view filename{"reviewed-movie-control-lifecycle.json"};
constexpr std::size_t maximum_file_bytes = 8U << 20U;
constexpr std::size_t maximum_depth = 32U;
constexpr std::uint64_t maximum_callback_ordinal = 1'000'000U;

struct Json final {
  using Object = std::map<std::string, Json, std::less<>>;
  using Array = std::vector<Json>;
  std::variant<std::nullptr_t, bool, std::uint64_t, std::string, Object, Array> value;
};

class Parser final {
 public:
  explicit Parser(std::string_view source) : source_(source) {}
  [[nodiscard]] std::optional<Json> parse() {
    auto result = value(0U);
    space();
    if (!result || cursor_ != source_.size()) return std::nullopt;
    return result;
  }
 private:
  void space() { while (cursor_ < source_.size() && std::isspace(static_cast<unsigned char>(source_[cursor_]))) ++cursor_; }
  [[nodiscard]] bool consume(char expected) { space(); if (cursor_ == source_.size() || source_[cursor_] != expected) return false; ++cursor_; return true; }
  [[nodiscard]] std::optional<std::string> string() {
    space();
    if (cursor_ == source_.size() || source_[cursor_++] != '"') return std::nullopt;
    std::string result;
    while (cursor_ < source_.size()) {
      const char character = source_[cursor_++];
      if (character == '"') return result;
      // The private producer writes plain ASCII category strings. Rejecting
      // escapes keeps the admitted schema narrowly source-free.
      if (character == '\\' || static_cast<unsigned char>(character) < 0x20U || result.size() >= 128U) return std::nullopt;
      result.push_back(character);
    }
    return std::nullopt;
  }
  [[nodiscard]] std::optional<Json> number() {
    space(); const auto begin = cursor_;
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
    Json::Object result; space();
    if (cursor_ < source_.size() && source_[cursor_] == '}') { ++cursor_; return Json{std::move(result)}; }
    for (;;) {
      const auto key = string(); if (!key || !consume(':')) return std::nullopt;
      const auto entry = value(depth + 1U); if (!entry || !result.emplace(*key, *entry).second) return std::nullopt;
      space(); if (cursor_ < source_.size() && source_[cursor_] == '}') { ++cursor_; return Json{std::move(result)}; }
      if (!consume(',')) return std::nullopt;
    }
  }
  [[nodiscard]] std::optional<Json> array(std::size_t depth) {
    if (!consume('[')) return std::nullopt;
    Json::Array result; space();
    if (cursor_ < source_.size() && source_[cursor_] == ']') { ++cursor_; return Json{std::move(result)}; }
    for (;;) {
      const auto entry = value(depth + 1U); if (!entry || result.size() >= 16U) return std::nullopt;
      result.push_back(*entry); space();
      if (cursor_ < source_.size() && source_[cursor_] == ']') { ++cursor_; return Json{std::move(result)}; }
      if (!consume(',')) return std::nullopt;
    }
  }
  [[nodiscard]] std::optional<Json> value(std::size_t depth) {
    if (depth > maximum_depth) return std::nullopt;
    space();
    if (cursor_ == source_.size()) return std::nullopt;
    if (source_[cursor_] == '{') return object(depth);
    if (source_[cursor_] == '[') return array(depth);
    if (source_[cursor_] == '"') { const auto result = string(); return result ? std::optional<Json>{Json{*result}} : std::nullopt; }
    if (source_.substr(cursor_).starts_with("true")) { cursor_ += 4U; return Json{true}; }
    if (source_.substr(cursor_).starts_with("false")) { cursor_ += 5U; return Json{false}; }
    return number();
  }
  std::string_view source_; std::size_t cursor_{};
};

[[nodiscard]] bool regular_non_link(const std::filesystem::path& path) noexcept {
  std::error_code error; const auto status = std::filesystem::symlink_status(path, error);
  return !error && std::filesystem::is_regular_file(status) && !std::filesystem::is_symlink(status);
}
[[nodiscard]] bool directory_non_link(const std::filesystem::path& path) noexcept {
  std::error_code error; const auto status = std::filesystem::symlink_status(path, error);
  return !error && std::filesystem::is_directory(status) && !std::filesystem::is_symlink(status);
}
[[nodiscard]] const Json::Object* object(const Json& value) { return std::get_if<Json::Object>(&value.value); }
[[nodiscard]] const Json::Array* array(const Json& value) { return std::get_if<Json::Array>(&value.value); }
[[nodiscard]] const std::string* string(const Json& value) { return std::get_if<std::string>(&value.value); }
[[nodiscard]] const std::uint64_t* integer(const Json& value) { return std::get_if<std::uint64_t>(&value.value); }
[[nodiscard]] const bool* boolean(const Json& value) { return std::get_if<bool>(&value.value); }
[[nodiscard]] const Json* member(const Json::Object& value, std::string_view key) { const auto found = value.find(key); return found == value.end() ? nullptr : &found->second; }
template <std::ranges::input_range Keys>
[[nodiscard]] bool exact_keys(const Json::Object& value, const Keys& keys) {
  if (value.size() != keys.size()) return false;
  return std::ranges::all_of(keys, [&value](const auto& key) { return value.contains(key); });
}
[[nodiscard]] bool exact_keys(const Json::Object& value,
                              std::initializer_list<std::string_view> keys) {
  if (value.size() != keys.size()) return false;
  return std::ranges::all_of(keys, [&value](std::string_view key) { return value.contains(key); });
}
[[nodiscard]] bool equals(const Json::Object& value, std::string_view key, std::string_view expected) { const auto item = member(value, key); const auto actual = item ? string(*item) : nullptr; return actual && *actual == expected; }
template <typename T> requires std::same_as<std::remove_cvref_t<T>, bool>
[[nodiscard]] bool equals(const Json::Object& value, std::string_view key, T expected) { const auto item = member(value, key); const auto actual = item ? boolean(*item) : nullptr; return actual && *actual == expected; }
template <std::integral T> requires (!std::same_as<std::remove_cvref_t<T>, bool>)
[[nodiscard]] bool equals(const Json::Object& value, std::string_view key, T expected) { const auto item = member(value, key); const auto actual = item ? integer(*item) : nullptr; return actual && *actual == static_cast<std::uint64_t>(expected); }

[[nodiscard]] bool phase_record(const Json::Object& record, bool success, std::uint64_t callback) {
  constexpr std::array keys{"dispatch_order","callback_ordinal","component_is_constructed","owner_is_constructed_owner","global_lifecycle_entered","global_lifecycle_completed","global_lifecycle_outcome","phase_one_completed","outcome","component_status_before","component_status_after","owner_status_before","owner_status_after","event_member_before","event_member_after","external_service","ordinary_member_before","ordinary_member_after"};
  return exact_keys(record, keys) && equals(record, "dispatch_order", 4U) && equals(record, "callback_ordinal", callback) &&
      equals(record, "component_is_constructed", true) && equals(record, "owner_is_constructed_owner", true) && equals(record, "global_lifecycle_entered", true) &&
      equals(record, "global_lifecycle_completed", success) && equals(record, "global_lifecycle_outcome", success ? "success" : "failure") && equals(record, "phase_one_completed", success) && equals(record, "outcome", success ? "success" : "failure") &&
      equals(record, "component_status_before", 0U) && equals(record, "component_status_after", success ? 4U : 0U) && equals(record, "owner_status_before", 0U) && equals(record, "owner_status_after", success ? 4U : 0U) &&
      equals(record, "event_member_before", false) && equals(record, "event_member_after", success) && equals(record, "external_service", "entered") && equals(record, "ordinary_member_before", false) && equals(record, "ordinary_member_after", success);
}
[[nodiscard]] bool dispatch_record(const Json::Object& record, bool success, std::uint64_t callback) {
  constexpr std::array keys{"observation_order","phase","callback_ordinal","movie_component_is_constructed","movie_owner_is_constructed_owner","sequence_component_is_constructed","sequence_owner_is_constructed_owner","movie_phase_one_completed","movie_phase_two_completed","component_status_before","component_status_after","owner_status_before","owner_status_after","event16_gate","handoff_sender_is_movie_owner","handoff_target_is_sequence_owner","handoff","delivery_mode","player_activation","outcome","external_service"};
  return exact_keys(record, keys) && equals(record, "observation_order", 0U) && equals(record, "phase", success ? "player_activation" : "failure") && equals(record, "callback_ordinal", callback) &&
      equals(record,"movie_component_is_constructed",true) && equals(record,"movie_owner_is_constructed_owner",true) && equals(record,"sequence_component_is_constructed",true) && equals(record,"sequence_owner_is_constructed_owner",true) && equals(record,"movie_phase_one_completed",true) && equals(record,"movie_phase_two_completed",true) && equals(record,"component_status_before",4U) && equals(record,"component_status_after",4U) && equals(record,"owner_status_before",0U) && equals(record,"owner_status_after",0U) && equals(record,"event16_gate",success ? "admitted" : "failed") && equals(record,"handoff_sender_is_movie_owner",true) && equals(record,"handoff_target_is_sequence_owner",true) && equals(record,"handoff",success ? "delivered" : "failed") && equals(record,"delivery_mode",success ? "synchronous" : "not_observed") && equals(record,"player_activation",success ? "started" : "not_started") && equals(record,"outcome",success ? "success" : "failure") && equals(record,"external_service","entered");
}
[[nodiscard]] bool player_record(const Json::Object& record, std::string_view phase, std::uint64_t order, std::uint64_t callback, bool failure) {
  constexpr std::array keys{"observation_order","phase","callback_ordinal","sequence_component_constructed","sequence_owner_constructed","reader_graph_receipt","component_status_before","component_status_after","owner_status_before","owner_status_after","player_state_before","player_state_after","receiver_state","member_sweep","reference_sweep","activation","completion","outcome","external_service"};
  const std::array before{"cold", "phase_one_ready", "phase_two_ready", "active"};
  const std::array after{"phase_one_ready", "phase_two_ready", "active", "completed"};
  const auto index = static_cast<std::size_t>(order);
  const bool normal = !failure && index < before.size();
  return exact_keys(record, keys) && equals(record,"observation_order",order) && equals(record,"phase",phase) && equals(record,"callback_ordinal",callback) && equals(record,"sequence_component_constructed",true) && equals(record,"sequence_owner_constructed",true) && equals(record,"reader_graph_receipt","complete") && equals(record,"component_status_before",4U) && equals(record,"component_status_after",4U) && equals(record,"owner_status_before",4U) && equals(record,"owner_status_after",4U) &&
    (normal ? equals(record,"player_state_before",before[index]) && equals(record,"player_state_after",after[index]) : equals(record,"player_state_before","phase_two_ready") && equals(record,"player_state_after","failed")) && equals(record,"receiver_state",normal && order == 0U ? "open" : "sealed") && equals(record,"member_sweep",normal && order == 1U ? "derived" : "not_entered") && equals(record,"reference_sweep",normal && order == 1U ? "camera_and_sequence" : "not_entered") && equals(record,"activation",normal && order == 2U ? "started" : failure ? "failed" : "not_attempted") && equals(record,"completion",normal && order == 2U ? "pending" : normal && order == 3U ? "completed" : failure ? "failed" : "not_observed") && equals(record,"outcome",failure ? "failure" : "success") && equals(record,"external_service","entered");
}

[[nodiscard]] std::optional<std::uint64_t> validated_callback(const Json& root) {
  const auto top = object(root);
  if (!top || !exact_keys(*top, {"format","phase_one","dispatch","player_route","player_failure"}) || !equals(*top,"format","off.movie-control-cutscene-lifecycle-contract-bundle/v1")) return std::nullopt;
  const auto phase = member(*top,"phase_one"); const auto dispatch = member(*top,"dispatch"); const auto route = member(*top,"player_route"); const auto failure = member(*top,"player_failure");
  const auto phase_object = phase ? object(*phase) : nullptr; const auto dispatch_object = dispatch ? object(*dispatch) : nullptr; const auto route_array = route ? array(*route) : nullptr; const auto failure_object = failure ? object(*failure) : nullptr;
  if (!phase_object || !dispatch_object || !route_array || !failure_object || !exact_keys(*phase_object,{"candidate","failure"}) || !exact_keys(*dispatch_object,{"candidate","failure"}) || route_array->size() != 4U) return std::nullopt;
  const auto phase_candidate_value = member(*phase_object,"candidate");
  const auto phase_failure_value = member(*phase_object,"failure");
  const auto dispatch_candidate_value = member(*dispatch_object,"candidate");
  const auto dispatch_failure_value = member(*dispatch_object,"failure");
  if (!phase_candidate_value || !phase_failure_value || !dispatch_candidate_value ||
      !dispatch_failure_value) return std::nullopt;
  const auto phase_candidate = object(*phase_candidate_value);
  const auto phase_failure = object(*phase_failure_value);
  const auto dispatch_candidate = object(*dispatch_candidate_value);
  const auto dispatch_failure = object(*dispatch_failure_value);
  if (!phase_candidate || !phase_failure || !dispatch_candidate || !dispatch_failure) return std::nullopt;
  const auto callback_value = member(*phase_candidate,"callback_ordinal"); const auto callback = callback_value ? integer(*callback_value) : nullptr;
  if (!callback || *callback > maximum_callback_ordinal || !phase_record(*phase_candidate,true,*callback) || !phase_record(*phase_failure,false,*callback) || !dispatch_record(*dispatch_candidate,true,*callback) || !dispatch_record(*dispatch_failure,false,*callback)) return std::nullopt;
  constexpr std::array phases{"phase_one","phase_two","activation","completion"};
  for (std::size_t index{}; index < phases.size(); ++index) { const auto item = object((*route_array)[index]); if (!item || !player_record(*item, phases[index], index, *callback, false)) return std::nullopt; }
  if (!player_record(*failure_object,"failure",0U,*callback,true)) return std::nullopt;
  return *callback;
}
}  // namespace

std::optional<ReviewedMovieControlLifecycleContract>
ReviewedMovieControlLifecycleContract::load_local(const std::filesystem::path& local_directory) {
  if (local_directory.empty() || !directory_non_link(local_directory)) return std::nullopt;
  const auto path = local_directory / std::string{filename};
  if (path.parent_path() != local_directory || !regular_non_link(path)) return std::nullopt;
  std::error_code error; const auto size = std::filesystem::file_size(path, error);
  if (error || size == 0U || size > maximum_file_bytes) return std::nullopt;
  std::ifstream input(path, std::ios::binary); std::string bytes(static_cast<std::size_t>(size), '\0');
  input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  if (!input || input.gcount() != static_cast<std::streamsize>(bytes.size()) || input.peek() != std::char_traits<char>::eof()) return std::nullopt;
  const auto parsed = Parser{bytes}.parse(); if (!parsed) return std::nullopt;
  const auto callback = validated_callback(*parsed); if (!callback) return std::nullopt;
  // Do not retain source-derived observer identity. The receipt only records
  // that this exact structural schema passed local admission.
  return ReviewedMovieControlLifecycleContract{1U};
}
}  // namespace off::graphics
