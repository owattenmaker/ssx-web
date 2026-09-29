#!/usr/bin/env python3
"""Sam's Equip Gear wardrobe as parts, like the original riders' WARDROBE/<ID> (tools/export_wardrobe.py layout).

The original gear system (docs/characters.md "Equip Gear and outfits") assembles a rider from per-slot model parts
and per-item textures. Sam's bucket (the Sam PS2 build's BOLTPS2 rows, tools/sam_ps2/SamWardrobe.cs) has Mac's
hierarchy and rules; this tool gives every one of its items a Sam model and texture:

  Sam-authored parts   sam_topa (shirt + vest + collar), sam_bottomb, sam_bootsa, sam_handsa(_nis), sam_heada(_nis),
                       sam_hair (Unc Cut, on the sec_dangle bones), sam_cap (O'Reilly/Fargo), sam_hair_side (Side
                       Splitter, sec_elephant bones), sam_afro (Afro Wig, sec_antenna bones), sam_helmet (Earn Your
                       Turns), sam_kit_net / sam_kit_tube (the fishing kits), sam_bindingsa + sam_boardflexa (K2).
  Sam textures         one map per item: tops (flannel/vest colourways; the Mac tops' palettes for the rest),
                       bottoms, boots, gloves, head (default, buzz, balaclavas), hair, caps, boards (every Mac board
                       graphic resampled onto the K2 outline), kits, the Earn Your Turns helmet and shades.
  Mac accessory parts  the head-worn ones (hats, eyewear, phones, crown, halo, horns, headbands, costume heads)
                       moved from Mac's head to Sam's (eyes 3.3 cm higher, face 1.2 cm flatter); body-worn ones
                       (backpacks, chain, turntable, disc player) and the special boards as they are.
  Rules                Sam keeps his hair under every hat (the hat model rows also equip 143), as his cap always did.

Writes web/public/assets/WARDROBE/SAM/{wardrobe.json (parts_mode), parts.bin, textures/*.png} over the export of
tools/export_wardrobe.py --character sam, and local/sam-model/wardrobe-spec.json (item -> model/texture) that
tools/sam_ps2/SamWardrobe.cs uses for the PS2 bucket. Requires numpy and Pillow.
  python3 tools/sam_wardrobe.py
"""
import hashlib, json, math, os, shutil, struct, sys
from pathlib import Path
import numpy as np
from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent))
import sam_mesh, sam_head
from sam_mesh import Mesh, Region, load_source
from sam_model_details import cap as cap_mesh, hair_skirt, collar, soft_box, dome, dome_rings
from sam_textures import (paint_suit, paint_boots, paint_face, paint_board, skin_tone, packers_decal, rgba_image,
                          buffalo, BLACK, OLIVE, TAN, NAVY, HAIR, CHARCOAL, MUSTARD, gaussian, rasterize)

ROOT = Path(os.environ.get('SSX_ROOT') or Path(__file__).resolve().parents[1])
WEB = ROOT / 'web/public/assets/WARDROBE'
MAC = WEB / 'MAC'
OUT = Path(os.environ.get('SAM_WARDROBE_OUT') or WEB / 'SAM')          # SAM_WARDROBE_OUT: a scratch build
SPEC = Path(os.environ.get('SAM_MODEL_OUT') or ROOT / 'local/sam-model') / 'wardrobe-spec.json'
# Mac's head-worn items (hats, eyewear, ...) go onto Sam's head through the same warp that made his head from Mac's
# HeadA (tools/sam_head.py warp_points), so they sit where they sat on Mac.
HEAD_FIT = 'tools/sam_head.py warp_points (Mac HeadA -> Sam landmark warp)'
HEAD_BONE = (0, 5)

# ---------------------------------------------------------------------------------------------- item design
# Tops (folder 22, class 44): flannel (red, dark, black) + vest, a solid shirt, or the designed outfits.
TOP_DESIGNS = {
    49: dict(outfit='rope_tow_regular'),                  # Midwest Unc
    50: dict(outfit='sunday_unc'),                        # Sunday Unc (Packers)
    51: dict(scheme=dict(shirt=(30, 40, 84), vest=(86, 92, 50))),   # Earn Your Turns: navy sleeves, olive tech vest
    55: dict(outfit='uphill_club'),                       # Uphill Club
    59: dict(outfit='lodge_legend'),                      # Lodge Legend
}
BOTTOMS = {79: (106, 88, 57), 78: (54, 66, 92), 80: (86, 88, 92), 81: (78, 84, 50), 82: (32, 40, 76), 83: (56, 56, 60)}
BOOTS = {139: ((38, 40, 48), (146, 122, 84)), 140: ((120, 34, 30), (30, 30, 32)), 141: ((140, 96, 44), (70, 48, 26))}
GLOVES = {132: ((174, 126, 40), None), 133: ((218, 196, 118), None), 134: ((112, 72, 42), (70, 44, 26))}
HEADS = {87: 'default', 147: 'buzz', 93: 'balaclava_smile', 94: 'balaclava'}
CAPS = {151: dict(crown=NAVY, bill=NAVY, patch=True), 152: dict(palette='mac_hats_a01_a02')}
HAIRS = {143: HAIR}
AFROS = {158: (40, 26, 18), 159: (228, 220, 196)}
SIDE = {156: (74, 44, 26)}
# names, prices and icons of Sam's items: sam_character/gear-names.json (tools/sam_ps2/SamWardrobe.cs -> the export)

