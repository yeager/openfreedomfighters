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
constexpr std::uint64_t maximum_callback_ordinal = 65'535U;
constexpr std::uint64_t maximum_observation_order = 4'096U;
constexpr std::uint64_t maximum_status_mask = 0xffff'ffffU;

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

[[nodiscard]] bool bounded_integer(const Json::Object& value, std::string_view key,
                                   std::uint64_t maximum) {
  const auto item = member(value, key); const auto actual = item ? integer(*item) : nullptr;
  return actual && *actual <= maximum;
}
[[nodiscard]] bool one_of(const Json::Object& value, std::string_view key,
                          std::initializer_list<std::string_view> choices) {
  const auto item = member(value, key); const auto actual = item ? string(*item) : nullptr;
  return actual && std::ranges::find(choices, *actual) != choices.end();
}
[[nodiscard]] bool same(const Json::Object& first, std::string_view first_key,
                        const Json::Object& second, std::string_view second_key) {
  const auto left = member(first, first_key); const auto right = member(second, second_key);
  if (!left || !right) return false;
  if (const auto integer_left = integer(*left)) { const auto integer_right = integer(*right); return integer_right && *integer_left == *integer_right; }
  if (const auto string_left = string(*left)) { const auto string_right = string(*right); return string_right && *string_left == *string_right; }
  if (const auto boolean_left = boolean(*left)) { const auto boolean_right = boolean(*right); return boolean_right && *boolean_left == *boolean_right; }
  return false;
}

