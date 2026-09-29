"""Execute all four exact snapshot patch sites and check routing/preservation."""
import struct
from unicorn import Uc, UC_ARCH_MIPS, UC_MODE_MIPS32, UC_MODE_LITTLE_ENDIAN
from unicorn.mips_const import *
from snapshot_hooks import SITES, pointer_hook, patch_words

cases = 0
for site, profile, cell, words in SITES:
    for character in range(32):
        u = Uc(UC_ARCH_MIPS, UC_MODE_MIPS32 | UC_MODE_LITTLE_ENDIAN)
        for base in (0x140000, 0x530000, 0x900000):
            u.mem_map(base, 0x10000)
        code = pointer_hook(site, profile, words)
        # Unicorn MIPS32 cannot execute DADDU. The original EE instruction is
        # retained in the shipped hook; here use ADDU for the 32-bit pointer.
        code = code.replace(struct.pack('<I', 0x0200282d), struct.pack('<I', 0x02002821))
        u.mem_write(site, patch_words(cell));u.mem_write(0x900000, code)
        u.mem_write(cell, struct.pack('<I', 0x900000))
        u.mem_write(0x5309fc, struct.pack('<I', 0xa00000))
        u.mem_write(0x535b31, bytes([character]))
        for reg in range(1, 32):u.reg_write(UC_MIPS_REG_0 + reg, 0x123400 + reg)
        u.reg_write(UC_MIPS_REG_V0, 0x535b20 if site == 0x14a334 else character)
        u.reg_write(UC_MIPS_REG_V1, 0xf88)
        u.reg_write(UC_MIPS_REG_A0, 0x4a6ca8 if profile == 0 else 0xf88)
        u.reg_write(UC_MIPS_REG_S2, 0x4a0000)
        before = {r:u.reg_read(UC_MIPS_REG_0+r) for r in range(1,32)}
        u.emu_start(site, site + 16, count=100)
        assert u.reg_read(UC_MIPS_REG_PC) == site + 16
        expected = 0xa00000 + profile * 0xf88 if character == 30 else 0x4a6ca8 + profile * 0x9b50 + character * 0xf88
        assert u.reg_read(UC_MIPS_REG_V1) == expected, (hex(site), character)
        assert u.reg_read(UC_MIPS_REG_V0) == character
        assert u.reg_read(UC_MIPS_REG_LO) == character * 0xf88
        assert u.reg_read(UC_MIPS_REG_HI) == 0
        changed = {2,3,24,25}
        if profile:changed.add(4);assert u.reg_read(UC_MIPS_REG_A0) == character * 0xf88
        if site == 0x14a7dc:
            changed.update((4,5))
            assert u.reg_read(UC_MIPS_REG_A0) == 0x4a6ca8
            assert u.reg_read(UC_MIPS_REG_A1) == before[16]
        for reg in set(range(1,32)) - changed:
            assert u.reg_read(UC_MIPS_REG_0 + reg) == before[reg], (hex(site), reg)
        cases += 1
print(f'{cases} snapshot source/destination routing cases passed; original rider addresses and live registers preserved.')