# Material names are PS2 SSH image short names (4 characters): stop sbot shed sglv sbtt shr1 shr2 shat sbrd skit safr shlm
# Mac model resource -> Sam resource (None: not drawn on Sam)
MODEL_MAP = {
    'mac_topa.mnf': 'sam_topa.mnf', 'mac_topb.mnf': 'sam_topa.mnf', 'mac_topc.mnf': 'sam_topa.mnf',
    'mac_bottoma.mnf': 'sam_bottomb.mnf', 'mac_bottomb.mnf': 'sam_bottomb.mnf',
    'mac_heada.mnf': 'sam_heada.mnf', 'mac_heada_nis.mnf': 'sam_heada_nis.mnf',
    'mac_headc.mnf': 'sam_heada.mnf', 'mac_headc_nis.mnf': 'sam_heada_nis.mnf',       # balaclavas: Sam's head, knit texture
    'mac_handsa.mnf': 'sam_handsa.mnf', 'mac_handsb.mnf': 'sam_handsa.mnf',
    'mac_handsa_nis.mnf': 'sam_handsa_nis.mnf', 'mac_handsb_nis.mnf': 'sam_handsa_nis.mnf',
    'mac_dummya_nis.mnf': None, 'mac_dummyb_nis.mnf': None, 'mac_eyes_nis.mnf': None,
    'mac_bootsa.mnf': 'sam_bootsa.mnf', 'mac_dangle.mnf': 'sam_hair.mnf',
    'mac_splithair.mnf': 'sam_hair.mnf', 'mac_dangle3.mnf': 'sam_hair.mnf',              # the hair under hats / headbands: Sam's
    'mac_headboltb1.mnf': 'sam_cap.mnf', 'mac_headboltb.mnf': None,                    # Skint = the buzz head texture
    'mac_elephant6.mnf': 'sam_hair_side.mnf', 'mac_antenna3.mnf': 'sam_afro.mnf',
    'mac_headboltb14.mnf': 'sam_helmet.mnf',                                          # Revenger -> Earn Your Turns Helmet
    'mac_headbolth.mnf': None, 'mac_headbolth2.mnf': None, 'mac_headbolth3.mnf': None, 'mac_tshirt.mnf': None,
    'mac_headboltb15.mnf': None,                                                       # hood hair (no Sam hood)
    'mac_backpack.mnf': 'sam_kit_net.mnf', 'mac_backpack2.mnf': 'sam_kit_tube.mnf',
    'board_bindingsa.mnf': 'sam_bindingsa.mnf', 'board_boardflexa.mnf': 'sam_boardflexa.mnf',
    'mac_topd.mnf': 'sam_topd.mnf',
}
HAT_ROWS = (150, 238, 279, 303, 351, 382, 149, 153, 154, 155, 160, 161)   # hat model rows: Sam's hair comes along


def u8(v): return int(max(0, min(255, round(v))))


# ---------------------------------------------------------------------------------------------- palettes
def palette(image, k=4, mask=None):
    """Dominant colours of a texture (median cut), most frequent first; near-transparent texels ignored."""
    a = np.asarray(image.convert('RGBA')).reshape(-1, 4)
    keep = a[:, 3] > 64
    if mask is not None: keep &= mask.reshape(-1)
    px = a[keep, :3]
    if len(px) < 16: px = a[:, :3]
    img = Image.fromarray(px.reshape(-1, 1, 3).astype(np.uint8), 'RGB').quantize(k, method=Image.Quantize.MEDIANCUT)
    pal = np.array(img.getpalette()[:3 * k]).reshape(-1, 3)
    counts = np.bincount(np.asarray(img).reshape(-1), minlength=k)
    return [tuple(int(v) for v in pal[i]) for i in np.argsort(-counts) if counts[i]]


def lum(c): return .299 * c[0] + .587 * c[1] + .114 * c[2]


def scheme_from(colours, vest=True):
    """Flannel + vest from a Mac top's palette: the two most distinct saturated colours check, the third is the vest."""
    cs = sorted(colours, key=lambda c: -(max(c) - min(c)) - .2 * lum(c))
    a = np.array(cs[0], float); b = np.array(cs[1] if len(cs) > 1 else cs[0], float) * .55
    dark = np.minimum(a, b) * .6
    v = np.array(cs[2] if len(cs) > 2 else cs[-1], float)
    v = lum(v) + (v - lum(v)) * .7
    return dict(flannel=(a, a * .55 + b * .45, dark), vest=v if vest else None)


# ---------------------------------------------------------------------------------------------- textures
def mac_texture(stem):   # Mac's wardrobe texture: the loose PNG or its entry in WARDROBE/MAC/textures.tex (tools/export_rider_textures.py)
    import io
    from export_rider_textures import rider_texture_png
    return Image.open(io.BytesIO(rider_texture_png('MAC', stem))).convert('RGBA')


def top_scheme(item, mac_entries):
    """(outfit, scheme) of a Tops item: a designed outfit, or a colourway from the Mac top's palette."""
    d = TOP_DESIGNS.get(item)
    if d and 'outfit' in d: return d['outfit'], None
    if d: return 'custom', d['scheme']
    cols = palette(mac_texture(mac_entries[item][11].replace('$$$', 'a02')), 4)
    if item in (60, 61, 62): return 'custom', scheme_from(cols, vest=False)      # the "II": the same flannel, no vest
    if item == 67: return 'custom', scheme_from(cols, vest=False)                # Plaid Shirt
    if item >= 66:                                                               # tees -> a solid shirt with a stripe
        main = np.array(max(cols, key=lambda c: (max(c) - min(c)) + .3 * lum(c)), float)
        return 'custom', dict(shirt=main * .85, shirt_stripe=np.array(cols[-1], float), vest=None)
    return 'custom', scheme_from(cols)


def paint_top(source, item, mac_entries):
    outfit, scheme = top_scheme(item, mac_entries)
    decal = packers_decal(Image.open(ROOT / 'sam_character/design/wardrobe/sam-wardrobe-options.png')) if outfit == 'sunday_unc' else None
    img = paint_suit(source, outfit, decal, scheme=scheme)
    return collar_swatch(img, outfit, scheme)


def collar_swatch(top_image, outfit, scheme):
    """The collar's swatch in the top map's free lower band (v > .89): the shirt of that top."""
    from sam_textures import RED, RED_DARK
    a = np.asarray(top_image).astype(float)[..., :3].copy()
    h, w = a.shape[:2]; y0 = int(.89 * h)
    vs, us = np.mgrid[y0:h, 0:w] + .5
    if outfit == 'sunday_unc':
        band = np.zeros(us.shape + (3,)) + [30, 32, 32]; band[:, ::4] *= 1.25
    elif outfit == 'lodge_legend': band = np.broadcast_to(OLIVE * 1.05, us.shape + (3,)).copy()
    elif scheme and scheme.get('shirt') is not None: band = np.broadcast_to(np.asarray(scheme['shirt'], float), us.shape + (3,)).copy()
    else:
        if outfit == 'uphill_club': cols = dict(red=np.array([38., 66, 150]), dark=np.array([22., 36, 86]), black=BLACK)
        elif scheme and scheme.get('flannel') is not None: f = scheme['flannel']; cols = dict(red=np.asarray(f[0], float), dark=np.asarray(f[1], float), black=np.asarray(f[2], float))
        else: cols = {}
        band = buffalo(us, vs, 8, **cols)
    a[y0:] = band
    return rgba_image(a)


TOP_COLLAR = {'collar': (0, .90, 1, 1)}


def paint_bottom(source, colour, pocket=True):
    img = paint_suit(source, 'custom', bottom=dict(colour=colour))
    return img


def paint_head(face_art, skin, kind):
    """Sam's head map variants (tools/sam_head.py paint): default, buzz (Skint), balaclava, balaclava_smile."""
    return sam_head.paint(kind)


