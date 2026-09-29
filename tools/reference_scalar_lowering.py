"""Compatibility entry for the shared opcode-driven development FP oracle."""
from original_fp_oracle import write_scalar_fp_oracle

def scalar_oracle_copy(source,target):
    return write_scalar_fp_oracle(source,target)
