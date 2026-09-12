#include "off/gameplay/reviewed_first_mission_evidence_contract.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void write(const std::filesystem::path& path, const std::string& contents) { std::ofstream output(path, std::ios::binary | std::ios::trunc); output << contents; check(static_cast<bool>(output), "fixture write succeeds"); }
// Authored structural fixture; no retail observation is present here.
std::string contract() { return R"({"format":"off.first-mission-observation-repeat-bundle/v1","method_version":1,"verified_data_manifest_fingerprint":"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef","platform":"windows","architecture":"x86_64","input_device":"keyboard_mouse","baseline_run_count":2,"baseline_event_count":3,"baseline_visible_change_count":0,"experiment_run_count":1,"experiment_probe":"movement","experiment_event_count":3,"visible_action_outcome":{"boundary":"control","state":"movement_only"},"reset_or_terminal_outcome":{"boundary":"mission","state":"loading"}})"; }
}
int main() {
  try {
    const std::filesystem::path root{OFF_TEST_WORK_DIR}; const auto directory=root / "reviewed-first-mission-evidence"; std::error_code error; std::filesystem::remove_all(root,error); std::filesystem::create_directories(directory,error); check(!error,"fixture directory exists");
    const auto path=directory / "reviewed-first-mission-evidence.json"; write(path,contract());
    const auto admitted=off::gameplay::ReviewedFirstMissionEvidenceContract::load_local(directory); check(admitted && admitted->admitted(),"exact aggregate bundle is admitted inertly"); check(admitted->facts().probe==off::gameplay::FirstMissionProbe::movement && admitted->facts().visible_state==off::gameplay::FirstMissionObservedState::movement_only && admitted->facts().terminal_state==off::gameplay::FirstMissionObservedState::loading,"only behavior categories are retained");
    write(path,contract()+std::string(1,'\0')); check(!off::gameplay::ReviewedFirstMissionEvidenceContract::load_local(directory),"trailing bytes are rejected");
    write(path,R"({"format":"off.first-mission-observation-repeat-bundle/v1"})"); check(!off::gameplay::ReviewedFirstMissionEvidenceContract::load_local(directory),"incomplete schema is rejected");
    auto invalid_terminal=contract();
    invalid_terminal.replace(invalid_terminal.find("\"state\":\"loading\""), 17U,
                             "\"state\":\"stable\"");
    write(path,invalid_terminal);
    check(!off::gameplay::ReviewedFirstMissionEvidenceContract::load_local(directory),
          "nonterminal mission category is rejected");
    write(path,contract()); const auto linked=root / "linked-contract.json"; std::filesystem::rename(path,linked,error); check(!error,"fixture moves"); std::filesystem::create_symlink(linked,path,error); check(!error,"symlink fixture exists"); check(!off::gameplay::ReviewedFirstMissionEvidenceContract::load_local(directory),"symlink is rejected");
    std::filesystem::remove_all(root,error); std::cout << "reviewed first-mission evidence contract tests passed\n";
  } catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