def paint_hair(colour, size=64):
    rng = np.random.default_rng(5)
    vs, us = np.mgrid[0:size, 0:size] / size
    streak = sum(a * np.sin(2 * np.pi * (us * f + rng.random()) + 2.5 * vs * rng.random()) for f, a in [(9, .10), (23, .07), (47, .05)])
    c = np.asarray(colour, float) * (1.12 + streak - .35 * vs ** 1.5)[..., None]
    return rgba_image(c)


def paint_afro(colour, size=64):
    rng = np.random.default_rng(7)
    vs, us = np.mgrid[0:size, 0:size] / size
    curls = .12 * np.sin(us * 60 + np.sin(vs * 40) * 2) * np.sin(vs * 55 + np.sin(us * 30))
    c = np.asarray(colour, float) * (1 + curls + rng.normal(0, .04, us.shape))[..., None]
    return rgba_image(c)


CAP_MAP = {'cap': (0, 0, 1, .5), 'bill': (0, .5, .5, .75), 'bill_under': (.5, .5, 1, .75), 'patch': (0, .75, .5, 1)}


def paint_cap(crown, bill, patch=False, size=(128, 64)):
    W, H = size
    img = np.zeros((H, W, 3)) + 30
    vs, us = np.mgrid[0:H, 0:W] + .5
    def box(n):
        u0, v0, u1, v1 = CAP_MAP[n]
        return slice(int(v0 * H), int(v1 * H)), slice(int(u0 * W), int(u1 * W))
    sy, sx = box('cap'); lu = (us[sy, sx] - sx.start) / (sx.stop - sx.start); lv = (vs[sy, sx] - sy.start) / (sy.stop - sy.start)
    crown = np.asarray(crown, float)
    c = crown * (1.08 - .18 * lv)[..., None]
    c[np.abs(((lu * 6) % 1) - .5) > .46] = crown * .62
    arch = (np.abs(lu - .5) < .075) & (lv < .46 - 40 * (lu - .5) ** 2)
    c[arch] = crown * .35
    strap = (np.abs(lu - .5) < .095) & (np.abs(lv - .17) < .05)
    c[strap] = [122, 88, 52]
    c[(np.abs(lu - .5) < .014) & (np.abs(lv - .17) < .06)] = [170, 140, 60]
    img[sy, sx] = c
    sy, sx = box('bill'); lv = (vs[sy, sx] - sy.start) / (sy.stop - sy.start)
    img[sy, sx] = np.asarray(bill, float) * (1.05 - .1 * lv)[..., None]
    sy, sx = box('bill_under'); img[sy, sx] = np.asarray(bill, float) * .55
    sy, sx = box('patch'); lu = (us[sy, sx] - sx.start) / (sx.stop - sx.start); lv = (vs[sy, sx] - sy.start) / (sy.stop - sy.start)
    if patch:
        p = np.broadcast_to(np.array([196., 150, 52]), lu.shape + (3,)).copy()
        p[(lu < .08) | (lu > .92) | (lv < .1) | (lv > .9)] = [120, 88, 30]
        pts = [(.14, .72), (.36, .52), (.52, .62), (.84, .30)]
        for (ax, ay), (bx, by) in list(zip(pts, pts[1:])) + [((.84, .30), (.62, .30)), ((.84, .30), (.84, .54))]:
            t = np.clip(((lu - ax) * (bx - ax) + (lv - ay) * (by - ay)) / ((bx - ax) ** 2 + (by - ay) ** 2), 0, 1)
            p[np.hypot(lu - (ax + t * (bx - ax)), lv - (ay + t * (by - ay))) < .055] = [34, 30, 36]
        img[sy, sx] = p
    else:
        img[sy, sx] = crown * 1.05
    return rgba_image(img)


KIT_MAP = {'canvas': (0, 0, .5, .5), 'dark': (.5, 0, 1, .5), 'green': (0, .5, .5, 1), 'wood': (.5, .5, .75, 1), 'amber': (.75, .5, 1, 1)}


def paint_kit(size=64):
    img = np.zeros((size, size, 3))
    for n, col in [('canvas', (118, 102, 70)), ('dark', (34, 36, 36)), ('green', (34, 70, 44)), ('wood', (124, 82, 44)), ('amber', (112, 72, 26))]:
        u0, v0, u1, v1 = KIT_MAP[n]
        img[int(v0 * size):int(v1 * size), int(u0 * size):int(u1 * size)] = col
    return rgba_image(img)


HELMET_MAP = {'shell': (0, 0, 1, .75), 'vent': (0, .75, .5, 1), 'strap': (.5, .75, 1, 1)}


