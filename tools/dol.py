#!/usr/bin/env python3
"""Inspect the user's GameCube executable and retain address-accurate evidence."""
import argparse
import hashlib
import json
import struct
from pathlib import Path


class Dol:
    def __init__(self, path):
        self.data = Path(path).read_bytes()
        if len(self.data) < 256:
            raise ValueError('Truncated DOL header')
        self.sections = []
        for i in range(18):
            offset, address, size = [struct.unpack_from('>I', self.data, base + i * 4)[0]
                                     for base in (0, 0x48, 0x90)]
            if not size:
                continue
            if offset < 256 or offset + size > len(self.data) or address + size > 0x81800000 or address < 0x80000000:
                raise ValueError('Invalid GameCube DOL section')
            self.sections.append(dict(index=i, type='text' if i < 7 else 'data', offset=offset, address=address, size=size))
        self.bss, self.bss_size, self.entry = struct.unpack_from('>III', self.data, 0xd8)

    def read(self, address, size):
        for section in self.sections:
            offset = address - section['address']
            if 0 <= offset and offset + size <= section['size']:
                start = section['offset'] + offset
                return self.data[start:start + size]
        raise ValueError(f'Address range {address:#x}+{size:#x} not in a loaded section')

    def report(self):
        return dict(sha256=hashlib.sha256(self.data).hexdigest(), size=len(self.data),
                    entry=self.entry, bss_address=self.bss, bss_size=self.bss_size, sections=self.sections)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('dol', type=Path)
    parser.add_argument('--disassemble', type=lambda value: int(value, 0))
    parser.add_argument('--bytes', type=lambda value: int(value, 0), default=128)
    parser.add_argument('--word', type=lambda value: int(value, 0))
    args = parser.parse_args()
    dol = Dol(args.dol)
    if args.disassemble is not None:
        from capstone import Cs, CS_ARCH_PPC, CS_MODE_BIG_ENDIAN, CS_MODE_32
        decoder = Cs(CS_ARCH_PPC, CS_MODE_BIG_ENDIAN | CS_MODE_32)
        decoder.skipdata = True
        data = dol.read(args.disassemble, args.bytes)
        for offset in range(0, len(data) - 3, 4):
            address = args.disassemble + offset
            word = int.from_bytes(data[offset:offset + 4], 'big')
            opcode = word >> 26
            if opcode in (4, 56, 57, 60, 61):
                # Capstone otherwise mislabels Gekko paired-single encodings
                # as later PowerPC/VSX instructions, which do not exist here.
                print(f'{address:08x}: .long        0x{word:08x}  # Gekko paired-single; see DolRecomp decode')
            else:
                for insn in decoder.disasm(data[offset:offset + 4], address):
                    print(f'{insn.address:08x}: {insn.mnemonic:12} {insn.op_str}')
    elif args.word is not None:
        for section in dol.sections:
            for offset in range(0, section['size'] - 3, 4):
                word = struct.unpack_from('>I', dol.data, section['offset'] + offset)[0]
                if word == args.word:
                    print(f'{section["type"]} 0x{section["address"] + offset:08x}')
    else:
        print(json.dumps(dol.report(), indent=2))


if __name__ == '__main__':
    main()
