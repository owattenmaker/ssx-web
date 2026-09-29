"""Sam's head: an original roster head (Mac's HeadA) warped to Sam's photo-fitted landmarks, with a painted head map.

Built from the original parts, like the rest of Sam's derived body (tools/sam_mesh.py):
  Mac's HeadA_NIS (the front-end head, 551 v), Eyes_NIS (46 v) and his "Skint" scalp HeadBoltB (103 v), the original
  cranium that closes HeadA's open top: the roster heads leave the skull to the hat and hair parts. They keep the
  roster topology (edge loops around the eyes, nose and mouth), Mac's UV layout and his head/neck/upper-spine weights.
  Source: web/public/assets/WARDROBE/MAC/{wardrobe.json,parts.bin} (tools/export_wardrobe.py; derived game data).

Fit (the likeness): 17 landmarks marked on four of Sam's photos (sam_character/*.jpeg) -> Sam's 3D landmarks, solved
jointly with one pinhole camera per photo at its EXIF focal length (26 / 28 / 70 mm equivalent; the three-quarter
photo was taken from about 0.55 m, so an orthographic fit would exaggerate the nose). Mac's landmarks are the prior
(regularised offsets, mirrored pairs). A biharmonic RBF warp (phi = r, plus affine) maps Mac's landmarks onto them;
a template-space sculpt between the landmarks recesses the cheeks (fitted to the three-quarter photo's profile
silhouette), and design targets set what the photos don't show: a long straight bridge, a heavier brow over deep-set,
hooded eyes, thin lips, a jaw tapering to a narrow chin and a slender neck.

Normals: Mac's authored normals carried through the warp (inverse transpose of its Jacobian), so the originals' split
normals at the lids, lips, nostrils and ears survive; recomputed smooth normals put up to 119 degree errors there,
which the view-dependent FE rim term (389CB8, added to the lit colour) showed as pale bands. The lid, nose-underside
and mouth-line normals are then tilted toward the face front: Sam's hooded lids and longer nose present more grazing
surfaces to the front-end camera than Mac's.

Head map (128x256): the top 128x128 is the roster HeadA layout at the roster's texel density, the bottom half the
scalp. It is painted, not a photo decal (the photos give only the landmark positions, no texels): airbrushed skin with
soft baked shading (AO, under the brow, beside the nose, under the cheekbones and jaw), painted low straight brows, a
drooping moustache, stubble, sideburns and hair, placed from the fitted landmarks; the eye, teeth and mouth-interior
swatches, the lid/lash darkening and the inner-ear shading come from Mac's map (hazel iris).

Rig units are native metres (Y up, face toward +Z, +X = Sam's left). Requires numpy and Pillow.
"""
import functools, json, math, os, struct
from collections import Counter
from pathlib import Path
import numpy as np
from PIL import Image, ImageFilter
from sam_textures import HAIR                   # the hair clumps' colour (gear map), so head and clumps match

ROOT = Path(os.environ.get('SSX_ROOT') or Path(__file__).resolve().parents[1])
MAC = ROOT / 'web/public/assets/WARDROBE/MAC'
PHOTOS = ROOT / 'sam_character'
MM = .001

# ---------------------------------------------------------------------------------------------- photo landmarks
# (px in the untouched photos; _L = +x, Sam's left). The mouth is open in the three-quarter photo, so its lower face
# is not used; the pogonion of the beanie photo and the side-view bridge silhouette are not landmarks.
PHOTO = {
    'side': ('C96C5016-3DA9-43B4-A7C4-F235C3494135_1_105_c.jpeg', 28),    # iPhone X, 28 mm equivalent
    'beanie': ('683DB2B2-DC8C-4DBB-9DFF-3BCF1284C8E0_1_105_c.jpeg', 26),  # iPhone 13 Pro, 26 mm
    'down': ('D67CC0AB-7E93-458D-8342-60EB310B3B37_1_105_c.jpeg', 26),
    'smile': ('FBC35F40-A745-4BFD-B6B0-3DFEB6FE3453_1_105_c.jpeg', 70),   # RX100 IV, 70 mm
}
OBS = {
    'side': {'eye_L': (238, 402), 'eye_R': (148, 405), 'nose_tip': (136, 477), 'subnasale': (160, 498), 'ear_top_L': (405, 412),
             'ear_lobe_L': (372, 520), 'tragus_L': (366, 468), 'eye_outer_L': (262, 405), 'eye_inner_L': (218, 408), 'eye_outer_R': (129, 403)},
    'beanie': {'eye_R': (327, 450), 'eye_L': (390, 452), 'nose_tip': (370, 483), 'subnasale': (367, 492), 'mouth_corner_R': (330, 510),
               'mouth_corner_L': (390, 513), 'menton': (360, 560), 'gonion_R': (297, 517), 'gonion_L': (400, 520)},
    'down': {'eye_R': (318, 398), 'eye_L': (398, 400), 'nose_tip': (362, 450), 'subnasale': (362, 462), 'mouth_corner_R': (320, 488),
             'mouth_corner_L': (408, 490), 'menton': (356, 548), 'eye_inner_R': (338, 398), 'eye_inner_L': (378, 400), 'eye_outer_R': (296, 396),
             'eye_outer_L': (420, 398)},
    'smile': {'eye_R': (488, 188), 'eye_L': (555, 190), 'nose_tip': (525, 228), 'subnasale': (523, 237), 'menton': (520, 305),
              'eye_outer_R': (473, 187), 'eye_outer_L': (572, 188), 'eye_inner_R': (502, 190), 'eye_inner_L': (543, 191)},
}
OBSERVED = {'nose_tip', 'subnasale', 'menton', 'ear_top', 'ear_lobe', 'tragus', 'gonion', 'eye', 'eye_outer', 'eye_inner', 'mouth_corner'}
# Mac's race head (HeadA_H) vertex ids of the landmarks; pairs are taken at |x|
MIDLINE = {'forehead_top': 125, 'glabella': 126, 'nasion': 127, 'bridge': 132, 'nose_tip': 131, 'columella': 101, 'subnasale': 89,
           'upper_lip': 80, 'stomion': 27, 'lower_lip': 24, 'pogonion': 33, 'menton': 44, 'under_chin': 50, 'throat': 41}
