import hashlib
import struct
import sys
import unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from reference_input import accepted_delta, raw_accepted_delta


def report(runs,control=0,held=False):
    records=[];frame=0
    for duration,w0,w1 in runs:
        records.append(dict(start=frame,end=frame+duration,duration=duration,word0=f'0x{w0:08x}',word1=f'0x{w1:08x}'))
        frame+=duration
    return dict(control_state=control,actions={'JumpHeld':held},accepted_commands=dict(frames=frame,records=records))


class AcceptedInputTests(unittest.TestCase):
    def test_extended_prefix_and_exact_hash(self):
        before=report([(100,0,0)])
        after=report([(101,0,0),(1,0xc000,0),(2,0x2000,0),(1,0,0),(2,0xff0000,0)],5)
        result=accepted_delta(before,after)
        expected=[(0,0),(0xc000,0),(0x2000,0),(0x2000,0),(0,0),(0xff0000,0),(0xff0000,0)]
        self.assertEqual(result['frames'],7)
        self.assertEqual(result['input_stream_sha256'],hashlib.sha256(b''.join(struct.pack('<2I',*words) for words in expected)).hexdigest())
        self.assertEqual([s['control_state'] for s in result['segments']],[0,0,2,2,5])
        self.assertTrue(result['segments'][1]['controls']['jumpPressed'])
        self.assertFalse(result['segments'][3]['controls']['jumpHeld'])
        self.assertEqual(result['baseline_prefix']['sha256'],hashlib.sha256(bytes(800)).hexdigest())

    def test_prefix_divergence_is_rejected(self):
        with self.assertRaisesRegex(ValueError,'diverges'):
            accepted_delta(report([(100,0,0)]),report([(99,0,0),(2,0xc000,0)],2))
        with self.assertRaisesRegex(ValueError,'shorter'):
            accepted_delta(report([(100,0,0)]),report([(99,0,0)]))

    def test_air_no_grab_sentinel_is_not_zero(self):
        before=report([(1,0xff0000,0)],5)
        selected=accepted_delta(before,report([(1,0xff0000,0),(1,0,0)],5))
        self.assertEqual(selected['segments'][0]['controls']['grabMask'],1)
        with self.assertRaisesRegex(ValueError,'Invalid original airborne grab'):
            accepted_delta(before,report([(1,0xff0000,0),(1,0xf0000,0)],5))
        result=accepted_delta(before,report([(3,0xff0000,0)],5))
        self.assertEqual(result['frames'],2)
        self.assertFalse(result['segments'][0]['controls']['jumpHeld'])

    def test_airborne_axes_and_all_grab_indices(self):
        masks=(1,2,4,8,3,5,9,6,10,12,7,11,13,14,15)
        for grab,mask in enumerate(masks):
            word0=(grab<<16)|(33<<24)|0x4000
            word1=31|(33<<6)|(31<<12)|(3<<18)
            result=accepted_delta(report([(1,0xff0000,0)],5),report([(1,0xff0000,0),(1,word0,word1)],5))
            controls=result['segments'][0]['controls']
            self.assertEqual(controls['grabMask'],mask)
            self.assertEqual(controls['spin'],-0.9999999403953552)
            self.assertEqual(controls['flip'],0.9999999403953552)
            self.assertEqual(controls['airAdjustLR'],0.9999999403953552)
            self.assertEqual(controls['airAdjustFB'],-0.9999999403953552)
            self.assertEqual(controls['boardPress'],-1.)
            self.assertTrue(controls['boostHeld'])

    def test_unknown_actions_and_state_fail_closed(self):
        with self.assertRaisesRegex(ValueError,'attack/handplant'):
            accepted_delta(report([(1,0,0)]),report([(1,0,0),(1,0x40000,0)]))
        with self.assertRaisesRegex(ValueError,'Unsupported baseline'):
            accepted_delta(report([(1,0,0)],9),report([(2,0,0)],9))

    def test_signed_six_bit_axes_use_original_reciprocal(self):
        result=accepted_delta(report([(1,0,0)]),report([(1,0,0),(1,33<<20,31)]))
        controls=result['segments'][0]['controls']
        self.assertEqual(controls['turn'],-0.9999999403953552)
        self.assertEqual(controls['brake'],0.9999999403953552)

    def test_raw_replay_preserves_ambiguous_landing_words(self):
        before=report([(100,0xff0000,0)],5)
        after=report([(105,0xff0000,0),(20,0,0)],0)
        with self.assertRaisesRegex(ValueError,'Unsupported control transition'):accepted_delta(before,after)
        raw=raw_accepted_delta(before,after)
        self.assertEqual(raw['frames'],25)
        self.assertEqual(raw['decoding'],'runtime_control_state')
        self.assertEqual([(r['start'],r['end'],r['word0']) for r in raw['segments']],[(0,5,'0x00ff0000'),(5,25,'0x00000000')])
        self.assertTrue(all('controls' not in r and 'control_state' not in r for r in raw['segments']))
        self.assertEqual(raw['final_control_state'],0)
        expected=struct.pack('<II',0xff0000,0)*5+bytes(8*20)
        self.assertEqual(raw['input_stream_sha256'],hashlib.sha256(expected).hexdigest())

    def test_raw_replay_rejects_divergent_prefix_and_malformed_coverage(self):
        with self.assertRaisesRegex(ValueError,'diverges'):
            raw_accepted_delta(report([(100,0,0)]),report([(99,0,0),(2,0xc000,0)],2))
        damaged=report([(101,0,0)])
        damaged['accepted_commands']['records'][0]['start']=1
        with self.assertRaisesRegex(ValueError,'coverage'):raw_accepted_delta(report([(100,0,0)]),damaged)


if __name__=='__main__':unittest.main()
