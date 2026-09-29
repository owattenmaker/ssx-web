"""Checks that conformance reports cannot silently omit missing recovered state."""
import sys
import hashlib
import struct
import unittest
from pathlib import Path
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from compare_reference import field_differences, quaternion_error_degrees
from probe_rider_pose import animation_inputs
from native_replay import verify_inputs
from reference_replay import pad_frame

class ComparisonEvidenceTests(unittest.TestCase):
    def test_missing_angular_state_is_reported(self):
        differences=field_differences(None,dict(mode=0,spin_rate=2.,axis_blend=0.))
        self.assertEqual({x['field'] for x in differences},{'mode','spin_rate','axis_blend'})
        self.assertTrue(all(x['missing'] for x in differences))

    def test_small_drift_and_shape_mismatch_are_not_rounded_away(self):
        values=field_differences(dict(position=[1.,2.,3.00000001],normal=[0.,1.]),dict(position=[1.,2.,3.],normal=[0.,0.,1.]))
        self.assertEqual(values[0]['field'],'position[2]')
        self.assertTrue(values[1]['shape_mismatch'])

    def test_quaternion_sign_equivalence_and_invalid_zero(self):
        self.assertEqual(quaternion_error_degrees([0,0,0,1],[0,0,0,-1]),0)
        self.assertAlmostEqual(quaternion_error_degrees([0,0,0,1],[1,0,0,0]),180)
        with self.assertRaises(ValueError):quaternion_error_degrees([0,0,0,0],[0,0,0,1])

    def test_expected_bones_cannot_enter_gameplay_seed(self):
        snapshot=dict(variant_flags=16,grab_end_semantic=287,current_semantics=[438,438,246,415,438,438],character='psymon',default_root_position=[0,0,0],default_root_rotation=[0,0,-1,0],default_mirror=True,bone_mask=0xffffff,limited_bones=True,contact_cache={},body_cache={},air_state=None,pivot_bone=0,pose_state={},layers=[],scale=[1,1,1],contact={},presentation={},source='fixture',ee_sha256='a'*64,
                      bones=[dict(world_position=[999,999,999])],root_position_cm=[888,888,888])
        with patch('probe_rider_pose.extract',return_value=snapshot):seed=animation_inputs(Path('fixture'))
        self.assertEqual(set(seed),{'character','collision_enabled','layers','scale','contact','presentation','pose_state','pivot_bone','air_state','contact_cache','body_cache','default_root_position','default_root_rotation','default_mirror','bone_mask','limited_bones','current_semantics', 'grab_end_semantic','variant_flags','provenance'})
        self.assertEqual(seed['character'],'psymon')
        self.assertEqual(seed['bone_mask'],0xffffff)
        self.assertNotIn('bones',seed)
        self.assertNotIn('root_position_cm',seed)

    def test_raw_verifier_does_not_claim_original_control_semantics(self):
        accepted=dict(decoding='runtime_control_state',frames=1,baseline_prefix=dict(verified=True),initial_control_state=5,final_control_state=0,
                      segments=[dict(start=0,end=1,word0='0x0',word1='0x0')],input_stream_sha256=hashlib.sha256(bytes(8)).hexdigest())
        spec=dict(frames=1,events=[],native=dict(accepted_input=accepted))
        telemetry=dict(fps=60,input_source='original_raw_commands',records=[dict(frame=0,input_frame=-1,control_state=5),
            dict(frame=1,input_frame=0,control_state=5,ps2_pad_hex=pad_frame().hex(),accepted_command=dict(word0='0x0',word1='0x0',control_state=5,controls=dict(grabMask=1)))])
        checked=verify_inputs(spec,telemetry)
        self.assertTrue(checked['accepted_commands_identical'])
        self.assertNotIn('accepted_controls_identical',checked)
        self.assertNotEqual(checked['native_final_control_state'],checked['reference_final_control_state'])
        telemetry['records'][1]['accepted_command']['control_state']=0
        with self.assertRaisesRegex(ValueError,'different native controller'):verify_inputs(spec,telemetry)

if __name__=='__main__':unittest.main()
