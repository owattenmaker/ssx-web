"""Probe targets over the connector-location terrain (tracks other than the course's).

The Snow Jam race event also holds the A_ARA1 (track 3) and ARA1_B (track 9) patches
(tools/export_event_membership.py). The world-query oracles move their captured rider
query onto these points so that the original octree query and the native world both
see the connector patches, including the ones sharing octree cells with track 8.
Returned points are original source centimetres (Z up).
"""
import json, math, random
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def _surface(coefficients, u, v):
    point = [0.0] * 3; du = [0.0] * 3; dv = [0.0] * 3
    for j in range(4):
        for i in range(4):
            c = coefficients[i + 4 * j]
            for k in range(3):
                point[k] += c[k] * u ** i * v ** j
                if i: du[k] += c[k] * i * u ** (i - 1) * v ** j
                if j: dv[k] += c[k] * j * u ** i * v ** (j - 1)
    n = [du[1] * dv[2] - du[2] * dv[1], du[2] * dv[0] - du[0] * dv[2], du[0] * dv[1] - du[1] * dv[0]]
    length = math.sqrt(sum(x * x for x in n)) or 1.0
    n = [x / length for x in n]
    if n[1] < 0:
        n = [-x for x in n]
    return point, n


def _overlaps(a, b):
    return all(a['authored_bounds_min'][k] <= b['authored_bounds_max'][k] and b['authored_bounds_min'][k] <= a['authored_bounds_max'][k] for k in range(3))


def targets(heights=(20.0, 60.0, 150.0), per_track=12, border=12, seed=0x3303F0, folder=ROOT / 'local/assets/native/ARA1'):
    patches = json.loads((Path(folder) / 'terrain.json').read_text())['patches']
    course = [p for p in patches if p['track'] == 8]
    rng = random.Random(seed); chosen = []
    for track in sorted({p['track'] for p in patches} - {8}):
        own = [p for p in patches if p['track'] == track]
        touching = [p for p in own if any(_overlaps(p, q) for q in course)]
        chosen += rng.sample(own, min(per_track, len(own))) + rng.sample(touching, min(border, len(touching)))
    result = []
    for p in chosen:
        u, v = rng.uniform(0.05, 0.95), rng.uniform(0.05, 0.95)
        point, n = _surface(p['coefficients'], u, v)
        for h in heights:
            native = [point[k] + n[k] * h / 100 for k in range(3)]
            result.append([native[0] * 100, -native[2] * 100, native[1] * 100])   # native (x, y up, z) m -> source cm
    return result


if __name__ == '__main__':
    t = targets(); print(len(t), t[:2])
