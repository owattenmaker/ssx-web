"""Version-one Sam save format and relocatable PS2 serialization routines.

Private candidate only. These routines are not enabled in the playtest ISO.
The original checksum is CRC-16/IBM (seed FBEA), stored in a uint32 field.
"""
import struct
from profile_hooks import Code, RECORD_SIZE

LEGACY_SIZE = 0x9B61
LEGACY_CHECKSUM = LEGACY_SIZE - 4
MARKER_OFFSET = 12 + 0x288  # discarded original rider lookup pointer
MAGIC = int.from_bytes(b'SAM3', 'little')
HEADER_OFFSET = 0x9B70
RECORD_OFFSET = 0x9B80
CHECKSUM_OFFSET = RECORD_OFFSET + RECORD_SIZE
SAVE_SIZE = CHECKSUM_OFFSET + 4
HEADER = struct.pack('<4I', MAGIC, 1, RECORD_SIZE, 30)


def checksum(data):
    value = 0xFBEA
    for byte in data:
        value ^= byte
        for _ in range(8):
            value = (value >> 1) ^ (0xA001 if value & 1 else 0)
    return value


def seal(data, at):
    struct.pack_into('<I', data, at, checksum(data[:at]))


def encode(legacy, sam):
    if len(legacy) != LEGACY_SIZE or len(sam) != RECORD_SIZE:
        raise ValueError('Incorrect profile size')
    if struct.unpack_from('<I', legacy, LEGACY_CHECKSUM)[0] != checksum(legacy[:LEGACY_CHECKSUM]):
        raise ValueError('Invalid original profile checksum')
    out = bytearray(SAVE_SIZE)
    out[:LEGACY_SIZE] = legacy
    struct.pack_into('<I', out, MARKER_OFFSET, MAGIC)
    seal(out, LEGACY_CHECKSUM)
    out[HEADER_OFFSET:RECORD_OFFSET] = HEADER
    out[RECORD_OFFSET:CHECKSUM_OFFSET] = sam
    struct.pack_into('<I', out, RECORD_OFFSET + 0x288, 0)
    seal(out, CHECKSUM_OFFSET)
    return bytes(out)


def classify(data):
    """0 rejects, 1 legacy, 2 extended; validate before changing live records."""
    if len(data) not in (LEGACY_SIZE, SAVE_SIZE):
        return 0
    if struct.unpack_from('<I', data, LEGACY_CHECKSUM)[0] != checksum(data[:LEGACY_CHECKSUM]):
        return 0
    marked = struct.unpack_from('<I', data, MARKER_OFFSET)[0] == MAGIC
    if len(data) == LEGACY_SIZE:
        return 0 if marked else 1
    if not marked or data[HEADER_OFFSET:RECORD_OFFSET] != HEADER:
        return 0
    if any(data[LEGACY_SIZE:HEADER_OFFSET]):
        return 0
    if struct.unpack_from('<I', data, RECORD_OFFSET + 0x288)[0] != 0:
        return 0
    return 2 if struct.unpack_from('<I', data, CHECKSUM_OFFSET)[0] == checksum(data[:CHECKSUM_OFFSET]) else 0


# Small explicit assembler helpers. Standard 32-bit operations also run on EE.
ZERO, V0, V1, A0, A1, A2, T0, T1, T2, T3, S0, S2, SP, RA = 0, 2, 3, 4, 5, 6, 8, 9, 10, 11, 16, 18, 29, 31

def imm(c, op, rt, rs, value):
    c.emit((op << 26) | (rs << 21) | (rt << 16) | (value & 0xffff))

def constant(c, reg, value):
    if value <= 65535:
        imm(c, 13, reg, ZERO, value)
    else:
        imm(c, 15, reg, ZERO, value >> 16)
        imm(c, 13, reg, reg, value)

def move(c, dest, source):
    c.emit((source << 21) | (dest << 11) | 0x21)

def add(c, dest, left, right):
    c.emit((left << 21) | (right << 16) | (dest << 11) | 0x21)

def call_crc(c):
    c.emit((3 << 26) | (0x3e62d0 >> 2))
    constant(c, A2, 0xfbea)

def load_buffer(c, reg=A0):
    imm(c, 35, reg, SP, 16)

def offset(c, dest, base, amount):
    constant(c, dest, amount)
    add(c, dest, base, dest)


