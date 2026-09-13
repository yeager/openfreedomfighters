#include "off/graphics/reviewed_movie_control_lifecycle_contract.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void write(const std::filesystem::path& path, const std::string& contents) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output << contents;
  check(static_cast<bool>(output), "fixture write succeeds");
}
void replace_first(std::string& value, std::string_view from, std::string_view to) {
  const auto position = value.find(from);
  check(position != std::string::npos, "fixture replacement source exists");
  value.replace(position, from.size(), to);
}
void replace_all(std::string& value, std::string_view from, std::string_view to) {
  std::size_t position{};
  while ((position = value.find(from, position)) != std::string::npos) {
    value.replace(position, from.size(), to);
    position += to.size();
  }
}
// This is authored structural fixture data, not a retail observation.
std::string contract() {
  const std::string phase_ok = R"({"dispatch_order":4,"callback_ordinal":7,"component_is_constructed":true,"owner_is_constructed_owner":true,"global_lifecycle_entered":true,"global_lifecycle_completed":true,"global_lifecycle_outcome":"success","phase_one_completed":true,"outcome":"success","component_status_before":0,"component_status_after":4,"owner_status_before":0,"owner_status_after":4,"event_member_before":false,"event_member_after":true,"external_service":"entered","ordinary_member_before":false,"ordinary_member_after":true})";
  const std::string phase_bad = R"({"dispatch_order":4,"callback_ordinal":7,"component_is_constructed":true,"owner_is_constructed_owner":true,"global_lifecycle_entered":true,"global_lifecycle_completed":false,"global_lifecycle_outcome":"failure","phase_one_completed":false,"outcome":"failure","component_status_before":0,"component_status_after":0,"owner_status_before":0,"owner_status_after":0,"event_member_before":false,"event_member_after":false,"external_service":"entered","ordinary_member_before":false,"ordinary_member_after":false})";
  const std::string dispatch_ok = R"({"observation_order":0,"phase":"player_activation","callback_ordinal":7,"movie_component_is_constructed":true,"movie_owner_is_constructed_owner":true,"sequence_component_is_constructed":true,"sequence_owner_is_constructed_owner":true,"movie_phase_one_completed":true,"movie_phase_two_completed":true,"component_status_before":4,"component_status_after":4,"owner_status_before":0,"owner_status_after":0,"event16_gate":"admitted","handoff_sender_is_movie_owner":true,"handoff_target_is_sequence_owner":true,"handoff":"delivered","delivery_mode":"synchronous","player_activation":"started","outcome":"success","external_service":"entered"})";
  const std::string dispatch_bad = R"({"observation_order":0,"phase":"failure","callback_ordinal":7,"movie_component_is_constructed":true,"movie_owner_is_constructed_owner":true,"sequence_component_is_constructed":true,"sequence_owner_is_constructed_owner":true,"movie_phase_one_completed":true,"movie_phase_two_completed":true,"component_status_before":4,"component_status_after":4,"owner_status_before":0,"owner_status_after":0,"event16_gate":"admitted","handoff_sender_is_movie_owner":true,"handoff_target_is_sequence_owner":true,"handoff":"failed","delivery_mode":"not_observed","player_activation":"not_started","outcome":"failure","external_service":"entered"})";
  const auto player = [](std::uint64_t order, std::string_view phase, std::string_view before, std::string_view after, std::string_view receiver, std::string_view members, std::string_view references, std::string_view activation, std::string_view completion, std::string_view outcome) {
    return "{\"observation_order\":" + std::to_string(order) + ",\"phase\":\"" + std::string(phase) + "\",\"callback_ordinal\":7,\"sequence_component_constructed\":true,\"sequence_owner_constructed\":true,\"reader_graph_receipt\":\"complete\",\"component_status_before\":4,\"component_status_after\":4,\"owner_status_before\":4,\"owner_status_after\":4,\"player_state_before\":\"" + std::string(before) + "\",\"player_state_after\":\"" + std::string(after) + "\",\"receiver_state\":\"" + std::string(receiver) + "\",\"member_sweep\":\"" + std::string(members) + "\",\"reference_sweep\":\"" + std::string(references) + "\",\"activation\":\"" + std::string(activation) + "\",\"completion\":\"" + std::string(completion) + "\",\"outcome\":\"" + std::string(outcome) + "\",\"external_service\":\"entered\"}";
  };
  const auto one = player(0,"phase_one","cold","phase_one_ready","open","not_entered","not_entered","not_attempted","not_observed","success");
  const auto two = player(1,"phase_two","phase_one_ready","phase_two_ready","sealed","derived","camera_and_sequence","not_attempted","not_observed","success");
  const auto three = player(2,"activation","phase_two_ready","active","sealed","not_entered","not_entered","started","pending","success");
  const auto four = player(3,"completion","active","completed","sealed","not_entered","not_entered","not_attempted","completed","success");
  const auto failed = player(0,"failure","phase_two_ready","failed","sealed","not_entered","not_entered","failed","failed","failure");
  return "{\"format\":\"off.movie-control-cutscene-lifecycle-contract-bundle/v1\",\"phase_one\":{\"candidate\":" + phase_ok + ",\"failure\":" + phase_bad + "},\"dispatch\":{\"candidate\":" + dispatch_ok + ",\"failure\":" + dispatch_bad + "},\"player_route\":[" + one + "," + two + "," + three + "," + four + "],\"player_failure\":" + failed + "}";
}