LATERAL = {'ear_top': 70, 'ear_lobe': 72, 'tragus': 65, 'gonion': 13, 'cheekbone': 69, 'temple': 68, 'chin_side': 48, 'jaw_mid': 15,
           'cheek': 67, 'neck_side': 57, 'alar_base': 98}
# template-space sculpt between the landmarks (metres of recession / narrowing)
SHAPE = dict(cheek=.020, temple=.010, jaw=.004, cheek_x=.004)


# ---------------------------------------------------------------------------------------------- Mac's parts
@functools.lru_cache(None)
def _mac():
    return json.loads((MAC / 'wardrobe.json').read_text()), (MAC / 'parts.bin').read_bytes()


def mac_part(res):
    """An original part: positions/normals/uv (package frame), triangles, skin [(file, bone, percent)], part record."""
    w, blob = _mac(); p = w['parts'][res]
    v = np.frombuffer(blob, '<f4', p['vertex_count'] * 10, p['vertex_offset']).reshape(-1, 10).astype(float)
    ix = np.frombuffer(blob, '<u4', p['index_count'], p['index_offset']).astype(int).reshape(-1, 3)
    skin = []
    for i in range(p['vertex_count']):
        at = p['skin_offset'] + i * 20
        skin.append([struct.unpack_from('<BBh', blob, at + 4 + k * 4) for k in range(blob[at])])
    return dict(p=v[:, :3].copy(), n=v[:, 3:6].copy(), uv=v[:, 6:8].copy(), f=ix, skin=skin, record=p)


def mac_head_map():
    """Mac's head map (byte domain RGB): the source of the eye / teeth / mouth-interior swatches."""
    import sys
    if str(ROOT / 'tools') not in sys.path: sys.path.append(str(ROOT / 'tools'))
    from texture_archive import read_archive, decode_png
    w, _ = _mac(); t = w['textures']['mac_head_a01']
    index, files = read_archive(ROOT / 'web/public' / t['pack'].lstrip('/'))
    wd, ht, rgba = decode_png(files['mac_head_a01'])
    return np.frombuffer(rgba, np.uint8).reshape(ht, wd, 4)[..., :3].astype(float) * 2   # PS2 texel domain -> bytes


def boundary_loops(f):
    e = Counter()
    for a, b, c in f:
        for u, v in ((a, b), (b, c), (c, a)): e[tuple(sorted((int(u), int(v))))] += 1
    adj = {}
    for a, b in [k for k, c in e.items() if c == 1]: adj.setdefault(a, []).append(b); adj.setdefault(b, []).append(a)
    seen = set(); out = []
    for s in sorted(adj):
        if s in seen: continue
        loop = [s]; seen.add(s); cur = s
        while True:
            nx = [n for n in adj[cur] if n not in seen]
            if not nx: break
            cur = nx[0]; seen.add(cur); loop.append(cur)
        out.append(loop)
    return out


def template_landmarks(race):
    p, f = race['p'], race['f']
    L = {k: p[i].copy() for k, i in MIDLINE.items()}
    for k, i in LATERAL.items(): q = p[i].copy(); q[0] = abs(q[0]); L[k] = q
    lp = boundary_loops(f)
    eye = [l for l in lp if len(l) == 8 and p[l][:, 1].min() > .54 and p[l][:, 1].max() < .57 and p[l][:, 0].min() > 0][0]
    E = p[eye]; L['eye'] = E.mean(0); L['eye_outer'] = E[E[:, 0].argmax()]; L['eye_inner'] = E[E[:, 0].argmin()]
    L['eye_top'] = E[E[:, 1].argmax()]; L['eye_bottom'] = E[E[:, 1].argmin()]
    M = p[[l for l in lp if len(l) == 8 and abs(p[l][:, 1].mean() - .488) < .005][0]]; L['mouth_corner'] = M[M[:, 0].argmax()]
    neck = [l for l in lp if len(l) == 10 and p[l][:, 1].max() < .45][0]
    return L, p[neck]


# ---------------------------------------------------------------------------------------------- cameras
def rot(yaw, pitch, roll):
    cy, sy = math.cos(yaw), math.sin(yaw); cp, sp = math.cos(pitch), math.sin(pitch); cr, sr = math.cos(roll), math.sin(roll)
    return np.array([[cr, -sr, 0], [sr, cr, 0], [0, 0, 1]]) @ np.array([[1, 0, 0], [0, cp, -sp], [0, sp, cp]]) @ np.array([[cy, 0, sy], [0, 1, 0], [-sy, 0, cy]])


def project(P, cam):
    """Pinhole camera (yaw, pitch, roll, tx, ty, D, f, cx, cy): [u, v] pixels and depth."""
    yaw, pitch, roll, tx, ty, D, f, cx, cy = cam
    q = np.asarray(P) @ rot(yaw, pitch, roll).T; depth = D - q[..., 2]
    return np.stack([cx + f * (q[..., 0] + tx) / depth, cy - f * (q[..., 1] + ty) / depth], -1), depth


def _fit_ortho(P3, P2):
    def solve(yaw, pitch, roll):
        q = P3 @ rot(yaw, pitch, roll).T
        A = np.zeros((2 * len(P3), 3)); b = np.zeros(2 * len(P3))
        A[0::2, 0] = q[:, 0]; A[0::2, 1] = 1; b[0::2] = P2[:, 0]; A[1::2, 0] = -q[:, 1]; A[1::2, 2] = 1; b[1::2] = P2[:, 1]
        x, *_ = np.linalg.lstsq(A, b, rcond=None); r = A @ x - b
        return (yaw, pitch, roll, *x), float(r @ r)
    best = min((solve(y, p, r) for y in np.radians(np.arange(-90, 91, 3)) for p in np.radians(np.arange(-30, 31, 3)) for r in np.radians(np.arange(-20, 21, 4))), key=lambda t: t[1])
    return best[0]


