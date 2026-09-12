"""Structural safeguards for the development-package workflow."""

import pathlib
import unittest


WORKFLOW = pathlib.Path(__file__).parents[1] / ".github" / "workflows" / "release.yml"


class ReleaseWorkflowTests(unittest.TestCase):
    def setUp(self):
        self.workflow = WORKFLOW.read_text(encoding="utf-8")

    def test_workflow_is_artifact_only_and_read_only(self):
        self.assertIn("permissions:\n  contents: read", self.workflow)
        self.assertIn("actions/upload-artifact@v7", self.workflow)
        self.assertNotIn("contents: write", self.workflow)
        self.assertNotIn("softprops/action-gh-release", self.workflow)
        self.assertNotIn("actions/create-release", self.workflow)
        self.assertNotIn("gh release", self.workflow)
        self.assertNotIn("create-release:", self.workflow)


if __name__ == "__main__":
    unittest.main()
