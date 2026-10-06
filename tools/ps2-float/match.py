"""The matcher (docs/ps2-float.md "Matcher"): which port arithmetic sites compute a tick differently from the PS2.

usage: match.py PORT.json ORACLE.fpu [--limit N] [--swaps OUT.json] [--exclude REGEX] [--from INDEX]

PORT.json is one tick of the port's helper calls (tools/ps2-float/trace_hook.mjs with a trace core); ORACLE.fpu is the
same tick of the PS2's own code (tools/ps2-float/ee_oracle --trace-fpu, from a snap_at.py state of that tick). Both
sides start the tick from the same state, so every port result should appear among the PS2's. Each port call is
classed:
  match     the PS2 has the same op, operands and result;
  swap      a multiply whose operands the PS2 has the other way round, with another result (the deficit): the port must
            pass them in the PS2's order (fs, ft);
  form      the PS2 has the same op and operands with another result (a different op underneath, e.g. a rounding);
  value     the result bits appear elsewhere in the PS2 tick (computed another way, same value);
  drift     an operand is within a few ULPs of a PS2 op's whose other operand matches: an earlier difference arrived here;
  unmatched nothing comparable (port-only work: presentation, the camera, or code the oracle's call did not cover).
Port calls at sites matching --exclude are skipped: by default the world objects, presentation and camera code, which the
PS2 runs outside the rider pass (0x128AF0) the oracle calls. The report lists swap / form / drift sites in port order (the first one is where the tick starts to differ), with the PS2
pcs they correspond to.
"""
import bisect
import json
import re
import sys
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from ps2fpu_mul import mul_bits  # noqa: E402

PORT_OPS = {1: 'add', 2: 'sub', 3: 'mul', 4: 'div', 5: 'sqrt', 6: 'add', 7: 'sub', 8: 'div', 9: 'sqrt', 10: 'div', 11: 'sqrt'}
PORT_NAMES = {1: 'terrain add', 2: 'terrain sub', 3: 'terrain mul', 4: 'terrain div', 5: 'terrain sqrt', 6: 'EE add', 7: 'EE sub',
              8: 'EE div', 9: 'EE sqrt', 10: 'collision div', 11: 'collision sqrt'}


# World objects (modifiers, rollers, the stage VM, weather, snow) and presentation / camera code: run outside 0x128AF0.
DEFAULT_EXCLUDE = (r'modifier|roller|magnet|stage_|weather|snow|camera|board_sparks|wake|trail|boost_ribbon|boost_.*hud|presentation|'
                   r'fx|lighting|irradiance|replay|falling_billboard|avalanche|livecomp|tumbler|one_way_volume|set_piece')


def ps2_ops(path):
    """Canonical (op, a, b, result, pc, name) from the oracle trace, MAC adds included (acc +- product)."""
    ops = []
    last_product = None
    pattern = re.compile(r'^fpu (\S+) pc (\S+) ([0-9A-F]{8}) ([0-9A-F]{8}) ([0-9A-F]{8}) -> ([0-9A-F]{8})')
    for line in open(path):
        match = pattern.match(line)
        if not match:
            continue
        name, pc, a, b, c, r = match.groups()
        a, b, c, r = (int(x, 16) for x in (a, b, c, r))
        base = name.split('.')[0]
        if base == 'mul(mac)':
            last_product = r
            ops.append(('mul', a, b, r, pc, name))
        elif base in ('add', 'adda', 'vadd', 'vadda'):
            ops.append(('add', a, b, r, pc, name))
        elif base in ('sub', 'suba', 'vsub', 'vsuba'):
            ops.append(('sub', a, b, r, pc, name))
        elif base in ('mul', 'mula', 'vmul', 'vmula', 'vmulaq', 'vmulai', 'vopmula'):
            ops.append(('mul', a, b, r, pc, name))
        elif base in ('madd', 'madda', 'vmadd', 'vmadda'):
            ops.append(('add', c, last_product, r, pc, name))
        elif base in ('msub', 'msuba', 'vmsub', 'vmsuba', 'vopmsub'):
            ops.append(('sub', c, last_product, r, pc, name))
        elif base in ('div', 'vdiv', 'rsqrt', 'vrsqrt'):
            ops.append(('div', a, b, r, pc, name))
        elif base in ('sqrt', 'vsqrt', 'sqrt(rsqrt)'):
            ops.append(('sqrt', a, 0, r, pc, name))
    return ops


def ulp_distance(x, y):
    def ordered(w):
        return (w & 0x7FFFFFFF) if not (w & 0x80000000) else -(w & 0x7FFFFFFF)
    return abs(ordered(x) - ordered(y))


