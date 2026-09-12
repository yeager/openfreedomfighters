"""MovieControl observer probe plans remain opaque and source-free."""

from __future__ import annotations

import pathlib
import sys
import unittest


sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
import movie_control_observer_probe_plan as probe_plan  # noqa: E402


def plan(**changes: object) -> dict[str, object]:
    value: dict[str, object] = {
        "format": probe_plan.INPUT_FORMAT,
        "probes": [
            {"slot": slot, "point": point}
            for slot, point in enumerate(probe_plan.PROTOCOL_POINTS)
        ],
    }
    value.update(changes)
    return value


class MovieControlObserverProbePlanTests(unittest.TestCase):
    def test_accepts_only_the_fixed_opaque_protocol(self) -> None:
        self.assertEqual(
            probe_plan.validate_probe_plan(plan()),
            {"format": probe_plan.OUTPUT_FORMAT,
             "probes": [
                 {"slot": slot, "point": point}
                 for slot, point in enumerate(probe_plan.PROTOCOL_POINTS)
             ]},
        )

    def test_canonicalizes_unordered_probes_and_rejects_bad_sets(self) -> None:
        unordered = [
            {"slot": 3, "point": "global_phase_one_leave"},
            {"slot": 1, "point": "candidate_enter"},
            {"slot": 0, "point": "global_phase_one_enter"},
            {"slot": 2, "point": "candidate_leave"},
        ]
        self.assertEqual(
            probe_plan.validate_probe_plan(plan(probes=unordered))["probes"],
            plan()["probes"],
        )
        invalid = (
            [{"slot": 0, "point": "global_phase_one_enter"}],
            [{"slot": 1, "point": "global_phase_one_enter"},
             {"slot": 0, "point": "candidate_enter"},
             {"slot": 2, "point": "candidate_leave"},
             {"slot": 3, "point": "global_phase_one_leave"}],
            [{"slot": 0, "point": "global_phase_one_enter"},
             {"slot": 1, "point": "candidate_enter"},
             {"slot": 2, "point": "candidate_enter"},
             {"slot": 3, "point": "global_phase_one_leave"}],
            [{"slot": 0, "point": "global_phase_one_enter"},
             {"slot": 1, "point": "candidate_enter"},
             {"slot": 2, "point": "candidate_leave"},
             {"slot": 3, "point": "global_phase_one_leave"},
             {"slot": 4, "point": "other"}],
        )
        for probes in invalid:
            with self.subTest(probes=probes):
                with self.assertRaises(ValueError):
                    probe_plan.validate_probe_plan(plan(probes=probes))

    def test_rejects_target_locator_and_retail_material_fields(self) -> None:
        for forbidden in (
            "locator", "address", "offset", "symbol", "path", "executable",
            "raw", "bytes", "payload", "identifier", "command", "script",
        ):
            with self.subTest(forbidden=forbidden):
                specimen = plan()
                specimen[forbidden] = "never accepted"
                with self.assertRaises(ValueError):
                    probe_plan.validate_probe_plan(specimen)

    def test_rejects_unrecognized_format_and_noncanonical_probe_shapes(self) -> None:
        with self.assertRaises(ValueError):
            probe_plan.validate_probe_plan(plan(format="other"))
        with self.assertRaises(ValueError):
            probe_plan.validate_probe_plan(plan(probes=[0, 1, 2, 3]))
        malformed = plan()
        malformed["probes"][0]["address"] = 1  # type: ignore[index]
        with self.assertRaises(ValueError):
            probe_plan.validate_probe_plan(malformed)

    def test_repository_paths_are_never_accepted_for_private_io(self) -> None:
        repository_file = pathlib.Path(__file__).resolve()
        with self.assertRaises(ValueError):
            probe_plan._outside_repository(repository_file, "input")


if __name__ == "__main__":
    unittest.main()