[[nodiscard]] bool phase_record(const Json::Object& record, bool success) {
  constexpr std::array keys{"dispatch_order","callback_ordinal","component_is_constructed","owner_is_constructed_owner","global_lifecycle_entered","global_lifecycle_completed","global_lifecycle_outcome","phase_one_completed","outcome","component_status_before","component_status_after","owner_status_before","owner_status_after","event_member_before","event_member_after","external_service","ordinary_member_before","ordinary_member_after"};
  return exact_keys(record, keys) && bounded_integer(record, "dispatch_order", maximum_observation_order) && bounded_integer(record, "callback_ordinal", maximum_callback_ordinal) &&
      equals(record, "component_is_constructed", true) && equals(record, "owner_is_constructed_owner", true) && equals(record, "global_lifecycle_entered", true) &&
      equals(record, "global_lifecycle_completed", success) && equals(record, "global_lifecycle_outcome", success ? "success" : "failure") && equals(record, "phase_one_completed", success) && equals(record, "outcome", success ? "success" : "failure") &&
      bounded_integer(record, "component_status_before", maximum_status_mask) && bounded_integer(record, "component_status_after", maximum_status_mask) && bounded_integer(record, "owner_status_before", maximum_status_mask) && bounded_integer(record, "owner_status_after", maximum_status_mask) &&
      member(record, "event_member_before") && boolean(*member(record, "event_member_before")) && member(record, "event_member_after") && boolean(*member(record, "event_member_after")) && one_of(record, "external_service", {"not_entered", "entered"}) && member(record, "ordinary_member_before") && boolean(*member(record, "ordinary_member_before")) && member(record, "ordinary_member_after") && boolean(*member(record, "ordinary_member_after"));
}
[[nodiscard]] bool dispatch_record(const Json::Object& record, bool success) {
  constexpr std::array keys{"observation_order","phase","callback_ordinal","movie_component_is_constructed","movie_owner_is_constructed_owner","sequence_component_is_constructed","sequence_owner_is_constructed_owner","movie_phase_one_completed","movie_phase_two_completed","component_status_before","component_status_after","owner_status_before","owner_status_after","event16_gate","handoff_sender_is_movie_owner","handoff_target_is_sequence_owner","handoff","delivery_mode","player_activation","outcome","external_service"};
  return exact_keys(record, keys) && bounded_integer(record, "observation_order", maximum_observation_order) && one_of(record, "phase", {"event16", "handoff", "player_activation", "completion", "failure"}) && bounded_integer(record, "callback_ordinal", maximum_callback_ordinal) &&
      equals(record,"movie_component_is_constructed",true) && equals(record,"movie_owner_is_constructed_owner",true) && equals(record,"sequence_component_is_constructed",true) && equals(record,"sequence_owner_is_constructed_owner",true) && equals(record,"movie_phase_one_completed",true) &&
      member(record, "movie_phase_two_completed") && boolean(*member(record, "movie_phase_two_completed")) && bounded_integer(record,"component_status_before",maximum_status_mask) && bounded_integer(record,"component_status_after",maximum_status_mask) && bounded_integer(record,"owner_status_before",maximum_status_mask) && bounded_integer(record,"owner_status_after",maximum_status_mask) && equals(record,"event16_gate","admitted") && equals(record,"handoff_sender_is_movie_owner",true) && equals(record,"handoff_target_is_sequence_owner",true) &&
      (success ? equals(record,"handoff", "delivered") && equals(record,"delivery_mode", "synchronous") && equals(record,"player_activation", "started") && equals(record,"outcome", "success") :
       equals(record,"outcome", "failure") &&
           ((equals(record,"handoff", "failed") && one_of(record,"delivery_mode", {"not_observed"}) && one_of(record,"player_activation", {"not_entered", "not_started"})) ||
            (equals(record,"handoff", "delivered") && equals(record,"delivery_mode", "synchronous") && equals(record,"player_activation", "failed")))) &&
      one_of(record,"external_service", {"not_entered", "entered"});
}
[[nodiscard]] bool player_record(const Json::Object& record, std::string_view phase, bool failure) {
  constexpr std::array keys{"observation_order","phase","callback_ordinal","sequence_component_constructed","sequence_owner_constructed","reader_graph_receipt","component_status_before","component_status_after","owner_status_before","owner_status_after","player_state_before","player_state_after","receiver_state","member_sweep","reference_sweep","activation","completion","outcome","external_service"};
  return exact_keys(record, keys) && bounded_integer(record,"observation_order",maximum_observation_order) && equals(record,"phase",phase) && bounded_integer(record,"callback_ordinal",maximum_callback_ordinal) && equals(record,"sequence_component_constructed",true) && equals(record,"sequence_owner_constructed",true) && equals(record,"reader_graph_receipt","complete") && bounded_integer(record,"component_status_before",maximum_status_mask) && bounded_integer(record,"component_status_after",maximum_status_mask) && bounded_integer(record,"owner_status_before",maximum_status_mask) && bounded_integer(record,"owner_status_after",maximum_status_mask) &&
    one_of(record,"player_state_before", {"cold", "phase_one_ready", "phase_two_ready", "active", "completed", "failed"}) && one_of(record,"player_state_after", {"cold", "phase_one_ready", "phase_two_ready", "active", "completed", "failed"}) && one_of(record,"receiver_state", {"closed", "open", "sealed"}) && one_of(record,"member_sweep", {"not_entered", "queried", "derived", "failed"}) && one_of(record,"reference_sweep", {"not_entered", "camera_only", "camera_and_sequence", "failed"}) && one_of(record,"activation", {"not_attempted", "attempted", "started", "failed"}) && one_of(record,"completion", {"not_observed", "pending", "completed", "failed"}) && equals(record,"outcome",failure ? "failure" : "success") && one_of(record,"external_service", {"not_entered", "entered"});
}