def main():
    port = json.load(open(sys.argv[1]))
    ps2 = ps2_ops(sys.argv[2])
    limit = int(sys.argv[sys.argv.index('--limit') + 1]) if '--limit' in sys.argv else 40
    exact = defaultdict(set)
    by_operands = defaultdict(list)
    by_result = defaultdict(list)
    by_one_operand = defaultdict(list)
    for op, a, b, r, pc, name in ps2:
        exact[(op, a, b)].add(r)
        by_operands[(op, a, b)].append((r, pc, name))
        if op in ('add',):
            exact[(op, b, a)].add(r)
            by_operands[(op, b, a)].append((r, pc, name))
        by_result[r].append((op, a, b, pc, name))
        by_one_operand[(op, 0, a)].append((b, r, pc, name))
        by_one_operand[(op, 1, b)].append((a, r, pc, name))
    exclude = re.compile(sys.argv[sys.argv.index('--exclude') + 1] if '--exclude' in sys.argv else DEFAULT_EXCLUDE)
    counts = defaultdict(int)
    findings = []
    # Per multiply site: calls whose result depends on the operand order, split by which order the PS2 used.
    order_votes = defaultdict(lambda: {'port': 0, 'swapped': 0})
    for index, (site, code, a, b, r) in enumerate(port['entries']):
        if exclude.search(port['sites'][site]):
            counts['excluded'] += 1
            continue
        op = PORT_OPS[code]
        if op == 'sqrt':
            b = 0
        kind = None
        detail = None
        if r in exact.get((op, a, b), ()):
            kind = 'match'
        elif op == 'mul' and r in exact.get((op, b, a), ()):
            kind = 'match'
        elif op == 'mul' and (op, b, a) in by_operands:
            kind = 'swap'
            detail = by_operands[(op, b, a)][0]
        elif (op, a, b) in by_operands:
            kind = 'form'
            detail = by_operands[(op, a, b)][0]
        elif r in by_result and (r & 0x7FFFFFFF) != 0:
            kind = 'value'
        else:
            near = None
            for side, same, other in ((0, a, b), (1, b, a)):
                for candidate, result, pc, name in by_one_operand.get((op, side, same), ()):
                    distance = ulp_distance(candidate, other)
                    if 0 < distance <= 4 and (near is None or distance < near[0]):
                        near = (distance, candidate, result, pc, name)
                if op == 'mul':
                    for candidate, result, pc, name in by_one_operand.get((op, 1 - side, same), ()):
                        distance = ulp_distance(candidate, other)
                        if 0 < distance <= 4 and (near is None or distance < near[0]):
                            near = (distance, candidate, result, pc, name)
            if near:
                kind = 'drift'
                detail = near
            else:
                kind = 'unmatched'
        if op == 'mul' and mul_bits(a, b) != mul_bits(b, a):
            if (op, a, b) in by_operands:
                order_votes[site]['port'] += 1
            elif (op, b, a) in by_operands:
                order_votes[site]['swapped'] += 1
        counts[kind] += 1
        if kind in ('swap', 'form', 'drift'):
            findings.append((index, kind, port['sites'][site], PORT_NAMES[code], a, b, r, detail))
    print('port calls', len(port['entries']), 'ps2 ops', len(ps2), dict(counts))
    seen = set()
    shown = 0
    for index, kind, site, name, a, b, r, detail in findings:
        key = (kind, site)
        if key in seen:
            continue
        seen.add(key)
        if kind in ('swap', 'form'):
            other, pc, ps2_name = detail
            print(f'{kind:5} #{index:5} {site} {name}({a:08X}, {b:08X}) = {r:08X}; PS2 {pc} {ps2_name} gives {other:08X}')
        else:
            distance, candidate, result, pc, ps2_name = detail
            print(f'{kind:5} #{index:5} {site} {name}({a:08X}, {b:08X}) = {r:08X}; PS2 {pc} {ps2_name} has {candidate:08X} '
                  f'({distance} ULP) -> {result:08X}')
        shown += 1
        if shown >= limit:
            break
    # Provenance of the first drift: walk back through the port calls that produced its operands (the nearest earlier
    # call with those result bits), so the chain ends at the call where the port first left the PS2.
    kinds = {}
    for index, kind, *_ in findings:
        kinds[index] = kind
    first_drift = next((f for f in findings if f[1] == 'drift'), None)
    if '--from' in sys.argv:
        start = int(sys.argv[sys.argv.index('--from') + 1])
        first_drift = next((f for f in findings if f[0] == start), (start,))
    if first_drift:
        by_value = defaultdict(list)
        for index, (site, code, a, b, r) in enumerate(port['entries']):
            by_value[r].append(index)

        def producer(value, before):
            """The nearest earlier port call whose result is value (or its negation)."""
            best = None
            for candidate in (value, value ^ 0x80000000):
                indices = by_value.get(candidate, [])
                position = bisect.bisect_left(indices, before) - 1
                if position >= 0 and (best is None or indices[position] > best):
                    best = indices[position]
            return best

        print('provenance of the first drift:')
        frontier = [(first_drift[0], 0)]
        visited = set()
        while frontier:
            index, depth = frontier.pop()
            if index in visited or depth > 12:
                continue
            visited.add(index)
            site, code, a, b, r = port['entries'][index]
            sensitive = PORT_OPS[code] == 'mul' and mul_bits(a, b) != mul_bits(b, a)
            print(f'  {"  " * depth}#{index} {port["sites"][site]} {PORT_NAMES[code]}({a:08X}, {b:08X}) = {r:08X}'
                  f'{" [order-sensitive]" if sensitive else ""} {kinds.get(index, "")}')
            for operand in (a, b):
                # A negation or abs between two calls is not a helper call: producer() follows the magnitude too.
                source = producer(operand, index)
                if source is not None and (operand & 0x7FFFFFFF) not in (0, 0x3F800000):
                    frontier.append((source, depth + 1))
    # A site is swapped only when every order-sensitive call there wanted the other order (a shared helper serving
    # PS2 code with both orders is reported, not swapped).
    swaps = []
    for site, votes in sorted(order_votes.items(), key=lambda kv: kv[0]):
        name = port['sites'][site]
        if votes['swapped'] and not votes['port']:
            swaps.append(name)
        elif votes['swapped'] and votes['port']:
            print(f'mixed  {name}: {votes["port"]} calls want the port order, {votes["swapped"]} the other')
    print('swap sites', len(swaps))
    if '--swaps' in sys.argv:
        Path(sys.argv[sys.argv.index('--swaps') + 1]).write_text(json.dumps(swaps, indent=1) + '\n')


if __name__ == '__main__':
    main()
