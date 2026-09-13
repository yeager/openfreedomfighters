#!/usr/bin/env python3
"""Intro visual-timeline receipts must remain source-free and bounded."""

import copy
import pathlib
import sys
import unittest


ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import intro_visual_timeline_audit as audit  # noqa: E402


def raw(*samples):
    return {
        "format": audit.INPUT_FORMAT,
        "session_scope": "fresh_isolated",
        "capture_extent": {"width": 1280, "height": 720},
        "samples": list(samples),
    }


def initial(order=1):
    return {"capture_order": order, "relation": "initial"}


def changed(order=2):
    return {"capture_order": order, "relation": "changed"}


class IntroVisualTimelineAuditTests(unittest.TestCase):
    def test_keeps_only_structural_change_orders(self):
        result = audit.sanitize_audit(raw(initial(), {"capture_order": 2, "relation": "unchanged"}, changed(3)))
        self.assertEqual(result, {
            "format": audit.OUTPUT_FORMAT,
            "session_scope": "fresh_isolated",
            "capture_extent": {"width": 1280, "height": 720},
            "sample_count": 3,
            "changed_capture_orders": [3],
        })

    def test_rejects_images_text_and_unknown_fields(self):
        specimen = raw(initial(), changed())
        specimen["samples"][1]["image"] = "not permitted"
        with self.assertRaises(ValueError):
            audit.sanitize_audit(specimen)
        specimen = raw(initial(), changed())
        specimen["comment"] = "not permitted"
        with self.assertRaises(ValueError):
            audit.sanitize_audit(specimen)

    def test_requires_fresh_isolated_ordered_state_change(self):
        specimen = raw(initial(), changed())
        specimen["session_scope"] = "attached"
        with self.assertRaises(ValueError):
            audit.sanitize_audit(specimen)
        specimen = raw(changed(), initial())
        with self.assertRaises(ValueError):
            audit.sanitize_audit(specimen)
        specimen = raw(initial(), {"capture_order": 2, "relation": "unchanged"})
        with self.assertRaises(ValueError):
            audit.sanitize_audit(specimen)

    def test_rejects_non_monotonic_or_invalid_extent(self):
        specimen = raw(initial(), changed(1))
        with self.assertRaises(ValueError):
            audit.sanitize_audit(specimen)
        specimen = raw(initial(), changed())
        specimen["capture_extent"] = copy.deepcopy(specimen["capture_extent"])
        specimen["capture_extent"]["width"] = 0
        with self.assertRaises(ValueError):
            audit.sanitize_audit(specimen)


if __name__ == "__main__":
    unittest.main()
