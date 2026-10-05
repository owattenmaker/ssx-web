"""engine/ps2_fpu.hpp's multiplier in Python (for the matcher's order checks; tests: tools/ps2-float/test_ps2fpu_mul.py)."""

MASK32 = 0xFFFFFFFF


def _window(b, digit):
    return ((b >> (digit * 2 - 1)) if digit else (b << 1)) & 7


def _partial(a, b, digit):
    window = _window(b, digit)
    partial = (a << (digit * 2)) & MASK32
    if window in (3, 4):
        partial = (partial + partial) & MASK32
    if 4 <= window <= 6:
        partial ^= (-(1 << (digit * 2))) & MASK32
    return partial if 1 <= window <= 6 else 0


def _correction(b, digit):
    window = _window(b, digit)
    return (1 << (digit * 2)) if 4 <= window <= 6 else 0


def _csa(a, b, c):
    partial = a ^ b
    carry = (((partial & c) | (a & b)) << 1) & MASK32
    return partial ^ c, carry


def _array(a, b):
    exact = a * b
    p = [_partial(a, b, k) for k in range(8)]
    sum0, carry0 = _csa(p[1], p[2], p[3])
    sum1, carry1 = _csa(p[4] & ~0x7FF & MASK32, p[5] & ~0xFFF & MASK32, p[6])
    high1 = carry1 | _correction(b, 6) | (p[5] & 0x800)
    row7 = p[7] | (((p[5] & 0x400) + _correction(b, 5)) & MASK32)
    sum2, carry2 = _csa(p[0], sum0, carry0)
    sum3, carry3 = _csa(row7, sum1, high1)
    sum4, carry4 = _csa(carry2, sum3, carry3)
    sum5, carry5 = _csa(sum2, sum4, carry4)
    low = sum5 & ~0x7FFF & MASK32
    high = ((carry5 + _correction(b, 7)) & MASK32) & ~0x7FFF & MASK32
    return exact - ((((low + high) & MASK32) ^ (exact & MASK32)) & 0x8000)


def mul_bits(fs, ft):
    sign = (fs ^ ft) & 0x80000000
    ef, et = (fs >> 23) & 0xFF, (ft >> 23) & 0xFF
    if ef == 0 or et == 0:
        return sign
    a, b = 0x800000 | (fs & 0x7FFFFF), 0x800000 | (ft & 0x7FFFFF)
    product = a * b
    carry = product >> 47
    shift = 24 if carry else 23
    significand = product >> shift
    tail = product & ((1 << shift) - 1)
    exponent = ef + et - 127 + carry
    if exponent >= 256:
        return sign | 0x7FFFFFFF
    if exponent <= 0:
        return sign
    word = sign | (exponent << 23) | (significand & 0x7FFFFF)
    if exponent == 255 and significand == 0xFFFFFF and tail:
        return word
    if (word & 0x7FFFFFFF) == 0x800000:
        return word
    if tail < 0x8000 and (product >> shift) != (_array(a, b) >> shift):
        return word - 1
    return word