def fit_camera(P3, P2, f, size, init=None):
    P3 = np.asarray(P3, float); P2 = np.asarray(P2, float); cx, cy = size[0] / 2, size[1] / 2
    if init is None:
        yaw, pitch, roll, s, tu, tv = _fit_ortho(P3, P2)
        x = np.array([yaw, pitch, roll, (tu - cx) / s, (cy - tv) / s, f / s])
    else:
        x = np.array(init[:6], float)
    def res(x): return (project(P3, (*x, f, cx, cy))[0] - P2).ravel()
    eps = np.array([1e-4, 1e-4, 1e-4, 1e-5, 1e-5, 1e-4])
    for _ in range(40):
        r = res(x); J = np.stack([(res(x + np.eye(6)[k] * eps[k]) - r) / eps[k] for k in range(6)], 1)
        step, *_ = np.linalg.lstsq(J.T @ J + 1e-3 * np.diag(np.diag(J.T @ J)), -J.T @ r, rcond=None); x = x + step
        if np.abs(step).max() < 1e-7: break
    return (*x, f, cx, cy)


def solve_landmarks(T, iters=10, lam=400.):
    """Sam's 3D landmarks (Mac's + regularised offsets) and one pinhole camera per photo."""
    size = {k: Image.open(PHOTOS / v[0]).size for k, v in PHOTO.items()}
    focal = {k: v[1] * math.hypot(*size[k]) / 43.27 for k, v in PHOTO.items()}          # 35 mm equivalent -> px
    def expand(S):
        out = {}
        for k, v in S.items():
            if abs(T[k][0]) < 1e-6: out[k] = np.array([0, v[1], v[2]])
            else: out[k + '_L'] = np.array([abs(v[0]), v[1], v[2]]); out[k + '_R'] = np.array([-abs(v[0]), v[1], v[2]])
        return out
    S = {k: T[k].copy() for k in OBSERVED}; cams = {}
    for _ in range(iters):
        P = expand(S)
        for ph in OBS:
            keys = [k for k in OBS[ph] if k in P]
            cams[ph] = fit_camera([P[k] for k in keys], [OBS[ph][k] for k in keys], focal[ph], size[ph], cams.get(ph))
        for b in S:
            mid = abs(T[b][0]) < 1e-6
            ob = [(ph, nm, sg) for ph in OBS for nm, sg in ([(b, 1)] if mid else [(b + '_L', 1), (b + '_R', -1)]) if nm in OBS[ph]]
            x = S[b].copy()
            for _ in range(5):
                def res(x):
                    r = []
                    for ph, nm, sg in ob: r.extend(project(np.array([sg * x[0], x[1], x[2]]), cams[ph])[0] - OBS[ph][nm])
                    return np.array(r + list(lam * (x - T[b])))
                r = res(x); J = np.stack([(res(x + np.eye(3)[k] * 1e-5) - r) / 1e-5 for k in range(3)], 1)
                if mid: J[:, 0] = 0
                step, *_ = np.linalg.lstsq(J, -r, rcond=None); x = x + step
                if mid: x[0] = 0
            S[b] = x
    return S, cams


# ---------------------------------------------------------------------------------------------- warp
def g(x, s): return np.exp(-(np.asarray(x, float) / s) ** 2)


def predeform(p, shape=SHAPE):
    """Template-space sculpt between the landmarks (the RBF then maps the landmarks exactly onto their targets)."""
    p = np.asarray(p, float).copy(); ax = np.abs(p[:, 0]); y = p[:, 1]; z = p[:, 2]
    front = np.clip((z - .05) / .05, 0, 1)
    dz = -(shape['cheek'] * g(ax - .052, .02) * g(y - .515, .03) + shape['temple'] * g(ax - .058, .016) * g(y - .58, .022)
           + shape['jaw'] * g(ax - .045, .02) * g(y - .47, .02))
    p[:, 2] += dz * front
    p[:, 0] -= np.sign(p[:, 0]) * shape['cheek_x'] * g(ax - .052, .016) * g(y - .492, .018) * front   # flatter lower cheeks
    return p


def design_targets(T, S):
    """Targets for the landmarks the photos don't show, set from the fitted ones."""
    t = {k: S[k].copy() for k in OBSERVED}
    lift = S['eye'][1] - T['eye'][1]                           # the eye line rose; brow and forehead follow
    t['glabella'] = T['glabella'] + [0, lift, 1.5 * MM]         # a heavier brow over deep-set eyes
    t['forehead_top'] = T['forehead_top'] + [0, lift + 2 * MM, 0]
    t['nasion'] = T['nasion'] + [0, lift, .5 * MM]
    fr = (T['nasion'][1] - T['bridge'][1]) / (T['nasion'][1] - T['nose_tip'][1])
    t['bridge'] = t['nasion'] + (S['nose_tip'] - t['nasion']) * fr + [0, 0, 1.2 * MM]          # long straight bridge
    t['columella'] = S['nose_tip'] + (S['subnasale'] - S['nose_tip']) * .45 + [0, -1.5 * MM, 0]
    t['upper_lip'] = T['upper_lip'] + [0, -2.5 * MM, -1.0 * MM]
    t['stomion'] = T['stomion'] + [0, -1.0 * MM, -1.5 * MM]
    t['lower_lip'] = T['lower_lip'] + [0, 1.0 * MM, -4.0 * MM]    # thin lips (the moustache covers the upper one)
    t['pogonion'] = T['pogonion'] + [0, -3.5 * MM, 1.0 * MM]      # long, defined chin
    t['under_chin'] = T['under_chin'] + [0, 1.5 * MM, -3 * MM]
    t['chin_side'] = T['chin_side'] + [-9 * MM, -2 * MM, -1 * MM] # the jaw tapers to a narrow chin (Mac's is square)
    t['jaw_mid'] = T['jaw_mid'] + [-6 * MM, 0, -2 * MM]
    t['gonion'] = S['gonion'] + [-4 * MM, 0, -1 * MM]
    t['cheek'] = T['cheek'] + [-5 * MM, 0, -2 * MM]
    t['mouth_corner'] = S['mouth_corner'] + [-4 * MM, 0, -.5 * MM]
    t['alar_base'] = T['alar_base'] + [-2.5 * MM, -1 * MM, 0]
    t['neck_side'] = T['neck_side'] + [-6 * MM, 0, 0]              # a slender neck (Mac's is an athlete's)
    t['eye_top'] = T['eye_top'] + [0, lift - 2.8 * MM, -1.4 * MM]  # hooded, deep-set eyes
    t['eye_bottom'] = T['eye_bottom'] + [0, lift + .4 * MM, -.5 * MM]
    return t, lift


