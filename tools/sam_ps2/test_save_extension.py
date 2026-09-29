"""Execute real game checksum plus candidate writer/validator in Unicorn.

Uses private original ELF and real profile snapshots. No emulator/card writes.
"""
import json
import struct
from pathlib import Path
from unicorn import Uc, UC_ARCH_MIPS, UC_MODE_MIPS32, UC_MODE_LITTLE_ENDIAN
from unicorn.mips_const import *
from save_extension import *

ROOT = Path(__file__).resolve().parents[2]
elf = (ROOT / 'local/disc/SLUS_207.72').read_bytes()
phoff = struct.unpack_from('<I', elf, 28)[0]
phsize, phnum = struct.unpack_from('<HH', elf, 42)

def original(at, size):
    for i in range(phnum):
        typ, off, va, _, length, *_ = struct.unpack_from('<8I', elf, phoff + i * phsize)
        if typ == 1 and va <= at and at + size <= va + length:
            return elf[off + at - va:off + at - va + size]
    raise ValueError(hex(at))

ram = (ROOT / 'local/sam-ps2/roster/save-tests/after-fresh-load-ram.bin').read_bytes()
profile_base = struct.unpack_from('<I', ram, 0x5309fc)[0]
u = Uc(UC_ARCH_MIPS, UC_MODE_MIPS32 | UC_MODE_LITTLE_ENDIAN)
u.mem_map(0x100000, 0x1000000)
u.mem_write(0x451018, original(0x451018, 512))
crc_code = bytearray(original(0x3e62d0, 0x58))
# The only unsupported instruction here is EE DADDU a3,a0,zero;
# pointers are 32-bit, so standard ADDU has the same effective result.
assert struct.unpack_from('<I', crc_code)[0] == 0x0080382d
struct.pack_into('<I', crc_code, 0, 0x00803821)
u.mem_write(0x3e62d0, bytes(crc_code))
u.mem_write(0x900000, writer())
u.mem_write(0x910000, validator())
u.mem_write(0x5309fc, struct.pack('<I', 0xa00000))
profiles = []
for index in range(3):
    record = bytearray(ram[profile_base + index * RECORD_SIZE:profile_base + (index + 1) * RECORD_SIZE])
    record[0xbdf] = 7 + index
    profiles.append(bytes(record))
u.mem_write(0xa00000, b''.join(profiles))
u.mem_write(0x4a1220, struct.pack('<I', 0xb00000))
legacy = bytearray(LEGACY_SIZE)
legacy[:12] = ram[0x535b20:0x535b2c]
legacy[12:12 + 0x9b50] = ram[0x4a6ca8:0x4b07f8]
legacy[0x9b5c] = 3  # Save while Mac is selected; Sam must still be included.
seal(legacy, LEGACY_CHECKSUM)
assert classify(legacy) == 1

def run(at, count=2000000):
    u.reg_write(UC_MIPS_REG_SP, 0xc10000)
    u.reg_write(UC_MIPS_REG_RA, 0x100000)
    u.reg_write(UC_MIPS_REG_GP, 0x4a30f0)
    u.reg_write(UC_MIPS_REG_S1, 0x13579)
    u.emu_start(at, 0x100000, count=count)
    assert u.reg_read(UC_MIPS_REG_PC) == 0x100000
    assert u.reg_read(UC_MIPS_REG_SP) == 0xc10000
    assert u.reg_read(UC_MIPS_REG_S1) == 0x13579

