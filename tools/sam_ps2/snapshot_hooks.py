"""Route current-rider snapshot copies to Sam's owned profile records.

This fixes the two active-profile copies in both snapshot directions. The
snapshot's all-rider profile-2 block is a separate extension still to audit.
"""
import struct
from profile_hooks import Code
# site, profile, pointer cell, original four instructions
SITES = [
    (0x14a334, 0, 0x530a44, (0x24030f88, 0x80420011, 0x00431818, 0x00641821)),
    (0x14a408, 1, 0x530a48, (0x3c03004b, 0x00442018, 0x246307f8, 0x00831821)),
    (0x14a7dc, 0, 0x530a4c, (0x26446ca8, 0x0200282d, 0x00431818, 0x00641821)),
    (0x14a8b0, 1, 0x530a50, (0x3c03004b, 0x00442018, 0x246307f8, 0x00831821)),
]


def pointer_hook(site, profile, words):
    c = Code()
    for word in words:
        if word & 0xfc00003f == 0x18:
            # EE MULT writes rd as well as HI/LO. Preserve both outputs.
            dest = (word >> 11) & 31
            c.emit(word & ~(31 << 11), (dest << 11) | 0x12)
        else:
            c.emit(word)
    # V0 still contains the character ID after all four displaced instructions.
    c.emit(0x2418001e)
    c.branch(5, 2, 24, 'resume');c.emit(0)
    c.emit(0x3c190053, 0x8f2309fc)
    if profile:
        c.emit(0x24630000 | (profile * 0xf88))
    c.label('resume')
    # Direct tail jump preserves the leaf writer's original return address.
    c.emit((2 << 26) | ((site + 16) >> 2), 0)
    return c.bytes()


def patch_words(cell):
    return struct.pack('<4I', 0x3c190053, 0x8f390000 | (cell & 65535), 0x03200008, 0)