class RBF:
    """Biharmonic radial basis warp (phi(r) = r, plus an affine part)."""
    def __init__(self, src, dst, smooth=1e-4):
        src = np.asarray(src, float); n = len(src)
        K = np.linalg.norm(src[:, None] - src[None], axis=2) + smooth * np.eye(n); P = np.hstack([np.ones((n, 1)), src])
        A = np.zeros((n + 4, n + 4)); A[:n, :n] = K; A[:n, n:] = P; A[n:, :n] = P.T
        b = np.zeros((n + 4, 3)); b[:n] = np.asarray(dst, float) - src
        self.w = np.linalg.solve(A, b); self.src = src

    def __call__(self, p):
        p = np.asarray(p, float); n = len(self.src)
        return p + np.linalg.norm(p[:, None] - self.src[None], axis=2) @ self.w[:n] + np.hstack([np.ones((len(p), 1)), p]) @ self.w[n:]


def mirrored(points, T):
    out = {}
    for k, v in points.items():
        if abs(T[k][0]) < 1e-6: out[k] = np.array([0., v[1], v[2]])
        else: out[k + '_L'] = np.array([abs(v[0]), v[1], v[2]]); out[k + '_R'] = np.array([-abs(v[0]), v[1], v[2]])
    return out


def symmetrise(p, ref):
    """Average each vertex with its mirror twin (pairs found in the template), exact zero on the midline."""
    out = p.copy(); key = {tuple(np.round(q, 5)): i for i, q in enumerate(ref)}
    for i, q in enumerate(ref):
        j = key.get(tuple(np.round(q * [-1, 1, 1], 5)))
        if j is not None: out[i] = (p[i] + p[j] * [-1, 1, 1]) / 2
    out[np.abs(ref[:, 0]) < 1e-6, 0] = 0
    return out


def warp_normals(f, p, n, h=1e-4):
    """The authored normals carried through the warp f: inverse-transpose of its Jacobian (keeps the original
    artists' split normals at the lids, lips, nostrils and ears)."""
    J = np.stack([(f(p + h * e) - f(p - h * e)) / (2 * h) for e in np.eye(3)], 2)        # (n, 3, 3), J[:, :, k] = d f / d x_k
    out = np.einsum('nji,nj->ni', np.linalg.inv(J), n)                                   # J^-T n
    return out / np.maximum(np.linalg.norm(out, axis=1, keepdims=True), 1e-12)


def symmetrise_normals(n, ref):
    out = n.copy(); key = {tuple(np.round(q, 5)): i for i, q in enumerate(ref)}
    for i, q in enumerate(ref):
        j = key.get(tuple(np.round(q * [-1, 1, 1], 5)))
        if j is not None: out[i] = (n[i] + n[j] * [-1, 1, 1]) / 2
    out[np.abs(ref[:, 0]) < 1e-6, 0] = 0
    return out / np.maximum(np.linalg.norm(out, axis=1, keepdims=True), 1e-12)


@functools.lru_cache(None)
def fitted():
    """The warped head parts ('race' HeadA_H, 'nis' HeadA_NIS, 'eyes', 'scalp'), landmarks and the warp."""
    parts = {'race': mac_part('mac_heada.mnf'), 'nis': mac_part('mac_heada_nis.mnf'), 'eyes': mac_part('mac_eyes_nis.mnf'), 'scalp': mac_part('mac_headboltb.mnf')}
    T, neck = template_landmarks(parts['race'])
    S, cams = solve_landmarks(T)
    tgt, lift = design_targets(T, S)
    Td = {k: predeform(v[None])[0] for k, v in T.items()}
    src, dst = [], []
    Tm, Gm = mirrored({k: Td[k] for k in tgt}, T), mirrored(tgt, T)
    for k in Gm: src.append(Tm[k]); dst.append(Gm[k])
    for v in neck: src.append(v); dst.append(v * [.9, 1, 1] + [0, 0, .003 * (v[2] < .03)])     # the neck ring, 10% slimmer
    scalp = parts['scalp']['p']
    for v in scalp[scalp[:, 1] > .60][::2]: src.append(v); dst.append(v + [0, lift, 0])        # skull: Mac's, lifted with the brow
    W = RBF(src, dst)
    f = lambda q: W(predeform(q))
    out = {}
    for k, m in parts.items():
        n0 = m['n'] / np.maximum(np.linalg.norm(m['n'], axis=1, keepdims=True), 1e-12)
        out[k] = dict(m, p=symmetrise(f(m['p']), m['p']), n=symmetrise_normals(warp_normals(f, m['p'], n0), m['p']))
    # Sam's hooded lids and longer, lower nose tip turn more grazing surfaces toward the front-end camera than Mac's,
    # where the original view-dependent rim term (added to the lit colour) paints pale bands: tilt those normals forward.
    for k in ('nis', 'race'):
        p, n = out[k]['p'], out[k]['n'].copy(); ax = np.abs(p[:, 0])
        e, sn, st, mc = tgt['eye'], tgt['subnasale'], tgt['stomion'], tgt['mouth_corner']
        lids = ss(-np.abs(ax - e[0]), -.024, -.016) * ss(-np.abs(p[:, 1] - (e[1] - .001)), -.014, -.009) * (p[:, 2] > e[2] - .016)
        under_nose = g(p[:, 0], .012) * g(p[:, 1] - (sn[1] + .004), .007) * (p[:, 2] > sn[2] - .008) * ss(-n[:, 1], .2, .5)
        lip_line = ss(-(ax - mc[0]), -.004, .002) * g(p[:, 1] - st[1], .004) * (p[:, 2] > mc[2] - .006) * ss(-np.abs(n[:, 1]), -.9, -.4)
        n += np.array([0, 0, 1.]) * (1.4 * lids + 1.0 * under_nose + .8 * lip_line)[:, None]
        out[k]['n'] = n / np.linalg.norm(n, axis=1, keepdims=True)
    return out, tgt, cams, W


