#!/usr/bin/env python3
import copy
import pathlib
import sys
import unittest


ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import menu_visual_layout_trace as trace  # noqa: E402


def sample(phase="focused"):
    return {
        "observation_order": 1,
        "phase": phase,
        "viewport": {"width": 1280, "height": 720},
        "panel": {"x": 200, "y": 100, "width": 880, "height": 520},
        "elements": [
            {"role": "panel", "bounds": {"x": 200, "y": 100, "width": 880, "height": 520}},
            {"role": "heading", "bounds": {"x": 300, "y": 150, "width": 240, "height": 40}},
            {"role": "row_label", "bounds": {"x": 300, "y": 240, "width": 160, "height": 24}},
            {"role": "row_value", "bounds": {"x": 650, "y": 240, "width": 160, "height": 24}},
            {"role": "selected_focus", "bounds": {"x": 280, "y": 226, "width": 560, "height": 48}},
        ],
    }


def raw(*samples):
    return {"format": trace.INPUT_FORMAT, "samples": list(samples)}


class MenuVisualLayoutTraceTests(unittest.TestCase):
    def test_keeps_only_geometry_and_categories(self):
        result = trace.sanitize_trace(raw(sample()))
        self.assertEqual(result["format"], trace.OUTPUT_FORMAT)
        self.assertEqual(result["samples"][0]["phase"], "focused")
        self.assertEqual(result["samples"][0]["elements"][1]["role"], "heading")

    def test_rejects_retail_text_or_unknown_fields(self):
        specimen = sample()
        specimen["elements"][1]["text"] = "retail text"
        with self.assertRaises(ValueError):
            trace.sanitize_trace(raw(specimen))

    def test_rejects_non_panel_element_outside_panel(self):
        specimen = sample()
        specimen["elements"][2]["bounds"]["x"] = 1
        with self.assertRaises(ValueError):
            trace.sanitize_trace(raw(specimen))

    def test_focused_trace_requires_focus_bounds(self):
        specimen = sample()
        specimen["elements"].pop()
        with self.assertRaises(ValueError):
            trace.sanitize_trace(raw(specimen))

    def test_order_and_singleton_roles_are_strict(self):
        first = sample("entry")
        second = copy.deepcopy(sample("submenu"))
        second["observation_order"] = 1
        with self.assertRaises(ValueError):
            trace.sanitize_trace(raw(first, second))
        specimen = sample()
        specimen["elements"].append(copy.deepcopy(specimen["elements"][1]))
        with self.assertRaises(ValueError):
            trace.sanitize_trace(raw(specimen))


if __name__ == "__main__":
    unittest.main()
