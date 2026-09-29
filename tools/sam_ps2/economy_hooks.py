"""Sam-only economy getters; original executable bodies remain fallback paths."""
import struct
from profile_hooks import Code

def direct_getter(original, offset):
    c=Code();c.emit(0x2418001e);c.branch(5,6,24,'original');c.emit(0)
    # Preserve the original signed profile<2 guard.
    c.emit(0x28b80002);c.branch(4,24,0,'zero');c.emit(0)
    c.emit(0x24180f88,0x00b80018,0x0000c012,0x3c190053,0x8f3909fc,
           0x0338c821,0x03e00008,0x8f220000|offset)
    c.label('zero');c.emit(0x03e00008,0x00001021)
    c.label('original')
    # Entire leaf body: internal PC-relative branch and return remain valid.
    return c.bytes()+original

def current_cash(resume=0x150998):
    c=Code()
    # Save character/profile before displaced MULT instructions destroy them.
    c.emit(0x0040c021,0x0200c821,0x24030f88,0x34049b50,
           0x00430018,0x00001012,0x02040018,0x00008012)
    c.emit(0x2403001e);c.branch(5,24,3,'resume');c.emit(0x24030f88)
    # Resume's existing adds still combine V0 + S0 + original record base.
    c.emit(0x03230018,0x00001012,0x3c180053,0x8f1809fc,0x00581021,
           0x00501023,0x3c18004a,0x27186ca8,0x00581023)
    c.label('resume');c.emit((2<<26)|(resume>>2),0)
    return c.bytes()

def mutation(original, tail, pointer_reg=3):
    """Preserve original arithmetic/store tail, changing only Sam's address."""
    c=Code();c.emit(0x2418001e);c.branch(5,6,24,'original');c.emit(0)
    c.emit(0x28b80002);c.branch(4,24,0,'zero');c.emit(0)
    c.emit(0x24180f88,0x00b80018,0x0000c012,0x3c190053,0x8f3909fc,
           (25<<21)|(24<<16)|(pointer_reg<<11)|0x21,
           (2<<26)|(tail>>2),0)
    c.label('zero');c.emit(0x03e00008,0x24020f88 if pointer_reg==2 else 0x00001021)
    c.label('original')
    return c.bytes()+original