def warp_points(p):
    """Mac's head frame -> Sam's (for Mac's head-worn wardrobe items: hats, eyewear)."""
    return fitted()[3](predeform(np.asarray(p, float)))


# ---------------------------------------------------------------------------------------------- mesh
def _weights(skin, bones_by_slot):
    out = []
    for group in skin:
        acc = {}
        for f, b, w in group: bone = bones_by_slot.get((f, b), 5); acc[bone] = acc.get(bone, 0) + w
        total = sum(acc.values()); out.append([[b, w / total] for b, w in sorted(acc.items(), key=lambda t: -t[1]) if w > 0])
    return out


FACE_V = (0, .5); SCALP_V = (.5, .5)      # head map: v' = v0 + v * scale (face layout on top, scalp at the bottom)


def build(mesh, material, level='nis'):
    """Adds the head (Sam_head), eyes (Sam_eyes) and scalp (Sam_scalp) with the rig's head/neck/upper-spine weights."""
    parts = fitted()[0]
    slots = {(0, i): i for i in range(22)}
    for key, name, (v0, vs) in ((level, 'Sam_head', FACE_V), ('eyes', 'Sam_eyes', FACE_V), ('scalp', 'Sam_scalp', SCALP_V)):
        if key == 'eyes' and level == 'race': continue            # the race head carries its own eyeballs
        m = parts[key]; uv = m['uv'].copy(); uv[:, 1] = v0 + uv[:, 1] * vs
        mesh.add(name, m['p'], m['f'].tolist(), material, uv, _weights(m['skin'], slots), m['n'])


class Head:
    """Measured sections of the fitted head, for the parts fitted around it (cap, beanie, hair, collar, glasses)."""
    def __init__(self):
        parts, L, _, _ = fitted()
        self.L = L; self.tris = np.concatenate([parts[k]['p'][parts[k]['f']] for k in ('nis', 'scalp')])
        self.top = float(self.tris[..., 1].max()); self.eye = L['eye']; self.ear_top = L['ear_top']; self.tragus = L['tragus']; self.lobe = L['ear_lobe']

    def sec(self, y, zmax=None):
        """(half width, front z, back z) of the surface at height y (optionally ignoring the face in front of zmax)."""
        tris = self.tris if zmax is None else self.tris[self.tris[..., 2].max(1) < zmax]
        a, b = tris, np.roll(tris, -1, axis=1)
        s = (a[..., 1] - y) * (b[..., 1] - y) < 0
        t = (y - a[..., 1][s]) / (b[..., 1][s] - a[..., 1][s]); P = a[s] + t[:, None] * (b[s] - a[s])
        return float(np.abs(P[:, 0]).max()), float(P[:, 2].max()), float(P[:, 2].min())

    def ring(self, y, side, front, back):
        """Ellipse (rx, rz, cz) around the section at y with clearances (clamped under the skull top)."""
        x, zf, zb = self.sec(min(y, self.top - .002))
        return x + side, (zf + front - (zb - back)) / 2, (zf + front + zb - back) / 2


@functools.lru_cache(None)
def head():
    return Head()


# ---------------------------------------------------------------------------------------------- painting
S = 4                                     # supersampling of the painted map
SKIN = np.array([196, 126, 94.]); MUST = np.array([98, 70, 52.]); BROW = np.array([66, 46, 36.]); LIP = np.array([178, 112, 104.])


def ss(x, a, b):
    t = np.clip((np.asarray(x, float) - a) / (b - a), 0, 1); return t * t * (3 - 2 * t)


def noise(shape, scale, seed, aniso=(1, 1)):
    rng = np.random.default_rng(seed); H, W = shape
    grid = rng.random((max(2, int(H / (scale * aniso[0])) + 2), max(2, int(W / (scale * aniso[1])) + 2)))
    return np.asarray(Image.fromarray((grid * 255).astype(np.uint8)).resize((W, H), Image.BICUBIC)).astype(float) / 255