def writer():
    """Hook 152B64..70. S2 source profile; returns V0 CRC and S0 CRC address.

    Original writer has already allocated SAVE_SIZE and copied the ten riders.
    Original epilogue stores returned checksum and restores caller registers.
    """
    c = Code()
    imm(c, 9, SP, SP, -32)
    imm(c, 43, RA, SP, 20)
    imm(c, 35, A0, 28, -0x1ed0)
    imm(c, 43, A0, SP, 16)
    constant(c, T0, MAGIC)
    imm(c, 43, T0, A0, MARKER_OFFSET)
    constant(c, A1, LEGACY_CHECKSUM)
    call_crc(c)
    load_buffer(c)
    offset(c, T0, A0, LEGACY_CHECKSUM)
    # Legacy checksum is unaligned; store all four bytes explicitly.
    for i in range(4):
        imm(c, 40, V0, T0, i)
        c.emit((V0 << 16) | (V0 << 11) | (8 << 6) | 2)
    for i in range(4, HEADER_OFFSET - LEGACY_CHECKSUM):
        imm(c, 40, ZERO, T0, i)
    offset(c, T0, A0, HEADER_OFFSET)
    for i, value in enumerate(struct.unpack('<4I', HEADER)):
        constant(c, T1, value)
        imm(c, 43, T1, T0, i * 4)
    imm(c, 9, T0, T0, 16)
    constant(c, T1, 0x5309fc)
    imm(c, 35, T1, T1, 0)
    constant(c, T2, RECORD_SIZE)
    c.emit((S2 << 21) | (T2 << 16) | 0x18, (T2 << 11) | 0x12)
    add(c, T1, T1, T2)
    constant(c, T2, RECORD_SIZE // 4)
    c.label('copy')
    imm(c, 35, T3, T1, 0)
    imm(c, 43, T3, T0, 0)
    imm(c, 9, T1, T1, 4)
    imm(c, 9, T2, T2, -1)
    c.branch(5, T2, ZERO, 'copy')
    imm(c, 9, T0, T0, 4)
    move(c, S0, T0)
    # Never persist an owned runtime lookup pointer.
    imm(c, 43, ZERO, S0, 0x288 - RECORD_SIZE)
    load_buffer(c)
    constant(c, A1, CHECKSUM_OFFSET)
    call_crc(c)
    imm(c, 35, RA, SP, 20)
    c.emit(0x03e00008)
    imm(c, 9, SP, SP, 32)
    return c.bytes()


def validator():
    """A0 buffer, A1 actual length; V0 classify result, no buffer mutations."""
    c = Code()
    imm(c, 9, SP, SP, -32)
    imm(c, 43, RA, SP, 24)
    imm(c, 43, A0, SP, 16)
    imm(c, 43, A1, SP, 20)
    constant(c, T0, LEGACY_SIZE)
    c.branch(4, A1, T0, 'legacy_crc');c.emit(0)
    constant(c, T0, SAVE_SIZE)
    c.branch(5, A1, T0, 'invalid');c.emit(0)
    c.label('legacy_crc')
    constant(c, A1, LEGACY_CHECKSUM)
    call_crc(c)
    load_buffer(c)
    offset(c, T0, A0, LEGACY_CHECKSUM)
    imm(c, 34, T1, T0, 3);imm(c, 38, T1, T0, 0)
    c.branch(5, V0, T1, 'invalid');c.emit(0)
    imm(c, 35, T0, SP, 20)
    constant(c, T1, LEGACY_SIZE)
    imm(c, 35, T2, A0, MARKER_OFFSET)
    constant(c, T3, MAGIC)
    c.branch(5, T0, T1, 'extended');c.emit(0)
    c.branch(4, T2, T3, 'invalid');c.emit(0)
    constant(c, V0, 1)
    c.branch(4, ZERO, ZERO, 'done');c.emit(0)
    c.label('extended')
    c.branch(5, T2, T3, 'invalid');c.emit(0)
    offset(c, T0, A0, LEGACY_SIZE)
    for i in range(HEADER_OFFSET - LEGACY_SIZE):
        imm(c, 36, T1, T0, i)
        c.branch(5, T1, ZERO, 'invalid');c.emit(0)
    offset(c, T0, A0, HEADER_OFFSET)
    for i, value in enumerate(struct.unpack('<4I', HEADER)):
        imm(c, 35, T1, T0, i * 4)
        constant(c, T2, value)
        c.branch(5, T1, T2, 'invalid');c.emit(0)
    imm(c, 35, T1, T0, 16 + 0x288)
    c.branch(5, T1, ZERO, 'invalid');c.emit(0)
    constant(c, A1, CHECKSUM_OFFSET)
    call_crc(c)
    load_buffer(c)
    offset(c, T0, A0, CHECKSUM_OFFSET)
    imm(c, 35, T1, T0, 0)
    c.branch(5, V0, T1, 'invalid');c.emit(0)
    constant(c, V0, 2)
    c.branch(4, ZERO, ZERO, 'done');c.emit(0)
    c.label('invalid');move(c, V0, ZERO)
    c.label('done')
    imm(c, 35, RA, SP, 24)
    c.emit(0x03e00008)
    imm(c, 9, SP, SP, 32)
    return c.bytes()


def loader_check():
    """Replace 152E08..14, validate before the original loader mutates anything.

    Original stack 28/2C are unused (ten lookup pointers occupy 00..27).
    Actual byte count is captured by the profile read completion hook.
    """
    c = Code()
    imm(c, 43, 17, SP, 0x28)  # original buffer, before S1 advances
    imm(c, 43, S2, SP, 0x2c)
    imm(c, 9, SP, SP, -32)
    imm(c, 43, RA, SP, 16)
    move(c, A0, 17)
    constant(c, T0, 0x530a30)
    imm(c, 35, A1, T0, 0)
    imm(c, 35, 25, T0, 4)  # validator pointer
    c.emit(0x0320f809, 0)
    c.branch(4, V0, ZERO, 'invalid');c.emit(0)
    # The saved inventory must fit this version's live lookup mapping.
    constant(c, T0, 2)
    c.branch(5, V0, T0, 'valid');c.emit(0)
    constant(c, T0, 0x5309fc);imm(c, 35, T0, T0, 0)
    constant(c, T1, RECORD_SIZE)
    c.emit((S2 << 21) | (T1 << 16) | 0x18, (T1 << 11) | 0x12)
    add(c, T0, T0, T1)
    imm(c, 35, T1, T0, 0x28c)
    offset(c, T0, 17, RECORD_OFFSET)
    imm(c, 35, T2, T0, 0x28c)
    c.branch(5, T1, T2, 'invalid');c.emit(0)
    c.label('valid')
    offset(c, T0, 17, LEGACY_CHECKSUM)
    imm(c, 34, V0, T0, 3);imm(c, 38, V0, T0, 0)
    imm(c, 35, RA, SP, 16)
    c.emit(0x03e00008);imm(c, 9, SP, SP, 32)
    c.label('invalid')
    # Skip the original comparison entirely: even a forged FFFFFFFF stored
    # checksum cannot convert a validation failure into a successful load.
    imm(c, 9, SP, SP, 32)
    move(c, V0, ZERO)
    c.emit(0x7bb00080, 0x7bb10070, 0x7bb20060,
           (2 << 26) | (0x15303c >> 2), 0)
    return c.bytes()


def loader_finish():
    """At original successful return, restore only the selected Sam profile."""
    c = Code()
    imm(c, 35, T0, SP, 0x28)
    imm(c, 35, T1, SP, 0x2c)
    imm(c, 9, SP, SP, -32)
    imm(c, 43, T1, SP, 20)
    constant(c, T2, RECORD_SIZE)
    c.emit((T1 << 21) | (T2 << 16) | 0x18, (T2 << 11) | 0x12)
    constant(c, T1, 0x5309fc);imm(c, 35, T1, T1, 0)
    add(c, T1, T1, T2)
    imm(c, 43, T1, SP, 16)
    imm(c, 35, T2, T1, 0x288)
    imm(c, 43, T2, SP, 24)
    imm(c, 35, T2, T0, MARKER_OFFSET)
    constant(c, T3, MAGIC)
    c.branch(4, T2, T3, 'extended');c.emit(0)
    constant(c, T0, 0x530a38);imm(c, 35, T0, T0, 0)
    c.branch(4, ZERO, ZERO, 'copy_setup');c.emit(0)
    c.label('extended');offset(c, T2, T0, RECORD_OFFSET);move(c, T0, T2)
    c.label('copy_setup');constant(c, T2, RECORD_SIZE // 4)
    c.label('copy')
    imm(c, 35, T3, T0, 0);imm(c, 43, T3, T1, 0)
    imm(c, 9, T0, T0, 4);imm(c, 9, T2, T2, -1)
    c.branch(5, T2, ZERO, 'copy');imm(c, 9, T1, T1, 4)
    imm(c, 35, A0, SP, 16);imm(c, 35, T2, SP, 24)
    imm(c, 43, T2, A0, 0x288)
    c.emit((3 << 26) | (0x1515b8 >> 2), 0)
    imm(c, 35, T0, SP, 16);imm(c, 35, T1, SP, 20)
    constant(c, T2, 70)
    c.emit((T1 << 21) | (T2 << 16) | 0x18, (T1 << 11) | 0x12)
    constant(c, T2, 0x535538);imm(c, 35, T2, T2, 0)
    add(c, T1, T1, T2)
    for i in range(7):
        imm(c, 36, T2, T0, 0xbdf + i);imm(c, 40, T2, T1, 210 + i)
    imm(c, 9, SP, SP, 32)
    # Replay the four displaced instructions. These LQ operations restore
    # full 128-bit EE saved registers, not just the scalar low words.
    c.emit(0x24020001, 0x7bb00080, 0x7bb10070, 0x7bb20060,
           (2 << 26) | (0x15303c >> 2), 0)
    return c.bytes()
