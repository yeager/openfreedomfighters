from __future__ import annotations

import copy
import json
import pathlib
import stat
import sys
import tempfile
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import intro_loader_tail_contract as contract  # noqa: E402
import intro_loader_tail_observation as observation  # noqa: E402
import test_intro_loader_tail_observation as fixture  # noqa: E402


TEST_WORKSPACE = pathlib.Path.home() / ".cache" / "openfreedomfighters-test"

def success_bytes() -> bytes:
    clean = observation.sanitize_trace(fixture.success())
    return json.dumps(clean, separators=(",", ":")).encode("utf-8")


def failure_bytes(*, callback: int = 11, prefix_stage: str = "entry",
                  terminal_outcome: str = "failure") -> bytes:
    events = [fixture.event(tail="entered", callback_ordinal=callback)]
    if prefix_stage != "entry":
        events.append(fixture.event(observation_order=1, stage=prefix_stage, tail="entered",
                                    callback_ordinal=callback, named_global="accepted"))
    events.append(fixture.event(observation_order=len(events), stage="failure", tail="entered",
                                callback_ordinal=callback, outcome="failure"))
    clean = observation.sanitize_trace({"format": observation.INPUT_FORMAT, "events": events})
    if terminal_outcome != "failure":
        clean["events"][-1]["outcome"] = terminal_outcome
    return json.dumps(clean, separators=(",", ":")).encode("utf-8")


class IntroLoaderTailContractTests(unittest.TestCase):
    def test_private_paths_reject_parent_links_and_never_overwrite_a_receipt(self) -> None:
        TEST_WORKSPACE.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=TEST_WORKSPACE) as directory:
            private = pathlib.Path(directory)
            target = private / "target"
            target.mkdir()
            alias = private / "alias"
            alias.symlink_to(target.name, target_is_directory=True)
            with self.assertRaisesRegex(ValueError, "must not traverse a symlink"):
                contract._outside_repository(alias / "success.json", "first successful trace")

            receipt_path = private / "receipt.json"
            value = {"format": contract.FORMAT}
            contract._write_new_receipt(receipt_path, value)
            self.assertEqual(stat.S_IMODE(receipt_path.stat().st_mode), 0o600)
            with self.assertRaisesRegex(ValueError, "refusing to overwrite"):
                contract._write_new_receipt(receipt_path, value)

    def test_emits_only_repeat_gate_categories(self) -> None:
        receipt = contract.contract_from_traces(success_bytes(), success_bytes(), failure_bytes())
        self.assertEqual(receipt, {"format": contract.FORMAT,
                                   "repeat_gate": "two-byte-identical-successes",
                                   "success": "complete-concrete-loader-tail",
                                   "failure": "distinct-terminal-failure"})
        rendered = json.dumps(receipt)
        for forbidden in ("callback", "ordinal", "path", "payload", "address"):
            self.assertNotIn(forbidden, rendered)

    def test_rejects_nonidentical_success_callback_order_and_failure_success(self) -> None:
        good = success_bytes()
        changed = copy.deepcopy(json.loads(good))
        changed["events"][1]["named_global"] = "not_present"
        different = json.dumps(changed, separators=(",", ":")).encode("utf-8")
        cases = (
            (good, different, failure_bytes(), "byte-identical"),
            (good, good, failure_bytes(callback=12), "callback"),
            (good, good, failure_bytes(prefix_stage="renderer"), "callback order"),
            (good, good, failure_bytes(terminal_outcome="success"), "complete sanitized"),
        )
        for first, second, failure, message in cases:
            with self.subTest(message=message):
                with self.assertRaisesRegex(ValueError, message):
                    contract.contract_from_traces(first, second, failure)

    def test_rejects_unobserved_or_incomplete_success_services(self) -> None:
        incomplete = json.loads(success_bytes())
        incomplete["events"][-1]["saved_0x4000"] = "not_attempted"
        raw = json.dumps(incomplete, separators=(",", ":")).encode("utf-8")
        with self.assertRaisesRegex(ValueError, "complete sanitized"):
            contract.contract_from_traces(raw, raw, failure_bytes())


if __name__ == "__main__":
    unittest.main()