def raster(meshes, W, H, attrs=None):
    """UV-space rasterisation of the +x half: per texel the interpolated position, normal (and optional attribute)."""
    pos = np.zeros((H, W, 3)); nrm = np.zeros((H, W, 3)); tag = np.full((H, W), -1); att = np.zeros((H, W))
    for mi, (p, n, uv, f, tg) in enumerate(meshes):
        at = attrs[mi] if attrs else None
        for a, b, c in f:
            if (p[a, 0] + p[b, 0] + p[c, 0]) < -1e-6: continue
            A = uv[a] * [W, H]; B = uv[b] * [W, H]; C = uv[c] * [W, H]
            area = (B[0] - A[0]) * (C[1] - A[1]) - (C[0] - A[0]) * (B[1] - A[1])
            if abs(area) < 1e-9: continue
            x0 = max(int(np.floor(min(A[0], B[0], C[0]))), 0); x1 = min(int(np.ceil(max(A[0], B[0], C[0]))), W - 1)
            y0 = max(int(np.floor(min(A[1], B[1], C[1]))), 0); y1 = min(int(np.ceil(max(A[1], B[1], C[1]))), H - 1)
            if x0 > x1 or y0 > y1: continue
            X, Y = np.meshgrid(np.arange(x0, x1 + 1) + .5, np.arange(y0, y1 + 1) + .5)
            w0 = ((B[0] - X) * (C[1] - Y) - (C[0] - X) * (B[1] - Y)) / area; w1 = ((C[0] - X) * (A[1] - Y) - (A[0] - X) * (C[1] - Y)) / area
            ins = (w0 >= -1e-4) & (w1 >= -1e-4) & (1 - w0 - w1 >= -1e-4)
            if not ins.any(): continue
            wv = np.stack([w0[ins], w1[ins], 1 - w0[ins] - w1[ins]], 1); sl = (slice(y0, y1 + 1), slice(x0, x1 + 1))
            pos[sl][ins] = wv @ p[[a, b, c]]; nn = wv @ n[[a, b, c]]; nrm[sl][ins] = nn / np.maximum(np.linalg.norm(nn, axis=1, keepdims=True), 1e-9); tag[sl][ins] = tg
            if at is not None: att[sl][ins] = wv @ at[[a, b, c]]
    return pos, nrm, tag, att


def dilate(img, valid, steps):
    img = img.copy(); valid = valid.copy()
    for _ in range(steps):
        acc = np.zeros_like(img); cnt = np.zeros(valid.shape)
        for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1), (1, 1), (1, -1), (-1, 1), (-1, -1)):
            v = np.roll(np.roll(valid, dy, 0), dx, 1); acc[v] += np.roll(np.roll(img, dy, 0), dx, 1)[v]; cnt[v] += 1
        grow = (~valid) & (cnt > 0); img[grow] = acc[grow] / cnt[grow][:, None]; valid = valid | grow
    return img


def vertex_ao(points, normals, tris, rays=96, max_dist=.06, seed=3):
    """Ambient occlusion: fraction of cosine-weighted hemisphere rays that escape the head within max_dist."""
    rng = np.random.default_rng(seed); u1 = rng.random(rays); u2 = rng.random(rays); r = np.sqrt(u1); th = 2 * np.pi * u2
    local = np.stack([r * np.cos(th), r * np.sin(th), np.sqrt(1 - u1)], 1)
    A = tris[:, 0]; E1 = tris[:, 1] - A; E2 = tris[:, 2] - A; out = np.zeros(len(points))
    for i, (o, n) in enumerate(zip(points, normals)):
        t1 = np.cross(n, [0, 1, 0] if abs(n[1]) < .9 else [1, 0, 0]); t1 /= np.linalg.norm(t1); d = local @ np.stack([t1, np.cross(n, t1), n]); o = o + n * 1e-4
        P = np.cross(d[:, None, :], E2[None]); det = np.einsum('rtk,tk->rt', P, E1); inv = 1 / np.where(np.abs(det) < 1e-12, 1e-12, det)
        Tv = o - A; u = np.einsum('rtk,tk->rt', P, Tv) * inv; Q = np.cross(Tv, E1); v = np.einsum('rk,tk->rt', d, Q) * inv; t = np.einsum('tk,tk->t', E2, Q)[None] * inv
        out[i] = 1 - ((np.abs(det) > 1e-12) & (u >= 0) & (v >= 0) & (u + v <= 1) & (t > 2e-4) & (t < max_dist)).any(1).mean()
    return out


def _utility(W):
    mac = np.clip(mac_head_map(), 0, 255).astype(np.uint8)
    big = np.asarray(Image.fromarray(mac).resize((W, W), Image.BICUBIC)).astype(float); near = np.asarray(Image.fromarray(mac).resize((W, W), Image.NEAREST)).astype(float)
    U, V = np.meshgrid((np.arange(W) + .5) / W, (np.arange(W) + .5) / W)
    lum = near.mean(2); sat = near.max(2) - near.min(2); eyebox = (U > .62) & (V < .27)
    iris = eyebox & (np.hypot(U - .836, V - .117) < .042)
    return big, dict(sclera=(eyebox & (sat < 60)) | iris, iris=iris, teeth=(U > .74) & (V > .84),
                     black=(lum < 40) & ((((U > .08) & (U < .34)) & ((V > .2) & (V < .36))) | ((U > .5) & (V > .82))))