[[nodiscard]] std::optional<std::uint64_t> validated_callback(const Json& root) {
  const auto top = object(root);
  if (!top || !exact_keys(*top, {"format","phase_one","dispatch","player_route","player_failure"}) || !equals(*top,"format","off.movie-control-cutscene-lifecycle-contract-bundle/v1")) return std::nullopt;
  const auto phase = member(*top,"phase_one"); const auto dispatch = member(*top,"dispatch"); const auto player_route_json = member(*top,"player_route"); const auto failure = member(*top,"player_failure");
  const auto phase_object = phase ? object(*phase) : nullptr; const auto dispatch_object = dispatch ? object(*dispatch) : nullptr; const auto route_array = player_route_json ? array(*player_route_json) : nullptr; const auto failure_object = failure ? object(*failure) : nullptr;
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
  if (!callback || !phase_record(*phase_candidate,true) || !phase_record(*phase_failure,false) || !dispatch_record(*dispatch_candidate,true) || !dispatch_record(*dispatch_failure,false) ||
      !same(*phase_candidate, "callback_ordinal", *phase_failure, "callback_ordinal") ||
      !same(*phase_candidate, "dispatch_order", *phase_failure, "dispatch_order") ||
      !same(*phase_candidate, "callback_ordinal", *dispatch_candidate, "callback_ordinal") ||
      !same(*dispatch_candidate, "callback_ordinal", *dispatch_failure, "callback_ordinal") ||
      !same(*phase_candidate, "component_status_before", *phase_failure, "component_status_before") ||
      !same(*phase_candidate, "owner_status_before", *phase_failure, "owner_status_before") ||
      !same(*phase_candidate, "event_member_before", *phase_failure, "event_member_before") ||
      !same(*phase_candidate, "ordinary_member_before", *phase_failure, "ordinary_member_before") ||
      !same(*dispatch_candidate, "movie_phase_one_completed", *dispatch_failure, "movie_phase_one_completed") ||
      !same(*dispatch_candidate, "movie_phase_two_completed", *dispatch_failure, "movie_phase_two_completed") ||
      !same(*dispatch_candidate, "component_status_before", *dispatch_failure, "component_status_before") ||
      !same(*dispatch_candidate, "owner_status_before", *dispatch_failure, "owner_status_before")) return std::nullopt;
  constexpr std::array phases{"phase_one","phase_two","activation","completion"};
  const Json::Object* player_route[4]{};
  for (std::size_t index{}; index < phases.size(); ++index) { player_route[index] = object((*route_array)[index]); if (!player_route[index] || !player_record(*player_route[index], phases[index], false) || !same(*phase_candidate, "callback_ordinal", *player_route[index], "callback_ordinal")) return std::nullopt; }
  if (!player_record(*failure_object,"failure",true) || !same(*phase_candidate, "callback_ordinal", *failure_object, "callback_ordinal") ||
      !same(*player_route[2], "player_state_before", *failure_object, "player_state_before") ||
      !same(*player_route[2], "component_status_before", *failure_object, "component_status_before") ||
      !same(*player_route[2], "owner_status_before", *failure_object, "owner_status_before")) return std::nullopt;
  for (std::size_t index{}; index + 1U < phases.size(); ++index) {
    if (!same(*player_route[index], "player_state_after", *player_route[index + 1U], "player_state_before")) return std::nullopt;
    const auto current_order = integer(*member(*player_route[index], "observation_order"));
    const auto next_order = integer(*member(*player_route[index + 1U], "observation_order"));
    if (!current_order || !next_order || *current_order >= *next_order) return std::nullopt;
  }
  if (!equals(*player_route[0], "player_state_before", "cold") || !equals(*player_route[0], "player_state_after", "phase_one_ready") ||
      !equals(*player_route[0], "receiver_state", "open") || !equals(*player_route[0], "activation", "not_attempted") || !equals(*player_route[0], "completion", "not_observed") ||
      !equals(*player_route[1], "player_state_before", "phase_one_ready") || !equals(*player_route[1], "player_state_after", "phase_two_ready") ||
      !equals(*player_route[1], "receiver_state", "sealed") || !equals(*player_route[1], "member_sweep", "derived") || !equals(*player_route[1], "reference_sweep", "camera_and_sequence") || !equals(*player_route[1], "activation", "not_attempted") || !equals(*player_route[1], "completion", "not_observed") ||
      !equals(*player_route[2], "player_state_before", "phase_two_ready") || !equals(*player_route[2], "player_state_after", "active") || !equals(*player_route[2], "receiver_state", "sealed") || !equals(*player_route[2], "activation", "started") || !equals(*player_route[2], "completion", "pending") ||
      !equals(*player_route[3], "player_state_before", "active") || !equals(*player_route[3], "player_state_after", "completed") || !equals(*player_route[3], "receiver_state", "sealed") || !equals(*player_route[3], "activation", "not_attempted") || !equals(*player_route[3], "completion", "completed") ||
      equals(*failure_object, "completion", "completed")) return std::nullopt;
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
