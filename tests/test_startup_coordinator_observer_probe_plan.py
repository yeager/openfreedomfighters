"""Startup coordinator observer plans cannot carry target material."""

from __future__ import annotations

import pathlib
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
import startup_coordinator_observer_probe_plan as plan  # noqa: E402


def raw(**changes: object) -> dict[str, object]:
    value: dict[str, object] = {"format": plan.INPUT_FORMAT, "probes": [
        {"slot": slot, "point": point} for slot, point in enumerate(plan.PROTOCOL_POINTS)
    ]}
    value.update(changes)
    return value


class StartupCoordinatorObserverProbePlanTests(unittest.TestCase):
    def test_accepts_exact_fixed_protocol_and_orders_slots(self) -> None:
        unordered = list(reversed(raw()["probes"]))  # type: ignore[index]
        self.assertEqual(plan.validate_probe_plan(raw(probes=unordered)), {
            "format": plan.OUTPUT_FORMAT,
            "probes": [{"slot": slot, "point": point}
                       for slot, point in enumerate(plan.PROTOCOL_POINTS)],
        })

    def test_rejects_target_and_retail_fields(self) -> None:
        for forbidden in ("address", "offset", "symbol", "path", "locator", "pid", "bytes", "text"):
            with self.subTest(forbidden=forbidden):
                candidate = raw()
                candidate[forbidden] = "forbidden"
                with self.assertRaises(ValueError):
                    plan.validate_probe_plan(candidate)


if __name__ == "__main__":
    unittest.main()