def paint_face(variant='default', W=128 * S):
    parts, L, _, _ = fitted()
    nis, eyes, race = parts['nis'], parts['eyes'], parts['race']
    occ = np.concatenate([parts[k]['p'][parts[k]['f']] for k in ('nis', 'eyes', 'scalp')])
    ao_v = vertex_ao(nis['p'], nis['n'], occ)
    pos, nrm, tag, ao = raster([(nis['p'], nis['n'], nis['uv'], nis['f'], 0), (eyes['p'], eyes['n'], eyes['uv'], eyes['f'], 1)], W, W, [ao_v, np.ones(len(eyes['p']))])
    p2, n2, t2, _ = raster([(race['p'], race['n'], race['uv'], race['f'], 2)], W, W)
    gap = (tag < 0) & (t2 >= 0); pos[gap] = p2[gap]; nrm[gap] = n2[gap]; tag[gap] = 2; ao[gap] = 1
    valid = tag >= 0
    x = np.abs(pos[..., 0]); y = pos[..., 1]; z = pos[..., 2]; nz = nrm[..., 2]; ny = nrm[..., 1]
    ey, ex = L['eye'][1], L['eye'][0]
    sn, st, mc, tip, men = L['subnasale'], L['stomion'], L['mouth_corner'], L['nose_tip'], L['menton']
    tr, lobe, etop = L['tragus'], L['ear_lobe'], L['ear_top']
    front = ss(nz, -.1, .45); shape2 = pos.shape[:2]
    # skin base with regional tints
    rgb = np.broadcast_to(SKIN, pos.shape).copy(); rosy = np.array([222, 148, 128.])
    rgb += (rosy - SKIN) * (.6 * g(x, .012) * g(y - tip[1], .012) * ss(z, tip[2] - .03, tip[2] - .005))[..., None]
    rgb += (rosy - SKIN) * (.12 * g(x - .045, .018) * g(y - (ey - .022), .016) * front)[..., None]
    rgb += (np.array([214, 158, 132.]) - SKIN) * (.5 * ss(y, ey + .01, ey + .05) * front)[..., None]
    rgb += (np.array([206, 156, 132.]) - SKIN) * (.6 * ss(-y, -(men[1] - .004), -(men[1] - .03)))[..., None]
    ear = ss(x, tr[0] - .004, tr[0] + .004) * ss(-z, -(tr[2] + .01), -(tr[2] - .002)) * ss(y, lobe[1] - .006, lobe[1] + .004) * ss(-y, -(etop[1] + .006), -(etop[1] - .002))
    rgb += (np.array([212, 132, 116.]) - SKIN) * (.55 * ear)[..., None]
    rgb *= (.97 + .06 * noise(shape2, 6 * S, 5))[..., None]
    # baked soft shading
    key = np.clip(nrm @ (np.array([0, .55, .83]) / np.linalg.norm([0, .55, .83])), -1, 1)
    shade = (.78 + .22 * ao) * (.90 + .10 * key)
    shade *= 1 - .20 * g(x - ex, .016) * g(y - (ey + .006), .006) * front                       # under the brow ridge
    shade *= 1 - .14 * g(x - ex * .45, .006) * g(y - (ey - .004), .010)                         # inner socket
    shade *= 1 - .12 * g(x - .016, .006) * g(y - (tip[1] - .002), .012) * front                 # beside the nose
    shade *= 1 - .16 * g((y - (ey - .036)) + .45 * (x - .048), .007) * g(x - .050, .014) * front  # under the cheekbones
    shade *= 1 + .07 * g(x - .052, .012) * g(y - (ey - .016), .007) * front                     # cheekbone highlight
    shade *= 1 - .10 * g((y - sn[1]) + .9 * (x - .024), .003) * ss(x, .016, .026) * ss(-x, -.036, -.030) * front   # nasolabial fold
    shade *= 1 - .22 * ss(-ny, .15, .6) * ss(-y, -(men[1] + .02), -(men[1] - .004))             # under the jaw and chin
    shade *= 1 + .06 * g(x, .012) * g(y - (ey + .03), .02) * front
    shade *= 1 + .08 * g(x, .006) * g(y - (ey - .015), .010) * front                            # nose bridge
    shade *= 1 + .05 * g(x, .012) * g(y - (men[1] + .012), .006) * front
    shade *= 1 - .10 * ss(-z, -(tr[2] - .005), -(tr[2] - .03)) * ss(y, .47, .55)                # behind the ear
    shade *= 1 - .45 * ss(-ny, .35, .8) * g(x - .005, .009) * g(y - (sn[1] + .004), .005) * ss(z, sn[2] - .006, sn[2] + .002)   # painted nostrils
    rgb *= shade[..., None]
    # stubble
    beard_top = mc[1] + .004 + .55 * np.clip(x - mc[0], 0, None)
    beard = ss(-(y - beard_top), -.003, .004) * ss(y, men[1] - .03, men[1] - .006) * ss(-z, -(tr[2] + .02), -(tr[2] + .005))
    beard = np.maximum(beard, g(x, .02) * g(y - (st[1] - .014), .010) * front)
    rgb += (np.array([150, 116, 98.]) - rgb) * (.30 * beard * (.7 + .6 * noise(shape2, 1.3 * S / 4 + 1, 7)))[..., None]
    # lips
    lower = ss(-(y - (st[1] - .0005)), -.0006, .0006) * ss(y, st[1] - .0085, st[1] - .006) * g(x, mc[0] * .85) * front
    rgb += (LIP - rgb) * (.55 * lower)[..., None]
    rgb *= (1 - .35 * g(y - st[1], .0012) * g(x, mc[0] * 1.05) * front)[..., None]
    # moustache
    x_end = mc[0] + .010
    top = np.where(x < .008, sn[1] - .0010, np.interp(x, [.008, .02, mc[0], x_end], [sn[1] - .0010, sn[1] + .0002, mc[1] + .0085, mc[1] + .003]))
    bot = np.interp(x, [0, mc[0] * .7, mc[0], x_end], [st[1] + .0022, st[1] + .0016, mc[1] - .0005, mc[1] - .0060])
    inside = ss(-(y - top), -.0016, .0008) * ss(y - bot, -.0008, .0010) * ss(-(x - x_end), -.0025, .0015) * ss(nz, .05, .3)
    t = np.clip((top - y) / np.maximum(top - bot, 1e-4), 0, 1)
    must = MUST * (.82 + .22 * t)[..., None] * (.82 + .32 * noise(shape2, 1.0 * S, 11, aniso=(2.5, .45)))[..., None]
    rgb += (must - rgb) * (inside * .95)[..., None]
    rgb *= (1 - .18 * g(y - (bot - .0012), .0012) * ss(-(x - x_end), -.002, .002) * front)[..., None]
    # brows: low, straight, fairly heavy
    bx0, bx1 = ex - .021, ex + .024
    bcen = ey + .0105 + .0012 * (1 - ((x - (ex + .002)) / .023) ** 2) - .003 * ss(x, ex + .012, bx1)
    bh = np.interp(x, [bx0, ex, bx1], [.0050, .0045, .0027])
    brow = ss(-np.abs(y - bcen), -bh - .0008, -bh + .0008) * ss(x, bx0 - .002, bx0 + .002) * ss(-x, -bx1 - .002, -bx1 + .002) * ss(nz, .1, .4)
    rgb += (BROW * (.85 + .35 * noise(shape2, 1.0 * S, 13, aniso=(.5, 2.2)))[..., None] - rgb) * (brow * .92)[..., None]
    # sideburns, temples and the hair behind the ears
    hair = HAIR * (.85 + .3 * noise(shape2, .9 * S, 17, aniso=(3, .5)))[..., None]
    side = ss(x, tr[0] - .012, tr[0] - .006) * ss(z, tr[2] - .002, tr[2] + .004) * ss(-z, -(tr[2] + .018), -(tr[2] + .012)) * ss(y, tr[1] - .006, tr[1] + .002)
    temple = ss(x, ex + .03, ex + .04) * ss(y, ey + .012, ey + .024) * ss(-z, -(tr[2] + .05), -(tr[2] + .03))
    back = ss(-z, -(tr[2] - .014), -(tr[2] - .022)) * ss(y, .478, .492)
    hmask = np.clip(np.maximum.reduce([side, temple, back]) * (1 - ear), 0, 1)
    if variant == 'buzz':
        hmask *= .35; hair = SKIN * .62 + np.array([34, 22, 14.])
    rgb += (hair - rgb) * hmask[..., None]
    # swatches of the roster layout (eyeball, teeth, mouth interior, eye socket)
    big, masks = _utility(W)
    sclera = big * [.86, .83, .78]                                                               # warmer, less glaring whites
    iris = masks['iris'] & ((big[..., 2] > big[..., 0] + 12) | (big.mean(2) < 80))
    sclera[iris] = np.array([118, 104, 64.]) * np.clip(big.mean(2, keepdims=True)[iris] / 110, .15, 1.4) ** 1.1   # hazel
    rgb = np.where(masks['sclera'][..., None], sclera, rgb)
    rgb = np.where(masks['teeth'][..., None], big, rgb)
    rgb = np.where((masks['black'] & ~masks['sclera'])[..., None], big, rgb)
    rgb *= (1 - .18 * g(x - ex, .012) * g(y - (ey + .0035), .0028) * front * valid)[..., None]   # hooded upper lid
    # lids and lashes: the roster map's painted darkening around the eye opening (as a ratio, keeping Sam's colours)
    ratio = np.clip(np.asarray(Image.fromarray(np.clip(big.mean(2), 0, 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(S * .4))).astype(float) / 150, .35, 1)
    near_eye = ss(-np.abs(x - ex), -.021, -.016) * ss(-np.abs(y - (ey + .001)), -.0062, -.0045) * valid * ~masks['sclera']
    rgb *= (1 - (1 - ratio) * near_eye * .85)[..., None]
    # ears: Mac's painted inner-ear shading on the same ear geometry (luminance ratio against his skin)
    ear_ratio = np.clip(np.asarray(Image.fromarray(np.clip(big.mean(2), 0, 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(S * .3))).astype(float) / 160, .45, 1.15)
    rgb *= (1 + (ear_ratio - 1) * ear * .9)[..., None]
    if variant.startswith('balaclava'):
        knit = np.array([28, 30, 34.]) if variant == 'balaclava' else np.array([150, 28, 30.])
        opening = (g(x - ex, .02) * g(y - ey, .008) + g(x, .02) * g(y - st[1], .006)) > .5
        cover = valid & ~opening & ~masks['sclera'] & ~masks['teeth']
        rib = 1 + .08 * np.sin(np.arctan2(x, z - .07) * 60)
        rgb = np.where(cover[..., None], knit * rib[..., None], rgb)
        if variant == 'balaclava_smile':
            smile = (np.abs(y - (st[1] - .002 + 3 * x ** 2)) < .002) & (x < .03) & valid
            rgb = np.where(smile[..., None], np.array([235, 220, 200.]), rgb)
    return dilate(rgb, valid | masks['sclera'] | masks['teeth'], 12)


def paint_scalp(variant='default', W=128 * S):
    sc = fitted()[0]['scalp']
    pos, nrm, tag, _ = raster([(sc['p'], sc['n'], sc['uv'], sc['f'], 0)], W, W)
    rgb = HAIR * (.78 + .4 * noise((W, W), .8 * S, 21, aniso=(.4, 3)))[..., None]
    if variant == 'buzz':
        rgb = SKIN * .62 + np.array([34, 22, 14.]) + (noise((W, W), .6 * S, 23) * 10)[..., None]
    elif variant.startswith('balaclava'):
        knit = np.array([28, 30, 34.]) if variant == 'balaclava' else np.array([150, 28, 30.])
        rgb = knit * (1 + .08 * np.sin(np.arange(W) / W * 90))[None, :, None] + np.zeros((W, W, 3))
    return dilate(np.broadcast_to(rgb, (W, W, 3)).copy(), tag >= 0, 12)


@functools.lru_cache(None)
def paint(variant='default'):
    """The 128x256 head map (RGBA, byte texel domain): roster HeadA layout on top, scalp below. Variants: default, buzz,
    balaclava, balaclava_smile."""
    face = paint_face(variant); scalp = paint_scalp(variant); W = face.shape[1]
    full = np.concatenate([face, scalp])
    img = Image.fromarray(np.clip(full, 0, 255).astype(np.uint8)).resize((128, 256), Image.LANCZOS)
    return Image.fromarray(np.dstack([np.asarray(img), np.full((256, 128), 255, np.uint8)]), 'RGBA')
