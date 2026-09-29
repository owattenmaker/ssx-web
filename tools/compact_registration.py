#!/usr/bin/env python3
"""Coalesce adjacent identical dispatch assignments to keep Clang builds practical."""
import re
import sys
from pathlib import Path


def compact(source):
    pattern = re.compile(r'\s*g_ps2RecompiledFunctionTable\[(\d+)\] = (\w+); // 0x[0-9a-f]+\s*$')
    lines = []
    run = None
    original = {}
    rewritten = {}

    def flush():
        nonlocal run
        if run is None:
            return
        first, last, function = run
        if first == last:
            lines.append(f'        g_ps2RecompiledFunctionTable[{first}] = {function};')
        else:
            lines.append(f'        for (uint32_t slot = {first}; slot <= {last}; ++slot) g_ps2RecompiledFunctionTable[slot] = {function};')
        for index in range(first, last + 1):
            rewritten[index] = function
        run = None

    for line in source.splitlines():
        match = pattern.fullmatch(line)
        if match:
            index, function = int(match[1]), match[2]
            original[index] = function
            if run and index == run[1] + 1 and function == run[2]:
                run = (run[0], index, function)
            else:
                flush()
                run = (index, index, function)
        else:
            flush()
            lines.append(line)
    flush()
    if not original or original != rewritten:
        raise ValueError('Dispatch table verification failed')
    return '\n'.join(lines) + '\n'


if __name__ == '__main__':
    source, destination = map(Path, sys.argv[1:])
    result = compact(source.read_text())
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(result)