std::string structurally_varied_contract() {
  auto result = contract();
  // None of these values is a runtime contract.  They demonstrate that local
  // admission keeps the producer's bounds and cross-record relations rather
  // than encoding one fixture's ordinal, order, or status masks.
  for (int index{}; index < 9; ++index) replace_first(result, "\"callback_ordinal\":7", "\"callback_ordinal\":65535");
  replace_all(result, "\"component_status_before\":0", "\"component_status_before\":91");
  replace_all(result, "\"owner_status_before\":0", "\"owner_status_before\":93");
  replace_first(result, "\"dispatch_order\":4,", "\"dispatch_order\":4096,");
  replace_first(result, "\"dispatch_order\":4,", "\"dispatch_order\":4096,");
  replace_first(result, "\"observation_order\":0,\"phase\":\"phase_one\"", "\"observation_order\":23,\"phase\":\"phase_one\"");
  replace_first(result, "\"observation_order\":1,\"phase\":\"phase_two\"", "\"observation_order\":41,\"phase\":\"phase_two\"");
  replace_first(result, "\"observation_order\":2,\"phase\":\"activation\"", "\"observation_order\":57,\"phase\":\"activation\"");
  replace_first(result, "\"observation_order\":3,\"phase\":\"completion\"", "\"observation_order\":4096,\"phase\":\"completion\"");
  return result;
}
}

int main() {
  try {
    const std::filesystem::path root{OFF_TEST_WORK_DIR};
    const auto directory = root / "reviewed-movie-control-lifecycle";
    std::error_code error; std::filesystem::remove_all(root, error); std::filesystem::create_directories(directory, error);
    check(!error, "fixture directory exists");
    const auto path = directory / "reviewed-movie-control-lifecycle.json";
    write(path, contract());
    const auto admitted = off::graphics::ReviewedMovieControlLifecycleContract::load_local(directory);
    check(admitted && admitted->admitted(), "exact reviewed lifecycle is admitted inertly");
    write(path, structurally_varied_contract());
    check(off::graphics::ReviewedMovieControlLifecycleContract::load_local(directory).has_value(), "bounded varied source-free structural values are admitted");
    auto mismatched_callback = structurally_varied_contract();
    replace_first(mismatched_callback, "\"callback_ordinal\":65535", "\"callback_ordinal\":65534");
    write(path, mismatched_callback);
    check(!off::graphics::ReviewedMovieControlLifecycleContract::load_local(directory), "cross-bundle callback mismatch is rejected");
    auto mismatched_precondition = structurally_varied_contract();
    replace_first(mismatched_precondition, "\"component_status_before\":91", "\"component_status_before\":90");
    write(path, mismatched_precondition);
    check(!off::graphics::ReviewedMovieControlLifecycleContract::load_local(directory), "phase failure precondition mismatch is rejected");
    write(path, contract() + std::string(1, '\0'));
    check(!off::graphics::ReviewedMovieControlLifecycleContract::load_local(directory), "trailing JSON is rejected");
    write(path, R"({"format":"off.movie-control-cutscene-lifecycle-contract-bundle/v1","phase_one":{},"dispatch":{},"player_route":[],"player_failure":{}})");
    check(!off::graphics::ReviewedMovieControlLifecycleContract::load_local(directory), "missing nested contract members are rejected safely");
    write(path, contract());
    const auto link = root / "linked-contract.json"; std::filesystem::rename(path, link, error); check(!error, "fixture moves");
    std::filesystem::create_symlink(link, path, error); check(!error, "symlink fixture exists");
    check(!off::graphics::ReviewedMovieControlLifecycleContract::load_local(directory), "symlink is rejected");
    std::filesystem::remove_all(root, error);
    std::cout << "reviewed MovieControl lifecycle contract tests passed\n";
  } catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
