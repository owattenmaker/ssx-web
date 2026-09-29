import sys
import unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from original_fp_oracle import correct_scalar_fp

class ScalarOracleTests(unittest.TestCase):
    def test_sqrt_uses_ft_and_nearest(self):
        source='// 0x31beec: 0x46050044 c1\nctx->f[1] = FPU_SQRT_S(ctx->f[0]);\n'
        fixed,audit=correct_scalar_fp(source)
        self.assertIn('ctx->f[1] = originalOracleScalarSqrt(ctx->f[5])',fixed)
        self.assertEqual(audit[0]['ft'],5)
        self.assertIn('FE_TONEAREST',fixed)

    def test_division_changes_only_scalar_operation(self):
        source=('// 0x1000: 0x46020803 div.s\nctx->f[0] = ctx->f[1] / ctx->f[2];\n'
                '// 0x1004: 0x4a6303bc vdiv\nVU_DIV(vf0, vf3);\n'
                '// 0x1008: 0x46020816 rsqrt.s\nSCALAR_RSQRT(1,2);\n')
        fixed,audit=correct_scalar_fp(source)
        self.assertIn('originalOracleScalarDivide(ctx->f[1],ctx->f[2])',fixed)
        self.assertIn('VU_DIV(vf0, vf3);',fixed)
        self.assertIn('SCALAR_RSQRT(1,2);',fixed)
        self.assertEqual(len(audit),1)

    def test_mismatched_generated_statement_is_rejected(self):
        with self.assertRaisesRegex(ValueError,'Expected one'):
            correct_scalar_fp('// 0x1000: 0x46020803 div.s\nctx->f[0] = ctx->f[4] / ctx->f[2];')
        once,_=correct_scalar_fp('// 0x1000: 0x46020803 div.s\nctx->f[0] = ctx->f[1] / ctx->f[2];')
        with self.assertRaisesRegex(ValueError,'already corrected'):correct_scalar_fp(once)

if __name__=='__main__':unittest.main()
