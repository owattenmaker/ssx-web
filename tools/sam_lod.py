"""Distance LODs for Sam's PS2 parts, sized like the original rider templates.

The original Mac/board templates carry H/M/L/Shdw models (tools/sam_ps2 --inspect-lods):
M keeps about 52% of the H triangles, L about 27%, the shadow model about 25%. Sam's
LODs are vertex-clustered from the authored H mesh; every LOD triangle references
existing native vertices, so position, UV, normal and skin weights stay exact and
tools/sam_ps2/SamParts.cs can emit them with the same vertex readers. The browser and
the native Metal client draw LOD0 only (like every original rider there).
"""
import numpy as np

LEVELS = [('M', .52), ('L', .27), ('Shdw', .25)]


def cluster(positions, uvs, tris, cell):
    key_p = np.floor(positions[tris.ravel()] / cell).astype(np.int64)
    key_u = np.floor(uvs[tris.ravel()] * 6).astype(np.int64)
    keys = np.concatenate([key_p, key_u], 1)
    verts = tris.ravel()
    uniq, inverse = np.unique(keys, axis=0, return_inverse=True)
    inverse = inverse.ravel()
    # representative: the member vertex closest to the cluster centroid
    rep = np.empty(len(uniq), np.int64)
    centroid = np.zeros((len(uniq), 3)); count = np.zeros(len(uniq))
    np.add.at(centroid, inverse, positions[verts]); np.add.at(count, inverse, 1)
    centroid /= count[:, None]
    dist = np.linalg.norm(positions[verts] - centroid[inverse], axis=1)
    order = np.lexsort((dist, inverse))
    first = np.ones(len(order), bool); first[1:] = inverse[order][1:] != inverse[order][:-1]
    rep[inverse[order][first]] = verts[order][first]
    out = rep[inverse].reshape(-1, 3)
    keep = (out[:, 0] != out[:, 1]) & (out[:, 1] != out[:, 2]) & (out[:, 0] != out[:, 2])
    out = out[keep]
    if len(out):
        _, idx = np.unique(np.sort(out, 1), axis=0, return_index=True)
        out = out[np.sort(idx)]
    return out


def decimate(positions, uvs, tris, target):
    """Largest cell size whose clustering keeps at least `target` triangles."""
    if target >= len(tris): return tris
    lo, hi = 1e-4, .3
    best = tris
    for _ in range(28):
        mid = (lo * hi) ** .5
        out = cluster(positions, uvs, tris, mid)
        if len(out) >= target: best = out; lo = mid
        else: hi = mid
    return best


def build_lods(vertices, indices, parts, family):
    """vertices: (n,10) native rows; parts: rider parts; family(name) -> PS2 part family.

    Decimates each family as a whole (so small pieces may drop out at distance, as in the
    originals) and splits the result back into per-part triangle lists.
    """
    pos = vertices[:, :3].astype(float); uv = vertices[:, 6:8].astype(float)
    part_of = np.empty(len(vertices), np.int64)
    for i, p in enumerate(parts): part_of[p['first_vertex']:p['first_vertex'] + p['vertex_count']] = i
    families = {}
    for i, p in enumerate(parts): families.setdefault(family(p['name']), []).append(i)
    levels = []
    for name, ratio in LEVELS:
        level = dict(name=name, ratio=ratio, parts=[])
        for fam, members in families.items():
            tris = np.concatenate([indices[parts[i]['first_index']:parts[i]['first_index'] + parts[i]['index_count']].reshape(-1, 3) for i in members])
            # clustering never merges across materials: decimate per material, sharing the family budget
            out_all = []
            for material in sorted({parts[i]['material'] for i in members}):
                sel = np.concatenate([indices[parts[i]['first_index']:parts[i]['first_index'] + parts[i]['index_count']].reshape(-1, 3) for i in members if parts[i]['material'] == material])
                out_all.append(decimate(pos, uv, sel, max(1, int(round(len(sel) * ratio)))))
            out = np.concatenate(out_all) if out_all else np.zeros((0, 3), np.int64)
            owner = part_of[out[:, 0]]
            for i in members:
                t = out[owner == i]
                if len(t): level['parts'].append(dict(name=parts[i]['name'], material=parts[i]['material'], family=fam, indices=t.ravel().tolist()))
            level.setdefault('triangles', {})[fam] = dict(h=int(len(tris)), lod=int(len(out)))
        levels.append(level)
    return levels