for index in range(3):
    u.mem_write(0xb00000, bytes(legacy) + bytes([0xa5]) * (SAVE_SIZE - LEGACY_SIZE + 16))
    u.reg_write(UC_MIPS_REG_S2, index)
    run(0x900000)
    assert u.reg_read(UC_MIPS_REG_S2) == index
    assert u.reg_read(UC_MIPS_REG_S0) == 0xb00000 + CHECKSUM_OFFSET
    # The game's unchanged epilogue stores the returned four-byte checksum.
    u.mem_write(0xb00000 + CHECKSUM_OFFSET, struct.pack('<I', u.reg_read(UC_MIPS_REG_V0)))
    encoded = encode(legacy, profiles[index])
    assert bytes(u.mem_read(0xb00000, SAVE_SIZE)) == encoded
    assert bytes(u.mem_read(0xb00000 + SAVE_SIZE, 16)) == bytes([0xa5]) * 16
    assert bytes(u.mem_read(0xa00000, 3 * RECORD_SIZE)) == b''.join(profiles)

cases = [('legacy', bytes(legacy), 1), ('extended', encoded, 2)]
for length in [0, 12, LEGACY_SIZE - 1, LEGACY_SIZE, LEGACY_SIZE + 1, SAVE_SIZE - 1]:
    cases.append((f'truncated-{length}', encoded[:length], 0))
cases.append(('extra-byte', encoded + b'\0', 0))
for at in [0, 12, MARKER_OFFSET, LEGACY_CHECKSUM, HEADER_OFFSET,
           HEADER_OFFSET + 4, HEADER_OFFSET + 8, HEADER_OFFSET + 12,
           LEGACY_SIZE, RECORD_OFFSET + 0xbdf, CHECKSUM_OFFSET]:
    bad = bytearray(encoded);bad[at] ^= 1
    cases.append((f'corrupt-{at:x}', bytes(bad), 0))
# Correct checksums must not hide unsupported schemas or runtime pointers.
for at in [MARKER_OFFSET, HEADER_OFFSET, HEADER_OFFSET + 4, HEADER_OFFSET + 8,
           HEADER_OFFSET + 12, LEGACY_SIZE, RECORD_OFFSET + 0x288]:
    bad = bytearray(encoded);bad[at] ^= 1
    seal(bad, LEGACY_CHECKSUM);seal(bad, CHECKSUM_OFFSET)
    cases.append((f'invalid-schema-{at:x}', bytes(bad), 0))
for name, data, expected in cases:
    assert classify(data) == expected, name
    # Preserve a valid stale tail deliberately: actual read size must prevail.
    u.mem_write(0xb00000, encoded + bytes(16))
    u.mem_write(0xb00000, data)
    before = bytes(u.mem_read(0xb00000, SAVE_SIZE + 16))
    u.reg_write(UC_MIPS_REG_A0, 0xb00000)
    u.reg_write(UC_MIPS_REG_A1, len(data))
    run(0x910000)
    assert u.reg_read(UC_MIPS_REG_V0) == expected, name
    assert bytes(u.mem_read(0xb00000, SAVE_SIZE + 16)) == before, name
print(f'3 independent profile writes and {len(cases)} validation cases passed against the original game checksum code.')
report = dict(profile_writes=3, validation_cases=len(cases), checksum='CRC-16/IBM seed FBEA stored as uint32',
              machine_code_tested=True, live_save_roundtrip=False,
              limitations='Writer/validator candidates only; not installed in any game build.')
(ROOT / 'local/sam-ps2/roster/save-tests/extension-unit-tests.json').write_text(json.dumps(report, indent=2) + '\n')

