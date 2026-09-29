"""Combine upstream heuristic ranges with independently verified function entries."""
import csv
import json
import re
from pathlib import Path


def write_function_map(output, entries_path, destination):
    functions = {}
    for source in Path(output).glob('*.cpp'):
        match = re.search(r'// Address: (0x[0-9a-fA-F]+) - (0x[0-9a-fA-F]+)', source.read_text())
        if match:
            start, end = (int(s, 16) for s in match.groups())
            functions[start] = end
        else:
            # Runtime stub wrappers omit the address-range comment. Keep their
            # entry points in the map so explicit handler bindings survive.
            stub = re.search(r'void sub_([0-9A-F]{8})_0x[0-9a-f]+\(', source.read_text())
            if stub:
                start = int(stub[1], 16)
                functions[start] = start + 4
    if not functions:
        raise ValueError('No heuristic function ranges found')
    for extra in json.loads(Path(entries_path).read_text()):
        start, end = int(extra['address'], 16), int(extra['end'], 16)
        if start % 4 or end % 4 or end <= start:
            raise ValueError(f'Invalid verified range {extra}')
        owners = [a for a, b in functions.items() if a <= start < b]
        if not owners:
            raise ValueError(f'Entry {start:#x} is outside discovered executable ranges')
        functions[start] = end
    starts = sorted(functions)
    with Path(destination).open('w') as stream:
        writer = csv.writer(stream)
        writer.writerow(['Name', 'Start', 'End', 'Size'])
        for i, start in enumerate(starts):
            end = min(functions[start], starts[i + 1]) if i + 1 < len(starts) else functions[start]
            writer.writerow([f'sub_{start:08X}', hex(start), hex(end), end - start])
    return len(starts)
