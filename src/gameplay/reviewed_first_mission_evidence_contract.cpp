#include "off/gameplay/reviewed_first_mission_evidence_contract.hpp"

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

namespace off::gameplay {
namespace {
constexpr std::string_view filename{"reviewed-first-mission-evidence.json"};
constexpr std::size_t maximum_file_bytes = 64U << 10U;
constexpr std::size_t maximum_depth = 8U;
constexpr std::uint64_t maximum_events = 4096U;

struct Json final {
  using Object = std::map<std::string, Json, std::less<>>;
  std::variant<bool, std::uint64_t, std::string, Object> value;
};
class Parser final {
 public:
  explicit Parser(std::string_view source) : source_(source) {}
  [[nodiscard]] std::optional<Json> parse() { auto result = value(0U); space(); return result && cursor_ == source_.size() ? result : std::nullopt; }
 private:
  void space() { while (cursor_ < source_.size() && std::isspace(static_cast<unsigned char>(source_[cursor_]))) ++cursor_; }
  [[nodiscard]] bool consume(char expected) { space(); if (cursor_ == source_.size() || source_[cursor_] != expected) return false; ++cursor_; return true; }
  [[nodiscard]] std::optional<std::string> string() {
    space(); if (cursor_ == source_.size() || source_[cursor_++] != '"') return std::nullopt;
    std::string result;
    while (cursor_ < source_.size()) { const char c = source_[cursor_++]; if (c == '"') return result; const auto byte = static_cast<unsigned char>(c); if (c == '\\' || byte < 0x20U || byte > 0x7EU || result.size() >= 128U) return std::nullopt; result.push_back(c); }
    return std::nullopt;
  }
  [[nodiscard]] std::optional<Json> number() {
    space(); const auto begin = cursor_; while (cursor_ < source_.size() && source_[cursor_] >= '0' && source_[cursor_] <= '9') ++cursor_;
    if (begin == cursor_ || (cursor_ - begin > 1U && source_[begin] == '0')) return std::nullopt;
    std::uint64_t result{}; for (std::size_t i = begin; i < cursor_; ++i) { const auto digit = static_cast<std::uint64_t>(source_[i] - '0'); if (result > (std::numeric_limits<std::uint64_t>::max() - digit) / 10U) return std::nullopt; result = result * 10U + digit; } return Json{result};
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
    if (source_.substr(cursor_).starts_with("true")) { cursor_ += 4U; return Json{true}; }
    if (source_.substr(cursor_).starts_with("false")) { cursor_ += 5U; return Json{false}; }
    return number();
  }
  std::string_view source_; std::size_t cursor_{};
};
[[nodiscard]] bool regular_non_link(const std::filesystem::path& path) noexcept { std::error_code e; const auto s = std::filesystem::symlink_status(path, e); return !e && std::filesystem::is_regular_file(s) && !std::filesystem::is_symlink(s); }
[[nodiscard]] bool directory_non_link(const std::filesystem::path& path) noexcept { std::error_code e; const auto s = std::filesystem::symlink_status(path, e); return !e && std::filesystem::is_directory(s) && !std::filesystem::is_symlink(s); }
[[nodiscard]] const Json::Object* object(const Json& value) { return std::get_if<Json::Object>(&value.value); }
[[nodiscard]] const std::string* string(const Json& value) { return std::get_if<std::string>(&value.value); }
[[nodiscard]] const std::uint64_t* integer(const Json& value) { return std::get_if<std::uint64_t>(&value.value); }
[[nodiscard]] const Json* member(const Json::Object& value, std::string_view key) { const auto found = value.find(key); return found == value.end() ? nullptr : &found->second; }
template <std::ranges::input_range Keys> [[nodiscard]] bool exact_keys(const Json::Object& value, const Keys& keys) { return value.size() == keys.size() && std::ranges::all_of(keys, [&value](const auto& key) { return value.contains(key); }); }
[[nodiscard]] bool equals(const Json::Object& value, std::string_view key, std::string_view expected) { const auto item = member(value, key); const auto actual = item ? string(*item) : nullptr; return actual && *actual == expected; }
[[nodiscard]] std::optional<std::uint64_t> natural(const Json::Object& value, std::string_view key) { const auto item = member(value, key); const auto result = item ? integer(*item) : nullptr; return result ? std::optional<std::uint64_t>{*result} : std::nullopt; }
[[nodiscard]] const std::string* text(const Json::Object& value, std::string_view key) { const auto item = member(value, key); return item ? string(*item) : nullptr; }
[[nodiscard]] bool exact_natural(const Json::Object& value, std::string_view key, std::uint64_t expected) { const auto item = member(value, key); const auto actual = item ? integer(*item) : nullptr; return actual && *actual == expected; }
[[nodiscard]] bool opaque_fingerprint(const Json::Object& value) { const auto item = member(value, "verified_data_manifest_fingerprint"); const auto fingerprint = item ? string(*item) : nullptr; return fingerprint && fingerprint->size() == 64U && std::ranges::all_of(*fingerprint, [](unsigned char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); }); }
[[nodiscard]] bool one_of(std::string_view value, std::initializer_list<std::string_view> allowed) { return std::ranges::find(allowed, value) != allowed.end(); }
[[nodiscard]] std::optional<FirstMissionProbe> probe(std::string_view value) {
  constexpr std::array<std::pair<std::string_view, FirstMissionProbe>, 11U> values{{
      {"movement", FirstMissionProbe::movement}, {"look", FirstMissionProbe::look},
      {"movement_look", FirstMissionProbe::movement_look}, {"fire", FirstMissionProbe::fire},
      {"aim", FirstMissionProbe::aim}, {"interact", FirstMissionProbe::interact},
      {"squad", FirstMissionProbe::squad}, {"pause", FirstMissionProbe::pause},
      {"menu", FirstMissionProbe::menu}, {"obstacle", FirstMissionProbe::obstacle},
      {"interaction_candidate", FirstMissionProbe::interaction_candidate},
  }};
  const auto found = std::ranges::find_if(values, [value](const auto& item) { return item.first == value; });
  return found == values.end() ? std::nullopt : std::optional<FirstMissionProbe>{found->second};
}
[[nodiscard]] std::optional<FirstMissionBoundary> boundary(std::string_view value) {
  constexpr std::array<std::pair<std::string_view, FirstMissionBoundary>, 8U> values{{
      {"handoff", FirstMissionBoundary::handoff}, {"control", FirstMissionBoundary::control},
      {"camera", FirstMissionBoundary::camera}, {"player", FirstMissionBoundary::player},
      {"collision", FirstMissionBoundary::collision}, {"interaction", FirstMissionBoundary::interaction},
      {"mission", FirstMissionBoundary::mission}, {"hud", FirstMissionBoundary::hud},
  }};
  const auto found = std::ranges::find_if(values, [value](const auto& item) { return item.first == value; });
  return found == values.end() ? std::nullopt : std::optional<FirstMissionBoundary>{found->second};
}
[[nodiscard]] std::optional<FirstMissionObservedState> state(std::string_view value) {
  constexpr std::array<std::pair<std::string_view, FirstMissionObservedState>, 35U> values{{
      {"cinematic_visible", FirstMissionObservedState::cinematic_visible}, {"loading_visible", FirstMissionObservedState::loading_visible}, {"gameplay_viewport_visible", FirstMissionObservedState::gameplay_viewport_visible}, {"no_response", FirstMissionObservedState::no_response}, {"movement_only", FirstMissionObservedState::movement_only}, {"camera_only", FirstMissionObservedState::camera_only}, {"movement_and_camera", FirstMissionObservedState::movement_and_camera}, {"blocked_by_overlay", FirstMissionObservedState::blocked_by_overlay}, {"fixed", FirstMissionObservedState::fixed}, {"follows_translation", FirstMissionObservedState::follows_translation}, {"rotates_with_look", FirstMissionObservedState::rotates_with_look}, {"independently_rotatable", FirstMissionObservedState::independently_rotatable}, {"absent", FirstMissionObservedState::absent}, {"spawned", FirstMissionObservedState::spawned}, {"controllable", FirstMissionObservedState::controllable}, {"disabled", FirstMissionObservedState::disabled}, {"no_contact", FirstMissionObservedState::no_contact}, {"blocked", FirstMissionObservedState::blocked}, {"sliding", FirstMissionObservedState::sliding}, {"stepped", FirstMissionObservedState::stepped}, {"falling", FirstMissionObservedState::falling}, {"unavailable", FirstMissionObservedState::unavailable}, {"prompt_only", FirstMissionObservedState::prompt_only}, {"entered_range", FirstMissionObservedState::entered_range}, {"accepted", FirstMissionObservedState::accepted}, {"rejected", FirstMissionObservedState::rejected}, {"unchanged", FirstMissionObservedState::unchanged}, {"objective_advanced", FirstMissionObservedState::objective_advanced}, {"failed", FirstMissionObservedState::failed}, {"completed", FirstMissionObservedState::completed}, {"loading", FirstMissionObservedState::loading}, {"stable", FirstMissionObservedState::stable}, {"changed", FirstMissionObservedState::changed}, {"hidden", FirstMissionObservedState::hidden}, {"unknown", FirstMissionObservedState::unknown},
  }};
  const auto found = std::ranges::find_if(values, [value](const auto& item) { return item.first == value; });
  return found == values.end() ? std::nullopt : std::optional<FirstMissionObservedState>{found->second};
}
[[nodiscard]] bool state_for_boundary(FirstMissionBoundary b, FirstMissionObservedState s) {
  using B=FirstMissionBoundary; using S=FirstMissionObservedState;
  switch (b) { case B::handoff: return s==S::cinematic_visible || s==S::loading_visible || s==S::gameplay_viewport_visible; case B::control: return s==S::no_response || s==S::movement_only || s==S::camera_only || s==S::movement_and_camera || s==S::blocked_by_overlay; case B::camera: return s==S::fixed || s==S::follows_translation || s==S::rotates_with_look || s==S::independently_rotatable || s==S::unknown; case B::player: return s==S::absent || s==S::spawned || s==S::controllable || s==S::disabled || s==S::unknown; case B::collision: return s==S::no_contact || s==S::blocked || s==S::sliding || s==S::stepped || s==S::falling || s==S::unknown; case B::interaction: return s==S::unavailable || s==S::prompt_only || s==S::entered_range || s==S::accepted || s==S::rejected || s==S::unknown; case B::mission: return s==S::unchanged || s==S::objective_advanced || s==S::failed || s==S::completed || s==S::loading || s==S::unknown; case B::hud: return s==S::absent || s==S::stable || s==S::changed || s==S::hidden || s==S::unknown; } return false;
}
[[nodiscard]] std::optional<FirstMissionEvidenceFacts> validated(const Json& root) {
  const auto top=object(root); constexpr std::array keys{"format","method_version","verified_data_manifest_fingerprint","platform","architecture","input_device","baseline_run_count","baseline_event_count","baseline_visible_change_count","experiment_run_count","experiment_probe","experiment_event_count","visible_action_outcome","reset_or_terminal_outcome"};
  const auto platform = top ? text(*top, "platform") : nullptr; const auto architecture = top ? text(*top, "architecture") : nullptr; const auto input_device = top ? text(*top, "input_device") : nullptr;
  if (!top || !exact_keys(*top,keys) || !equals(*top,"format","off.first-mission-observation-repeat-bundle/v1") || !opaque_fingerprint(*top) || !platform || !architecture || !input_device || !one_of(*platform,{"windows","linux","macos"}) || !one_of(*architecture,{"x86","x86_64","arm64"}) || !one_of(*input_device,{"keyboard_mouse","controller"})) return std::nullopt;
  const auto method=natural(*top,"method_version"); const auto base_events=natural(*top,"baseline_event_count"); const auto base_visible=natural(*top,"baseline_visible_change_count"); const auto experiment_events=natural(*top,"experiment_event_count");
  if (!method || *method == 0U || *method > 65535U || !exact_natural(*top,"baseline_run_count",2U) || !base_events || *base_events == 0U || *base_events > maximum_events || !base_visible || *base_visible > *base_events || !exact_natural(*top,"experiment_run_count",1U) || !experiment_events || *experiment_events < 3U || *experiment_events > maximum_events) return std::nullopt;
  const auto probe_value=member(*top,"experiment_probe"); const auto probe_text=probe_value ? string(*probe_value) : nullptr; const auto visible_value=member(*top,"visible_action_outcome"); const auto terminal_value=member(*top,"reset_or_terminal_outcome"); const auto visible=visible_value ? object(*visible_value) : nullptr; const auto terminal=terminal_value ? object(*terminal_value) : nullptr;
  constexpr std::array outcome_keys{"boundary","state"}; if (!probe_text || !visible || !terminal || !exact_keys(*visible,outcome_keys) || !exact_keys(*terminal,outcome_keys)) return std::nullopt;
  const auto p=probe(*probe_text); const auto visible_boundary_text=member(*visible,"boundary"); const auto visible_state_text=member(*visible,"state"); const auto terminal_state_text=member(*terminal,"state"); const auto vb=visible_boundary_text ? string(*visible_boundary_text) : nullptr; const auto vs=visible_state_text ? string(*visible_state_text) : nullptr; const auto ts=terminal_state_text ? string(*terminal_state_text) : nullptr;
  if (!p || !vb || !vs || !ts || !equals(*terminal,"boundary","mission")) return std::nullopt;
  const auto b=boundary(*vb);
  const auto v=state(*vs);
  const auto t=state(*ts);
  if (!b || !v || !t || !state_for_boundary(*b,*v) ||
      !(*t==FirstMissionObservedState::loading || *t==FirstMissionObservedState::failed ||
        *t==FirstMissionObservedState::completed)) return std::nullopt;
  return FirstMissionEvidenceFacts{.probe=*p,.visible_boundary=*b,.visible_state=*v,.terminal_state=*t};
}
}  // namespace

std::optional<ReviewedFirstMissionEvidenceContract> ReviewedFirstMissionEvidenceContract::load_local(const std::filesystem::path& local_directory) {
  if (local_directory.empty() || !directory_non_link(local_directory)) return std::nullopt;
  const auto path=local_directory / std::string{filename}; if (path.parent_path()!=local_directory || !regular_non_link(path)) return std::nullopt;
  std::error_code error; const auto size=std::filesystem::file_size(path,error); if (error || size==0U || size>maximum_file_bytes) return std::nullopt;
  std::ifstream input(path,std::ios::binary); std::string bytes(static_cast<std::size_t>(size),'\0'); input.read(bytes.data(),static_cast<std::streamsize>(bytes.size())); if (!input || input.gcount()!=static_cast<std::streamsize>(bytes.size()) || input.peek()!=std::char_traits<char>::eof()) return std::nullopt;
  const auto parsed=Parser{bytes}.parse(); if (!parsed) return std::nullopt; const auto facts=validated(*parsed); return facts ? std::optional<ReviewedFirstMissionEvidenceContract>{ReviewedFirstMissionEvidenceContract{*facts}} : std::nullopt;
}
}  // namespace off::gameplay