# Execute the loader callbacks with real normalization code. Unicorn lacks
# EE LQ; reduce only epilogue restores to scalar LW for this MIPS32 harness.
def scalar_epilogue(code):
    words = list(struct.unpack('<%dI' % (len(code) // 4), code))
    return struct.pack('<%dI' % len(words), *[(w & 0x03ffffff) | (35 << 26) if w >> 26 == 30 else w for w in words])
u.mem_write(0x920000, scalar_epilogue(loader_check()))
u.mem_write(0x930000, scalar_epilogue(loader_finish()))
u.mem_write(0x530a34, struct.pack('<I', 0x910000))
u.mem_write(0x530a38, struct.pack('<I', 0xa10000))
u.mem_write(0x535538, struct.pack('<I', 0xa20000))
normalizer = bytearray(original(0x1515b8, 0x48))
assert struct.unpack_from('<I', normalizer, 12)[0] == 0x0040282d
struct.pack_into('<I', normalizer, 12, 0x00402821)
u.mem_write(0x1515b8, bytes(normalizer))
default = bytearray(profiles[0]);default[0xbdf:0xbe6] = bytes([5] * 7)
struct.pack_into('<I', default, 0x288, 0)
u.mem_write(0xa10000, bytes(default))

def normalized(record, live_lookup):
    result = bytearray(record)
    struct.pack_into('<I', result, 0x288, live_lookup)
    count = struct.unpack_from('<I', result, 0x28c)[0]
    for i in range(count):
        at = 0x292 + 4 * i
        flag = struct.unpack_from('<H', result, at)[0]
        struct.pack_into('<H', result, at, (flag | 16) if flag & 4 else (flag & ~16))
    return bytes(result)

loader_cases = 0
for name, data, expected in cases:
    for target in range(3):
        u.mem_write(0xb00000, encoded + bytes(16));u.mem_write(0xb00000, data)
        u.mem_write(0x530a30, struct.pack('<I', len(data)))
        u.mem_write(0xa00000, b''.join(profiles))
        u.mem_write(0xa20000, bytes([0xa5]) * 420)
        u.reg_write(UC_MIPS_REG_SP, 0xc10000)
        u.reg_write(UC_MIPS_REG_RA, 0x100000)
        u.reg_write(UC_MIPS_REG_S1, 0xb00000)
        u.reg_write(UC_MIPS_REG_S2, target)
        # Expected failure branches to the common original epilogue.
        end = 0x100000 if expected else 0x15303c
        u.emu_start(0x920000, end, count=2000000)
        assert u.reg_read(UC_MIPS_REG_PC) == end, (name, target)
        assert bytes(u.mem_read(0xa00000, 3 * RECORD_SIZE)) == b''.join(profiles)
        assert u.reg_read(UC_MIPS_REG_SP) == 0xc10000
        if not expected:
            assert u.reg_read(UC_MIPS_REG_V0) == 0
            continue
        assert u.reg_read(UC_MIPS_REG_V0) == struct.unpack_from('<I', data, LEGACY_CHECKSUM)[0]
        # Original loader changes only the ten original riders; successful
        # callback receives its preserved buffer/index in the spare stack slots.
        u.emu_start(0x930000, 0x15303c, count=100000)
        assert u.reg_read(UC_MIPS_REG_PC) == 0x15303c
        assert u.reg_read(UC_MIPS_REG_SP) == 0xc10000
        assert u.reg_read(UC_MIPS_REG_V0) == 1
        for index in range(3):
            wanted = profiles[index]
            if index == target:
                source = data[RECORD_OFFSET:CHECKSUM_OFFSET] if expected == 2 else default
                lookup = struct.unpack_from('<I', profiles[index], 0x288)[0]
                wanted = normalized(source, lookup)
            assert bytes(u.mem_read(0xa00000 + index * RECORD_SIZE, RECORD_SIZE)) == wanted, (name, target, index)
        cache = bytearray([0xa5] * 420)
        source = data[RECORD_OFFSET:CHECKSUM_OFFSET] if expected == 2 else default
        cache[210 + target * 70:217 + target * 70] = source[0xbdf:0xbe6]
        assert bytes(u.mem_read(0xa20000, 420)) == cache
        loader_cases += 1
print(f'Loader rejects invalid saves before mutation; {loader_cases} legacy/extended target-profile restores preserve lookup ownership and other profiles.')
report.update(loader_validation_cases=len(cases) * 3, loader_restore_cases=loader_cases,
              limitations='Original ten-rider loader body not executed here; live card roundtrip still required.')
(ROOT / 'local/sam-ps2/roster/save-tests/extension-unit-tests.json').write_text(json.dumps(report, indent=2) + '\n')
