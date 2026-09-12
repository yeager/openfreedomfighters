from __future__ import annotations
import pathlib, sys, unittest
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]/"tools"))
import scene_manager_lifecycle_trace as trace
import scene_manager_lifecycle_contract_bundle as bundle
def event(**changes):
    value={"observation_order":0,"phase":"completion","callback_ordinal":4,"manager":"entered","previous_scene":"retained","staging":"ready","global_lifecycle":"completed","component_phase_one":"completed","camera_route":"ready","commit":"committed","outcome":"success","external_service":"entered"}; value.update(changes); return value
def clean(*events): return trace.sanitize_trace({"format":trace.INPUT_FORMAT,"events":list(events)})
class Tests(unittest.TestCase):
 def test_contract_needs_identical_completed_pair_and_separate_failure(self):
  good=clean(event()); bad=clean(event(phase="failure",callback_ordinal=5,staging="rejected",global_lifecycle="not_attempted",component_phase_one="not_attempted",camera_route="not_attempted",commit="not_attempted",outcome="failure"))
  receipt=bundle.bundle_contract(good,good,bad); self.assertEqual(receipt["format"],bundle.OUTPUT_FORMAT); self.assertTrue(receipt["candidate"]["scene_committed"])
  with self.assertRaises(ValueError): bundle.bundle_contract(good,clean(event(callback_ordinal=6)),bad)
 def test_rejects_retail_fields_and_wrong_lifecycle_order(self):
  malformed={"format":trace.INPUT_FORMAT,"events":[dict(event(),address=4)]}
  with self.assertRaises(ValueError): trace.sanitize_trace(malformed)
  with self.assertRaises(ValueError): clean(event(phase="camera_route",global_lifecycle="not_attempted",component_phase_one="completed",commit="not_attempted"))
if __name__=="__main__": unittest.main()
