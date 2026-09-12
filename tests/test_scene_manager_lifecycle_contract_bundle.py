from __future__ import annotations
import json
import pathlib
import stat
import sys
import tempfile
import unittest
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

 def test_trace_cli_uses_bounded_no_follow_owner_only_private_files(self):
  with tempfile.TemporaryDirectory() as directory:
   private=pathlib.Path(directory); source=private/"raw.json"; result=private/"sanitized.json"
   source.write_text(json.dumps({"format":trace.INPUT_FORMAT,"events":[event()]}),encoding="utf-8")
   old_argv=sys.argv
   try:
    sys.argv=["scene-trace",str(source),str(result)]
    self.assertEqual(trace.main(),0)
   finally: sys.argv=old_argv
   self.assertEqual(stat.S_IMODE(result.stat().st_mode),0o600)
   link=private/"linked.json"; link.symlink_to(source.name)
   with self.assertRaisesRegex(ValueError,"must not contain a symlink"): trace._outside_repository(link,"input")
   linked_parent=private/"linked-parent"; linked_parent.symlink_to(private, target_is_directory=True)
   with self.assertRaisesRegex(ValueError,"must not contain a symlink"):
    trace._outside_repository(linked_parent/"other.json","output")
   with self.assertRaisesRegex(ValueError,"outside the repository"):
    trace._outside_repository(pathlib.Path(__file__),"input")
   oversized=private/"oversized.json"; oversized.write_bytes(b" "*(trace.MAX_PRIVATE_RECORD_BYTES+1))
   with self.assertRaisesRegex(ValueError,"bounded regular private file"):
    trace._read_private_json_no_follow(oversized,"input")

 def test_contract_cli_refuses_links_duplicate_or_existing_output(self):
  good=clean(event()); bad=clean(event(phase="failure",callback_ordinal=5,staging="rejected",global_lifecycle="not_attempted",component_phase_one="not_attempted",camera_route="not_attempted",commit="not_attempted",outcome="failure"))
  with tempfile.TemporaryDirectory() as directory:
   private=pathlib.Path(directory); candidate=private/"candidate.json"; repeat=private/"repeat.json"; failure=private/"failure.json"; output=private/"receipt.json"
   for path,value in ((candidate,good),(repeat,good),(failure,bad)): path.write_text(json.dumps(value),encoding="utf-8")
   old_argv=sys.argv
   try:
    sys.argv=["scene-bundle",str(candidate),str(repeat),str(failure),str(output)]
    self.assertEqual(bundle.main(),0)
   finally: sys.argv=old_argv
   self.assertEqual(stat.S_IMODE(output.stat().st_mode),0o600)
   link=private/"candidate-link.json"; link.symlink_to(candidate.name)
   with self.assertRaisesRegex(ValueError,"must not contain a symlink"):
    bundle._outside_repository(link,"candidate")
   with self.assertRaisesRegex(ValueError,"outside the repository"):
    bundle._outside_repository(pathlib.Path(__file__),"candidate")
   old_argv=sys.argv
   try:
    sys.argv=["scene-bundle",str(candidate),str(candidate),str(failure),str(private/"other.json")]
    self.assertEqual(bundle.main(),1)
   finally: sys.argv=old_argv
   old_argv=sys.argv
   try:
    sys.argv=["scene-bundle",str(candidate),str(repeat),str(failure),str(output)]
    self.assertEqual(bundle.main(),1)
   finally: sys.argv=old_argv
if __name__=="__main__": unittest.main()