def paint_helmet(size=64):
    img = np.zeros((size, size, 3))
    vs, us = np.mgrid[0:size, 0:size] / size
    shell = np.array([232., 234, 236]) * (1.02 - .12 * vs)[..., None]
    img[:] = shell
    img[int(.75 * size):, :size // 2] = [40, 42, 46]
    img[int(.75 * size):, size // 2:] = [30, 30, 32]
    stripe = (np.abs(us - .5) < .06) & (vs < .75)
    img[stripe] = [200, 70, 40]
    return rgba_image(img)


def paint_board_from(source, k2_ref, mac_board=None, mac_geom=None):
    """K2 layout map: the user's K2 reference (202) or a Mac board graphic resampled onto the K2 outline."""
    img = np.asarray(paint_board(source, k2_ref)).astype(float)
    if mac_board is None: return rgba_image(img[..., :3])
    from sam_textures import BOARD
    size = img.shape[0]
    mb = np.asarray(mac_board.convert('RGB').resize((256, 256), Image.LANCZOS)).astype(float)
    pos, uv, tris, nrm = mac_geom
    # lookup grids (x along the board, z across) of the Mac board UV for the top and base faces
    X, Z = 128, 48
    for island, sign in [('top', 1), ('base', -1)]:
        face = tris[(nrm[tris].mean(1)[:, 1] * sign) > .3]
        xz = np.stack([(pos[:, 0] / 1.93 + .5), (pos[:, 2] / .456 + .5)], -1)
        grid, cover = rasterize(max(X, Z), xz * [1, 1], face, {'uv': uv})
        u0, v0, u1, v1 = BOARD[island]
        px0, px1 = int(round(u0 * size)), int(round(u1 * size))
        cols = np.arange(px0, px1); rows = np.arange(size)
        # K2 texel -> board (x along, z across) in the K2 layout of sam_mesh
        zz = ((cols + .5) / size - u0) / (u1 - u0)            # 0..1 across
        xx = 1 - (rows + .5) / size                           # 0..1 along (nose at v = 0)
        gx = np.clip((xx * (max(X, Z) - 1)).astype(int), 0, max(X, Z) - 1)
        gz = np.clip((zz * (max(X, Z) - 1)).astype(int), 0, max(X, Z) - 1)
        UV = grid['uv'][gz[None, :], gx[:, None]]
        ok = cover[gz[None, :], gx[:, None]]
        su = np.clip(UV[..., 0] * 255, 0, 255).astype(int); sv = np.clip(UV[..., 1] * 255, 0, 255).astype(int)
        sample = mb[sv, su]
        region = img[:, px0:px1, :3]
        region[ok] = sample[ok]
        img[:, px0:px1, :3] = region
    # bindings strip: the Mac board texture's own bindings (its u .785+ strip, same UVs as sam_bindingsa)
    b0 = int(.785 * size)
    strip = np.asarray(mac_board.convert('RGB').resize((size, size), Image.LANCZOS)).astype(float)[:, b0:]
    img[:, b0:, :3] = strip
    return rgba_image(img[..., :3])


# ---------------------------------------------------------------------------------------------- parts
class Parts:
    def __init__(self): self.parts = {}; self.bin = bytearray()

    def add(self, key, name, slot, bones, vertices, indices, skin, batches, extra=None, sha=None):
        v = np.asarray(vertices, '<f4').reshape(-1, 10); ix = np.asarray(indices, '<u4')
        assert len(skin) == len(v) and (ix < len(v)).all() and np.isfinite(v).all()
        def align():
            while len(self.bin) % 4: self.bin.append(0)
        align(); vo = len(self.bin); self.bin += v.tobytes()
        io = len(self.bin); self.bin += ix.tobytes()
        so = len(self.bin)
        for group in skin:
            assert 1 <= len(group) <= 4, group
            rec = bytearray(20); rec[0] = len(group)
            for k, (f, b, wgt) in enumerate(group): struct.pack_into('<BBh', rec, 4 + k * 4, f, b, wgt)
            self.bin += rec
        part = dict(resource=key, name=name, slot=slot, bone_count=len(bones), morph_count=0,
                    source_sha256=sha or hashlib.sha256(v.tobytes() + ix.tobytes()).hexdigest(), vertex_offset=vo,
                    vertex_count=len(v), index_offset=io, index_count=len(ix), skin_offset=so, batches=batches, bones=bones, morphs=[])
        if extra: part.update(extra)
        self.parts[key.lower()] = part


def percent(group):
    """float weights -> integer percent (largest remainder), as compile_sam_skin."""
    tot = sum(w for _, w in group); exact = [w / tot * 100 for _, w in group]; q = [math.floor(e) for e in exact]
    for i in sorted(range(len(group)), key=lambda i: -(exact[i] - q[i]))[:100 - sum(q)]: q[i] += 1
    return [(b, q[i]) for i, (b, _) in enumerate(group) if q[i] > 0]


def mesh_part(parts, key, name, slot, bones, mesh, rig, material_names, extra=None):
    """A Sam Mesh (sam_mesh.Mesh, rig bone indices) as a wardrobe part; batches by texture index -> material name."""
    v = np.array(mesh.vertices, float); ix = np.array(mesh.indices, np.uint32)
    fi = {i: (b['file'], b['index']) for i, b in enumerate(rig['bones'])}
    skin = [[(*fi[b], w) for b, w in percent(g)] for g in mesh.skin]
    batches = []
    for b in mesh.batches:
        m = material_names[b['texture']]
        if batches and batches[-1]['material'] == m and batches[-1]['first'] + batches[-1]['count'] == b['first_index']: batches[-1]['count'] += b['index_count']
        else: batches.append(dict(material=m, first=b['first_index'], count=b['index_count']))
    parts.add(key, name, slot, bones, v, ix, skin, batches, extra)


def mac_part(w_mac, mac_bin, res):
    p = w_mac['parts'][res]
    v = np.frombuffer(mac_bin, '<f4', p['vertex_count'] * 10, p['vertex_offset']).reshape(-1, 10).copy()
    ix = np.frombuffer(mac_bin, '<u4', p['index_count'], p['index_offset']).copy()
    skin = []
    for i in range(p['vertex_count']):
        at = p['skin_offset'] + i * 20; n = mac_bin[at]
        skin.append([struct.unpack_from('<BBh', mac_bin, at + 4 + k * 4) for k in range(n)])
    return p, v, ix, skin


def head_worn(p, skin):
    own = {(b['file'], b['index']) for b in p['bones']}
    return all(all((f, b) == HEAD_BONE or (f, b) in own for f, b, _ in g) for g in skin) and p['slot'] not in (0, 3, 7, 8, 9, 10, 1, 2)


def affine_head_fit(pairs):
    """The warp's least-squares {sx, dy, dz} (x scale, y and z shifts) over the fitted items, for tools/sam_ps2
    SamWardrobeParts.cs, which moves the PS2 LODs of Mac's head-worn items with an affine fit."""
    a = np.concatenate([p for p, _ in pairs]); b = np.concatenate([q for _, q in pairs])
    return dict(sx=round(float((a[:, 0] @ b[:, 0]) / (a[:, 0] @ a[:, 0])), 5), dy=round(float((b[:, 1] - a[:, 1]).mean()), 5), dz=round(float((b[:, 2] - a[:, 2]).mean()), 5))


def fit_head(v):
    v = v.copy()
    v[:, :3] = sam_head.warp_points(v[:, :3])
    return v


# ---------------------------------------------------------------------------------------------- build
def build():
    # tools/export_wardrobe.py --character sam writes the lists/rules; its output is kept as wardrobe.export.json so
    # this tool always starts from it (re-running never stacks its own changes)
    w = json.loads((OUT / 'wardrobe.json').read_text())
    if w.get('parts_mode'): w = json.loads((OUT / 'wardrobe.export.json').read_text())
    else: (OUT / 'wardrobe.export.json').write_text(json.dumps(w, separators=(',', ':')))
    w_mac = json.loads((MAC / 'wardrobe.json').read_text()); mac_bin = (MAC / 'parts.bin').read_bytes()
    mac_entries = {e[0]: e for e in w_mac['entries']}
    rig, original, src_parts, source = load_source()
    bones_of = lambda res: w_mac['parts'][res]['bones']
    body_bones, board_bones = bones_of('mac_topa.mnf'), bones_of('board_bindingsa.mnf')
    parts = Parts(); textures = {}
    (OUT / 'textures').mkdir(parents=True, exist_ok=True)
    for old in (OUT / 'textures').glob('*.png'): old.unlink()

    def tex(stem, image, name, **info):
        image.convert('RGBA').save(OUT / f'textures/{stem}.png')
        textures[stem] = dict(name=name, width=image.width, height=image.height, archive='sam', resource=stem + '.ssh', **info)
        return stem

    def mac_tex(stem):
        if stem in textures: return stem
        from export_rider_textures import rider_texture_png
        (OUT / f'textures/{stem}.png').write_bytes(rider_texture_png('MAC', stem)); textures[stem] = dict(w_mac['textures'][stem]); return stem

    # --- derived body parts (original UVs/weights); one Mesh per resource with texture index 0 = its material
    def derived(name, material, key, part_name, slot, bones, extra_fn=None, extra=None):
        mesh = Mesh(); ix = src_parts[name]; used = np.unique(ix); remap = {int(x): i for i, x in enumerate(used)}
        mesh.add('derived_' + name, original[used, :3], [[remap[int(i)] for i in t] for t in ix], 0, original[used, 6:8], [rig['skin'][i] for i in used], original[used, 3:6])
        if extra_fn: extra_fn(mesh)
        mesh_part(parts, key, part_name, slot, bones, mesh, rig, {0: material}, extra)
    derived('TopB', 'stop', 'sam_topa.mnf', 'TopA_H', 0, body_bones, lambda m: collar(m, Region(0, TOP_COLLAR, 'collar'), sam_head.head()))
    derived('BottomA', 'sbot', 'sam_bottomb.mnf', 'BottomB_H', 3, [])
    derived('BootsA', 'sbtt', 'sam_bootsa.mnf', 'BootsA_H', 10, [])
    derived('HandsA', 'sglv', 'sam_handsa.mnf', 'HandsA_H', 7, [])
    derived('HandsA', 'sglv', 'sam_handsa_nis.mnf', 'HandsA_NIS', 8, [], extra={'hidden': True})
    derived('BindingsA', 'sbrd', 'sam_bindingsa.mnf', 'BindingsA_H', 1, board_bones)
    # head (race + NIS)
    for key, name, slot, extra in [('sam_heada.mnf', 'HeadA_H', 4, None), ('sam_heada_nis.mnf', 'HeadA_NIS', 5, {'hidden': True})]:
        m = Mesh(); sam_head.build(m, 0); mesh_part(parts, key, name, slot, [], m, rig, {0: 'shed'}, extra)
    # hair / hats / helmet
    m = Mesh(); hair_skirt(m, Region(0, {'all': (0, 0, 1, 1)}, 'all'), sam_head.head()); mesh_part(parts, 'sam_hair.mnf', 'Dangle_H', 28, bones_of('mac_dangle.mnf'), m, rig, {0: 'shr1'})
    m = Mesh(); cap_mesh(m, Region(0, CAP_MAP, 'cap'), Region(0, CAP_MAP, 'bill'), Region(0, CAP_MAP, 'patch'), sam_head.head()); mesh_part(parts, 'sam_cap.mnf', 'HeadBoltB1_H', 50, [], m, rig, {0: 'shat'})
    side_bones = bones_of('mac_elephant6.mnf'); afro_bones = bones_of('mac_antenna3.mnf')
    m = Mesh(); sam_side_hair(m, Region(0, {'all': (0, 0, 1, 1)}, 'all')); mesh_part(parts, 'sam_hair_side.mnf', 'Elephant6_H', 25, side_bones, m, rig_with(rig, side_bones), {0: 'shr2'})
    m = Mesh(); sam_afro(m, Region(0, {'all': (0, 0, 1, 1)}, 'all')); mesh_part(parts, 'sam_afro.mnf', 'Antenna3_H', 20, afro_bones, m, rig_with(rig, afro_bones), {0: 'safr'})
    m = Mesh(); sam_helmet(m); mesh_part(parts, 'sam_helmet.mnf', 'HeadBoltB14_H', 50, [], m, rig, {0: 'shlm'})
    # K2 board
    m = Mesh(); sam_k2(m); mesh_part(parts, 'sam_boardflexa.mnf', 'BoardFlexA_H', 2, [], m, rig, {0: 'sbrd'})
    # fishing kits (Backpacks 180/181)
    for key, kit in [('sam_kit_net.mnf', 'net'), ('sam_kit_tube.mnf', 'tube')]:
        m = Mesh(); sam_kit(m, kit, original, src_parts, rig); mesh_part(parts, key, 'Backpack_H', 34, bones_of('mac_backpack.mnf'), m, rig, {0: 'skit'})
    # Ton o' Tunes: Sam's top + the boombox and strap of Mac's mac_topd
    p, v, ix, skin = mac_part(w_mac, mac_bin, 'mac_topd.mnf')
    top = parts.parts['sam_topa.mnf']; tv = np.frombuffer(parts.bin, '<f4', top['vertex_count'] * 10, top['vertex_offset']).reshape(-1, 10).copy()
    tix = np.frombuffer(parts.bin, '<u4', top['index_count'], top['index_offset']).copy(); tskin = read_skin(parts.bin, top)
    keep = [b for b in p['batches'] if b['material'] != 'suit']
    sel = np.concatenate([ix[b['first']:b['first'] + b['count']] for b in keep]); used = np.unique(sel); remap = {int(x): i for i, x in enumerate(used)}
    nv = np.concatenate([tv, v[used]]); nix = list(tix) + [remap[int(i)] + len(tv) for i in sel]
    nskin = tskin + [skin[i] for i in used]
    batches = list(top['batches']); first = len(tix)
    for b in keep: batches.append(dict(material=b['material'], first=first, count=b['count'])); first += b['count']
    parts.add('sam_topd.mnf', 'TopD_H', 0, body_bones, nv, nix, nskin, batches)

    # --- Mac accessory parts (head-worn ones moved to Sam's head)
    spec_models = {}; fit_pairs = []
    for e in w_mac['entries']:
        res = e[10]
        if not res: continue
        if res in MODEL_MAP: spec_models[res] = MODEL_MAP[res]; continue
        key = res.replace('mac_', 'samfit_', 1) if res.startswith('mac_') else res
        if key.lower() in parts.parts: spec_models[res] = key; continue
        p, v, ix, skin = mac_part(w_mac, mac_bin, res)
        # Mac's garment layers (hoodies, tees, collars: only his suit/boot maps) have no Sam counterpart; Sam's own
        # top/boots cover them. A costume's suit/boot batches (e.g. the pumpkin's collar) are left out likewise.
        own = [b for b in p['batches'] if b['material'] not in ('suit', 'boot')]
        if not own: spec_models[res] = None; continue
        p = dict(p, batches=own)
        fitted = head_worn(p, skin)
        if fitted: before = v[:, :3].copy(); v = fit_head(v); fit_pairs.append((before, v[:, :3].copy()))
        else: key = res
        extra = {k: p[k] for k in ('hidden',) if k in p}; extra['source_resource'] = p['resource']
        parts.add(key, p['name'], p['slot'], p['bones'], v, ix, skin, p['batches'], extra, sha=p['source_sha256'] + (':fit' if fitted else ''))
        spec_models[res] = key

    # --- textures per item
    face_art = Image.open(ROOT / 'sam_character/model/textures/face.png'); skin_col = skin_tone(paint_face(face_art))
    k2 = Image.open(ROOT / 'sam_character/design/equipment/k2-alchemist-user-reference.png')
    item_tex = {}
    for item, e in mac_entries.items():
        if e[1] == 44 or item in TOP_DESIGNS:                      # tops (class 44)
            if not e[11]: continue
            img = paint_top(source, item, mac_entries)
            item_tex[item] = tex(f'sam_top_{item}', img, 'stop')
    for item, colour in BOTTOMS.items(): item_tex[item] = tex(f'sam_bot_{item}', paint_bottom(source, colour), 'sbot')
    for item, (boot, sole) in BOOTS.items(): item_tex[item] = tex(f'sam_boot_{item}', paint_boots(source, boot=boot, sole=sole), 'sbtt')
    for item, (glove, palm) in GLOVES.items(): item_tex[item] = tex(f'sam_glove_{item}', paint_boots(source, glove=glove, palm=palm), 'sglv')
    for item, kind in HEADS.items(): item_tex[item] = tex(f'sam_head_{kind}', paint_head(face_art, skin_col, kind), 'shed')
    for item, c in CAPS.items():
        if c.get('palette'):
            pal = palette(mac_texture(c['palette']), 3); img = paint_cap(pal[0], pal[1] if len(pal) > 1 else pal[0])
        else: img = paint_cap(c['crown'], c['bill'], c.get('patch'))
        item_tex[item] = tex(f'sam_cap_{item}', img, 'shat')
    for item, col in HAIRS.items(): item_tex[item] = tex('sam_hair_unc', paint_hair(col), 'shr1')
    for item, col in SIDE.items(): item_tex[item] = tex(f'sam_hairside_{item}', paint_hair(col), 'shr2')
    for item, col in AFROS.items(): item_tex[item] = tex(f'sam_afro_{item}', paint_afro(col), 'safr')
    item_tex[161] = tex('sam_helmet_eyt', paint_helmet(), 'shlm')
    kit = tex('sam_kit', paint_kit(), 'skit'); item_tex[180] = kit; item_tex[181] = kit
    # sport shades for Earn Your Turns: Mac's cop-shade texture recoloured red/orange mirror
    shades = np.asarray(mac_texture('mac_eatc_01')).astype(float)
    l = shades[..., :3] @ [.299, .587, .114]
    shades[..., :3] = np.stack([l * 1.2 + 40, l * .55 + 20, l * .2], -1)
    item_tex[166] = tex('sam_eatc_eyt', rgba_image(np.clip(shades[..., :3], 0, 255)), 'eatc', texel_domain=None)
    # boards: 202 = the K2; every other standard board (190 folder) = its Mac graphic on the K2 outline
    bp, bv, bix, _ = mac_part(w_mac, mac_bin, 'board_boardflexa.mnf')
    geom = (bv[:, :3], bv[:, 6:8], bix.reshape(-1, 3).astype(int), bv[:, 3:6])
    for item, e in mac_entries.items():
        if e[1] == 190 and e[11] and item <= 218:
            img = paint_board_from(source, k2) if item == 202 else paint_board_from(source, k2, mac_texture(e[11].replace('$$$', 'a01')), geom)
            item_tex[item] = tex(f'sam_bord_{item}', img, 'sbrd')

    # --- entries: Mac's model/texture/group/slot with the Sam overrides
    names = {e[0]: e[7] for e in w['entries']}
    out_entries = []
    for e in w['entries']:
        item = e[0]; m = mac_entries.get(item); e = list(e)
        if m:
            model = m[10]; model = spec_models.get(model, model) if model else None
            texture = item_tex.get(item, m[11])
            slot = m[9]; group = m[8]
            if model and model.lower() in parts.parts: slot = parts.parts[model.lower()]['slot']
            if texture and texture not in textures and not texture.startswith('sam_'):
                # Mac texture pattern: copy every stem it can resolve to
                for stem in w_mac['textures']:
                    if match(texture, stem): mac_tex(stem)
            if item in (143, 145, 148): texture, group = item_tex[143], 2   # every hair row carries Sam's hair texture
            if item == 142: texture = None
            e[8], e[9], e[10], e[11] = group, slot, model, texture
        out_entries.append(e)
    w['entries'] = out_entries
    # Sam keeps his hair under hats
    # Sam has no hood: Mac's hood rows (53 Hoodup, 54 HoodDown, 144 HoodupHair) and their rules would swap his cap
    # for a hair style under an undrawn hood, so they are left out
    HOOD = {53, 54, 144}
    rules = [r for r in w['rules'] if not ({r[1], r[2], r[4]} & HOOD)]
    # the Earn Your Turns helmet (was Revenger) brings only Sam's hair, not Revenger's hair piece
    rules = [r for r in rules if r[1] != 161]
    new_rules = [[1, row, -1, 0, 143, 1] for row in HAT_ROWS]
    # ...and on every item that equips a hat row (the PS2 applies an item's own rules; a rule's target's rules are not
    # re-run on a fresh profile, so O'Reilly itself must bring the hair)
    new_rules += [[1, r[1], -1, 0, 143, 1] for r in rules if r[0] == 1 and r[4] in HAT_ROWS and r[5] == 1 and r[1] not in HAT_ROWS and r[1] != 161]
    # other hair styles and the costume heads take Sam's hair off
    new_rules += [[1, item, -1, 0, 143, 0] for item in (146, 156, 158, 159, 97, 99, 102, 106, 108, 110)]
    for r in new_rules:
        if r not in rules: rules.append(r)
    # the original table is ordered by (item, on): the PS2 lookup 0x14DB40 scans a character's rules and stops at the
    # first row after a matching run, so rules appended out of order never ran there; stable, the added ones follow
    rules.sort(key=lambda r: (r[1], r[0]))
    w['rules'] = rules
    missing = sorted({e[10] for e in out_entries if e[10] and e[10].lower() not in parts.parts})
    if missing: raise ValueError(f'models without parts: {missing}')
    w['parts'] = parts.parts; w['parts_bytes'] = len(parts.bin); w['textures'] = textures
    fe = json.loads((ROOT / 'web/public/assets/RIDER_SAM/fe/rider.json').read_text())['fe']; w['fe'] = fe
    w['parts_mode'] = True
    w['sam']['parts_mode'] = True
    (OUT / 'parts.bin').write_bytes(bytes(parts.bin)); (OUT / 'wardrobe.json').write_text(json.dumps(w, separators=(',', ':')))
    # PS2 distance LODs of the Sam-authored parts (tools/sam_lod.py ratios; per material so no batch merges)
    from sam_lod import decimate, LEVELS
    lods = {}
    for key, p in parts.parts.items():
        if not key.startswith('sam_'): continue
        v = np.frombuffer(parts.bin, '<f4', p['vertex_count'] * 10, p['vertex_offset']).reshape(-1, 10).astype(float)
        ix = np.frombuffer(parts.bin, '<u4', p['index_count'], p['index_offset']).reshape(-1, 3).astype(int)
        lods[key] = {}
        for level, ratio in LEVELS:
            out = []
            for b in p['batches']:
                tri = ix[b['first'] // 3:(b['first'] + b['count']) // 3]
                d = decimate(v[:, :3], v[:, 6:8], tri, max(1, int(round(len(tri) * ratio))))
                out.append(dict(material=b['material'], indices=d.ravel().tolist()))
            lods[key][level] = out
    SPEC.parent.mkdir(parents=True, exist_ok=True)
    (SPEC.parent / 'wardrobe-lods.json').write_text(json.dumps(lods, separators=(',', ':')))
    SPEC.write_text(json.dumps(dict(note='tools/sam_wardrobe.py: per item the browser model resource and texture stem (PS2: tools/sam_ps2/SamWardrobe.cs)',
                                    items={str(e[0]): dict(name=e[7], model=e[10], texture=e[11], slot=e[9], group=e[8]) for e in out_entries if e[10] or e[11]},
                                    hat_rows=list(HAT_ROWS), rules=w['rules'],
                                    head_fit=affine_head_fit(fit_pairs), head_warp=HEAD_FIT, fitted=sorted(k for k in parts.parts if k.startswith('samfit_'))), indent=1))
    print(json.dumps(dict(parts=len(parts.parts), textures=len(textures), bytes=len(parts.bin))))


def match(pattern, stem):
    if len(pattern) != len(stem): return False
    return all(p == '$' or p == s for p, s in zip(pattern, stem))


def read_skin(buf, p):
    out = []
    for i in range(p['vertex_count']):
        at = p['skin_offset'] + i * 20; n = buf[at]
        out.append([struct.unpack_from('<BBh', buf, at + 4 + k * 4) for k in range(n)])
    return out


def rig_with(rig, extra_bones):
    """The Sam rig plus a part's own secondary bones (appended; weights to them use indices 26..)."""
    r = dict(rig); r['bones'] = list(rig['bones']) + [dict(file=b['file'], index=b['index']) for b in extra_bones]
    return r


# ---------------------------------------------------------------------------------------------- new geometry
def sam_side_hair(mesh, region):
    """Side Splitter: longer shaggy hair parted at the side, swaying on the two sec_elephant bones (rig 26/27), fitted
    around the head's measured sections (tools/sam_head.py Head)."""
    H = sam_head.head()
    a0, a1 = math.radians(40), math.radians(320); columns = 33; rows = [0, .25, .5, .75, 1.]
    ytop = H.eye[1] + .075
    p = []; uv = []; faces = []; skin = []
    for r, t in enumerate(rows):
        for i in range(columns):
            s = i / (columns - 1); a = a0 + (a1 - a0) * s; back = (1 - math.cos(a)) / 2
            yb = H.lobe[1] - .006 - .080 * back + ((-.014 if i % 2 == 0 else .006) if r == len(rows) - 1 else 0)
            top = ytop - .02 * math.cos(a - .5)
            y = top + (yb - top) * t
            rx0, rz0, cz = H.ring(min(y, H.top - .006), .006, .004, .008)
            flare = t ** 1.2; rx = max(rx0, H.ear_top[0] + .004 if t > .2 else 0) + .030 * flare; rx *= (1 + .05 * t * math.sin(i * 1.7)); rz = rz0 + .026 * flare
            x = rx * math.sin(a); p.append([x, y, cz + rz * math.cos(a)]); uv.append([s, t])
            sway = .55 * t ** 1.3; left = .5 + .5 * max(-1, min(1, x / .04))
            g = [[5, 1 - sway], [26, sway * left], [27, sway * (1 - left)]]
            g = [[b, v] for b, v in g if v > 1e-4]; tot = sum(v for _, v in g); skin.append([[b, v / tot] for b, v in g])
    for r in range(len(rows) - 1):
        for i in range(columns - 1):
            a = r * columns + i; b = a + columns; faces.extend([[a, b, a + 1], [a + 1, b, b + 1]])
    mesh.add('side_hair', p, faces, region, uv, skin)
    # crown with the side part
    rings = []
    for f, k in ((0, 1), (.45, .88), (.8, .55), (1, .06)):
        y = ytop - .02 + (H.top + .014 - ytop + .02) * f; rx, rz, cz = H.ring(min(y, H.top - .004), .007, .005, .009)
        rings.append((y, rx * k, rz * k, cz))
    dome_rings(mesh, 'hair_crown', rings, region, top=[0, H.top + .016, rings[-1][3]])


def sam_afro(mesh, region):
    """Afro Wig: a big rounded curly volume, gently swaying on the two sec_antenna bones (rig 26/27), around the skull."""
    H = sam_head.head()
    rx0, rz0, cz = H.ring(H.eye[1] + .05, 0, 0, 0)
    R = max(rx0, rz0) + .065; cy = H.eye[1] + .075; cz -= .010
    p = []; uv = []; faces = []; skin = []; rings, sides = 10, 24
    for j in range(rings + 1):
        phi = math.pi * .78 * j / rings
        for i in range(sides + 1):
            a = 2 * math.pi * i / sides; bump = 1 + .05 * math.sin(a * 7) * math.sin(phi * 6)
            x = R * math.sin(phi) * math.sin(a) * bump; y = cy + R * .87 * math.cos(phi) * bump; z = cz + R * math.sin(phi) * math.cos(a) * bump
            if math.cos(a) > .55 and phi > .9: y = max(y, H.eye[1] + .022)   # keep the forehead and face open
            p.append([x, y, z]); uv.append([i / sides, j / rings])
            t = j / rings; sw = .3 * t; left = .5 + .5 * max(-1, min(1, x / .06))
            g = [[5, 1 - sw], [26, sw * left], [27, sw * (1 - left)]]; g = [[b, v] for b, v in g if v > 1e-4]; tot = sum(v for _, v in g)
            skin.append([[b, v / tot] for b, v in g])
    for j in range(rings):
        for i in range(sides):
            a = j * (sides + 1) + i; b = a + sides + 1; faces.extend([[a, a + 1, b], [a + 1, b + 1, b]])
    mesh.add('afro', p, faces, region, uv, skin)


def sam_helmet(mesh):
    """Earn Your Turns: a white vented ski helmet with a dark chin strap, on the head bone, around the fitted skull."""
    H = sam_head.head()
    shell = Region(0, HELMET_MAP, 'shell'); strap = Region(0, HELMET_MAP, 'strap')
    yb = H.eye[1] + .030; ytop = H.top + .040
    rx1, rz1, cz1 = H.ring(yb + .035, .020, .014, .022)
    rings = [(yb, *H.ring(yb, .020, .014, .022)), (yb + .035, rx1 * 1.02, rz1 * 1.02, cz1)]
    for f in (.35, .62, .82, .95):
        k = math.sqrt(1 - f ** 2.3); rings.append((yb + .035 + (ytop - yb - .035) * f, rx1 * 1.02 * k, rz1 * 1.02 * k, cz1))
    dome_rings(mesh, 'helmet_shell', rings, shell, top=[0, ytop, cz1])
    rx, rz, cz = H.ring(yb, .020, .014, .022)
    for side in (-1, 1): mesh.tube('helmet_strap', [side * (rx - .004), yb, H.tragus[2] + .006], [side * .05, H.L['menton'][1] + .012, H.L['menton'][2] - .045], .006, strap, 5, 5)


def sam_k2(mesh):
    from sam_textures import BOARD
    SX, SZ = 1.095, 1.2
    stations = [(-.87, .11), (-.84, .16), (-.76, .179), (-.60, .17), (-.35, .149), (0, .14), (.35, .152), (.60, .174), (.76, .19), (.84, .158), (.88, .075)]
    outline = [[-.858 * SX, 0]] + [[x * SX, -z * SZ] for x, z in stations] + [[.889 * SX, 0]] + [[x * SX, z * SZ] for x, z in reversed(stations)] + [[-.858 * SX, 0]]
    perimeter = [[x, .010 + max(0, abs(x) - .74) ** 2 * .9, z] for x, z in outline]
    XMAX, ZMAX = .889 * SX, .19 * SZ
    for top in [True, False]:
        pos = [[0, .010 if top else -.015, 0]] + [[x, y if top else y - .025, z] for x, y, z in perimeter]
        u0, _, u1, _ = BOARD['top' if top else 'base']
        uv = [[u0 + (z / (2 * ZMAX) + .5) * (u1 - u0), .5 - x / (2 * XMAX)] for x, y, z in pos]
        faces = [[0, i + 1, i + 2] if top else [0, i + 2, i + 1] for i in range(len(perimeter) - 1)]
        mesh.add('K2_top' if top else 'K2_base', pos, faces, 0, uv, [[[23, 1.]]] * len(pos))
    wall = []; faces = []; wuv = []
    for x, y, z in perimeter: wall.extend([[x, y, z], [x, y - .025, z]]); wuv.extend([[(BOARD['sidewall'][0] + BOARD['sidewall'][2]) / 2, .5]] * 2)
    for i in range(len(perimeter) - 1): a = i * 2; faces.extend([[a, a + 1, a + 2], [a + 1, a + 3, a + 2]])
    mesh.add('K2_sidewall', wall, faces, 0, wuv, [[[23, 1.]]] * len(wall))


def sam_kit(mesh, kit, original, src_parts, rig):
    from sam_model_details import GarmentSurface
    G = lambda n: Region(0, KIT_MAP, n)
    surface = GarmentSurface(original[:, :3], src_parts['TopB'], original[:, 3:6], rig['skin'])
    soft_box(mesh, 'fly_fishing_chest_pack', [0, .16, .200], [.18, .145, .060], G('canvas'), 3, None, .010)
    soft_box(mesh, 'pack_front_pocket', [0, .15, .235], [.135, .085, .025], G('canvas'), 3, None, .007)
    mesh.tube('pack_zipper', [-.054, .184, .250], [.054, .184, .250], .002, G('dark'))
    for side in [-1, 1]: surface.ribbon(mesh, 'fitted_pack_strap', [(side * (.075 + t * .06), .12 + t * .235) for t in np.linspace(0, 1, 10)], .014, G('dark'), .013)
    if kit == 'tube':
        soft_box(mesh, 'closed_fly_box', [.042, .10, .256], [.075, .051, .018], G('green'))
        mesh.tube('tool_retractor', [.078, .16, .241], [.078, .095, .247], .003, G('dark'))
        mesh.tube('packed_rod_tube', [-.09, -.13, -.182], [.12, .52, -.122], .031, G('dark'), sides=12, cap_ends=True)
        for y, z in [(.08, -.131), (.33, -.114)]: mesh.tube('rod_case_strap', [-.12, y, z], [.10, y + .05, z - .004], .009, G('green'))
    else:
        back_z = lambda y: -.131 + max(0, y - .25) * .2
        for i in range(24):
            a = i * 2 * math.pi / 24; b = (i + 1) * 2 * math.pi / 24; ya, yb = .34 + .16 * math.cos(a), .34 + .16 * math.cos(b)
            mesh.tube('net_frame', [.115 * math.sin(a), ya, back_z(ya)], [.115 * math.sin(b), yb, back_z(yb)], .007, G('wood'))
        mesh.tube('net_handle', [0, .18, back_z(.18)], [0, .04, back_z(.04)], .012, G('wood'))
        for off in [-.08, -.04, 0, .04, .08]:
            span = .16 * math.sqrt(max(0, 1 - (off / .115) ** 2)); mesh.tube('net_mesh', [off, .34 - span, back_z(.34 - span) - .004], [off, .34 + span, back_z(.34 + span) - .004], .0018, G('dark'), sides=4)
        for off in [-.12, -.06, 0, .06, .12]:
            span = .115 * math.sqrt(max(0, 1 - (off / .16) ** 2)); y = .34 + off; mesh.tube('net_mesh', [-span, y, back_z(y) - .005], [span, y, back_z(y) - .005], .0018, G('dark'), sides=4)


if __name__ == '__main__':
    build()
    from export_rider_textures import pack_rider; pack_rider('SAM')   # textures/*.png -> WARDROBE/SAM/textures.tex
