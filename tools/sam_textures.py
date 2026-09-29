#!/usr/bin/env python3
"""Paint Sam's runtime textures at the original SSX 3 rider texture density.

The original riders ship one 256x256 suit map plus 128x128 boot/glove, head,
board/binding and hair/accessory maps (tools/rider_assets.py). Sam's earlier
package used 1254px photographic atlases and a 1536px concept sheet at runtime,
which read as a pasted photo next to the painted roster. This module paints the
same five-map set:

  suit   256  flannel sleeves, olive quilted vest body, tan cargo pants (or the
              Packers jacket / charcoal pants), painted into the derived body's
              own UV layout. Fold shading is the high-pass of the source suit map,
              so the folds keep the original painted look.
  boots  128  dark boots with tan soles, mustard work gloves (source layout).
  face   128  Sam's authored face artwork reduced to runtime density.
  board  128  K2 Alchemist top/base islands from the user's reference, white
              Union-style bindings recoloured from the source binding strip.
  gear   128  cap panels, bill, uphill-arrow patch, skin, hair clumps, knit,
              accessory swatches and the collar flannel.

Colours are chosen for the original byte-domain rider lighting (texel * light
/ 128 + rim), so mid-values sit where the original suits sit rather than at the
bright values of the concept sheet. Needs numpy and Pillow. The painted maps are
derived from the user's rider textures and are written only into the git-ignored
native packages by tools/sam_mesh.py.
"""
import math
import numpy as np
from PIL import Image, ImageFilter

GEAR = {  # (u0, v0, u1, v1) in the 128x128 gear map
    'cap': (0, 0, .5, .25), 'bill': (.5, 0, 1, .125), 'bill_under': (.5, .125, 1, .25),
    'patch': (.5, .25, .75, .5), 'skin': (.75, .25, 1, .5), 'hair': (0, .25, .5, .75),
    'knit': (.5, .5, 1, .75), 'canvas': (0, .75, .25, 1), 'olive': (.25, .75, .5, 1),
    'dark': (.5, .75, .625, .875), 'green': (.625, .75, .75, .875), 'wood': (.75, .75, .875, .875),
    'metal': (.875, .75, 1, .875), 'amber': (.5, .875, .625, 1), 'flannel': (.625, .875, 1, 1),
}
BOARD = {'top': (.0, .0, .36, 1.), 'base': (.38, .0, .74, 1.), 'sidewall': (.75, .0, .78, 1.)}

RED, RED_DARK, BLACK = np.array([158, 27, 31.]), np.array([84, 18, 22.]), np.array([27, 22, 24.])
OLIVE, TAN, NAVY = np.array([86, 90, 47.]), np.array([120, 98, 62.]), np.array([27, 36, 76.])
MUSTARD, BOOT, SOLE = np.array([174, 126, 40.]), np.array([38, 40, 48.]), np.array([146, 122, 84.])
PACKERS_GREEN, CREAM, GOLD, CHARCOAL = np.array([26, 66, 42.]), np.array([218, 206, 170.]), np.array([206, 150, 38.]), np.array([56, 56, 60.])
HAIR = np.array([44, 30, 22.])                 # dark brown, as in Sam's photos (was 66, 40, 24: read light in the FE)


def rgba_image(rgb, alpha=None):
    rgb = np.clip(np.round(rgb), 0, 255).astype(np.uint8)
    a = np.full(rgb.shape[:2], 255, np.uint8) if alpha is None else alpha.astype(np.uint8)
    return Image.fromarray(np.dstack([rgb, a]), 'RGBA')


def luminance(rgb):
    return rgb[..., 0] * .299 + rgb[..., 1] * .587 + rgb[..., 2] * .114


def gaussian(image, sigma):
    """Separable Gaussian blur with wrap-around edges (the maps tile)."""
    r = int(math.ceil(sigma * 3))
    k = np.exp(-.5 * (np.arange(-r, r + 1) / sigma) ** 2); k /= k.sum()
    out = sum(w * np.roll(image, i - r, 0) for i, w in enumerate(k))
    return sum(w * np.roll(out, i - r, 1) for i, w in enumerate(k))


def fold_shading(rgb, radius, strength=.85, low=.62, high=1.3):
    """High-pass of the source painting: folds and creases without its colour blocks."""
    lum = luminance(rgb) + 6
    blur = gaussian(lum, radius)
    ratio = np.clip(lum / np.maximum(blur, 1), low, high)
    return 1 + (ratio - 1) * strength


def rasterize(size, uv, triangles, attributes):
    """Per-texel attribute maps (barycentric interpolation) plus a coverage mask."""
    maps = {k: np.zeros((size, size, v.shape[1])) for k, v in attributes.items()}
    mask = np.zeros((size, size), bool)
    for tri in triangles:
        t = uv[tri] * size - .5
        x0, y0 = np.floor(t.min(0)).astype(int); x1, y1 = np.ceil(t.max(0)).astype(int)
        x0, y0 = max(x0, 0), max(y0, 0); x1, y1 = min(x1, size - 1), min(y1, size - 1)
        if x1 < x0 or y1 < y0: continue
        xs, ys = np.meshgrid(np.arange(x0, x1 + 1), np.arange(y0, y1 + 1))
        a, b, c = t
        det = (b[1] - c[1]) * (a[0] - c[0]) + (c[0] - b[0]) * (a[1] - c[1])
        if abs(det) < 1e-12: continue
        w0 = ((b[1] - c[1]) * (xs - c[0]) + (c[0] - b[0]) * (ys - c[1])) / det
        w1 = ((c[1] - a[1]) * (xs - c[0]) + (a[0] - c[0]) * (ys - c[1])) / det
        w2 = 1 - w0 - w1
        inside = (w0 >= -.02) & (w1 >= -.02) & (w2 >= -.02)
        if not inside.any(): continue
        yy, xx = ys[inside], xs[inside]
        for k, v in attributes.items():
            maps[k][yy, xx] = w0[inside, None] * v[tri[0]] + w1[inside, None] * v[tri[1]] + w2[inside, None] * v[tri[2]]
        mask[yy, xx] = True
    return maps, mask


def dilate(maps, mask, steps=6):
    """Bleed island attributes outward so bilinear/mip sampling never reaches unpainted texels."""
    mask = mask.copy()
    for _ in range(steps):
        grown = mask.copy()
        for dy, dx in [(0, 1), (0, -1), (1, 0), (-1, 0)]:
            shifted = np.roll(np.roll(mask, dy, 0), dx, 1)
            # no wrap-around: islands never bleed across the map edge
            if dy == 1: shifted[0] = False
            if dy == -1: shifted[-1] = False
            if dx == 1: shifted[:, 0] = False
            if dx == -1: shifted[:, -1] = False
            src = shifted & ~grown
            for m in maps.values():
                m[src] = np.roll(np.roll(m, dy, 0), dx, 1)[src]
            grown |= src
        mask = grown
    return mask


def buffalo(u, v, period, red=RED, dark=RED_DARK, black=BLACK):
    """Buffalo check in texel units, 2x2 supersampled so the check edges stay soft."""
    out = np.zeros(u.shape + (3,))
    for du in (-.25, .25):
        for dv in (-.25, .25):
            a = ((u + du) / period) % 1 < .5; b = ((v + dv) / period) % 1 < .5
            out += np.where((a & b)[..., None], black, np.where((a ^ b)[..., None], dark, red)) / 4
    # faint twill so the flannel reads as cloth at close range
    return out * (1 - .05 * (((u + v) // 1) % 2))[..., None]


def smoothstep(a, b, x):
    t = np.clip((x - a) / (b - a), 0, 1)
    return t * t * (3 - 2 * t)


def part_maps(source, part, size):
    uv, tris, pos, nrm = source['uv'], source['parts'][part], source['pos'], source['nrm']
    return rasterize(size, uv, tris, {'p': pos, 'n': nrm})


def paint_suit(source, outfit, decal=None, scheme=None, bottom=None):
    """256 suit map in the derived body's UV layout (TopB + BottomA).

    `scheme` (Equip Gear top colourways, tools/sam_wardrobe.py): dict(flannel=(red, dark, black) or None,
    shirt=colour for a solid shirt instead of flannel, vest=colour or None (no vest)). `bottom`: dict(colour,
    pocket) for the trousers."""
    size = 256
    base = source['textures'][0].astype(float)[..., :3]
    shade = fold_shading(base, 5)
    # The source jacket carries a small printed chest label; a 5x5 median removes such thin print
    # while keeping the broad fold shading used under the vest.
    smooth = np.asarray(Image.fromarray(base.astype(np.uint8)).filter(ImageFilter.MedianFilter(5))).astype(float)
    vest_shade = fold_shading(smooth, 5)
    rgb = base.copy()
    vs, us = np.mgrid[0:size, 0:size] + .5
    for part in ['TopB', 'BottomA']:
        maps, mask = part_maps(source, part, size)
        mask = dilate(maps, mask, 4)
        p, n = maps['p'], maps['n']
        x, y, z = p[..., 0], p[..., 1], p[..., 2]
        if part == 'TopB':
            # Sleeves live in the left island column, the torso front/back to its right.
            sleeve = us < size * .245
            if outfit == 'sunday_unc':
                paint = np.broadcast_to(PACKERS_GREEN, rgb.shape).copy()
                side = np.sign(x) + (x == 0)
                origin = np.stack([side * .159, np.full_like(x, .340), np.full_like(x, .017)], -1)
                axis = np.stack([side * .171, np.full_like(x, -.198), np.full_like(x, -.007)], -1)
                axis /= np.linalg.norm(axis, axis=-1, keepdims=True)
                d = ((p - origin) * axis).sum(-1)
                for lo, hi, col in [(.100, .126, CREAM), (.126, .152, GOLD), (.152, .178, CREAM)]:
                    band = sleeve & (d >= lo) & (d < hi)
                    paint[band] = col
                cuff = sleeve & (d > .40)
                paint[cuff] = np.where((((d[cuff] - .40) / .012) % 2 < 1)[..., None], CREAM, PACKERS_GREEN * .8)
                hem = ~sleeve & (y < -.28)
                paint[hem] = np.where((((y[hem] + .28) / -.012) % 2 < 1)[..., None], CREAM * .92, GOLD * .9)
                collar = ~sleeve & (y > .37)
                paint[collar] = np.where((((y[collar] - .37) / .01) % 2 < 1)[..., None], CREAM * .9, PACKERS_GREEN * .7)
                placket = ~sleeve & (z > 0) & (np.abs(x) < .012) & (y > -.28)
                paint[placket] = PACKERS_GREEN * .72
                snaps = placket & ((((y + .17) / .082) % 1 - .5) ** 2 * .082 ** 2 + x ** 2 < .0055 ** 2)
                paint[snaps] = np.array([150, 156, 150.])
                if decal is not None:
                    # oval G + PACKERS lettering on the left chest, projected along +z
                    h, w = decal.shape[:2]
                    cx, cy, sx, sy = .094, .25, .077, .085
                    du = (x - cx) / sx + .5; dv = .5 - (y - cy) / sy
                    on = ~sleeve & (z > .05) & (du >= 0) & (du < 1) & (dv >= 0) & (dv < 1)
                    samp = decal[np.clip((dv[on] * h).astype(int), 0, h - 1), np.clip((du[on] * w).astype(int), 0, w - 1)].astype(float)
                    strength = samp[:, 3:4] / 255
                    paint[on] = paint[on] * (1 - strength) + samp[:, :3] * strength
            elif outfit == 'lodge_legend':
                # Lodge Legend (character bible): a sand insulated jacket, quilted all over, with an olive
                # yoke, cuffs and hem, a zipper and chest pockets
                SAND, OLV = np.array([150., 128, 88]), OLIVE * 1.05
                channel = (y + .30) / .10
                phase = channel % 1
                puff = .88 + .18 * np.sin(np.pi * phase) ** .6
                seam = np.exp(-(phase * 9) ** 2) + np.exp(-((phase - 1) * 9) ** 2)
                paint = SAND * (puff - .22 * seam)[..., None]
                side = np.sign(x) + (x == 0)
                origin = np.stack([side * .159, np.full_like(x, .340), np.full_like(x, .017)], -1)
                axis = np.stack([side * .171, np.full_like(x, -.198), np.full_like(x, -.007)], -1)
                axis /= np.linalg.norm(axis, axis=-1, keepdims=True)
                d = ((p - origin) * axis).sum(-1)
                olive_part = (~sleeve & (y > .27)) | (sleeve & (d > .40)) | (~sleeve & (y < -.27)) | (sleeve & (d < .06))
                paint = np.where(olive_part[..., None], OLV * (puff - .15 * seam)[..., None], paint)
                zipper = ~sleeve & (z > 0) & (np.abs(x) < .006)
                paint = np.where(zipper[..., None], np.array([60., 58, 40]), paint)
                pocket = ~sleeve & (z > 0) & (np.abs(np.abs(x) - .10) < .04) & (np.abs(y - .17) < .035) & ((np.abs(np.abs(x) - .10) > .032) | (np.abs(y - .17) > .027))
                paint = np.where(pocket[..., None], OLV * .8, paint)
            else:
                # Uphill Club (PS2 wardrobe variant): cobalt/black flannel under a burnt-orange vest
                cols = dict(red=np.array([38., 66, 150]), dark=np.array([22., 36, 86]), black=BLACK) if outfit == 'uphill_club' else {}
                vest_col = np.array([168., 78, 26]) if outfit == 'uphill_club' else OLIVE
                if scheme:
                    if scheme.get('flannel') is not None:
                        f = [np.asarray(c, float) for c in scheme['flannel']]; cols = dict(red=f[0], dark=f[1], black=f[2])
                    vest_col = None if scheme.get('vest') is None else np.asarray(scheme['vest'], float)
                plaid = buffalo(us, vs, 13, **cols)
                torso_plaid = buffalo(us, vs, 15, **cols)
                if scheme and scheme.get('shirt') is not None:
                    shirt = np.asarray(scheme['shirt'], float)
                    plaid = np.broadcast_to(shirt, plaid.shape).copy(); torso_plaid = plaid.copy()
                    if scheme.get('shirt_stripe') is not None:   # a contrast chest stripe (tees)
                        stripe = (np.abs(y - .21) < .03)
                        torso_plaid[stripe] = np.asarray(scheme['shirt_stripe'], float)
                vest = ~sleeve & (np.abs(x) < .19) & (y > -.19) & (y < .37)
                paint = np.where(sleeve[..., None], plaid, torso_plaid)
                # olive quilted vest: five horizontal channels, sewn seams and a zipper
                channel = (y + .19) / .112
                phase = channel % 1
                puff = .86 + .22 * np.sin(np.pi * phase) ** .6
                seam = np.exp(-((phase - .0) * 9) ** 2) + np.exp(-((phase - 1) * 9) ** 2)
                vc = vest_col if vest_col is not None else OLIVE
                olive = vc * (puff - .28 * seam)[..., None]
                edge = (np.abs(np.abs(x) - .19) < .012) | (np.abs(y - .37) < .012) | (np.abs(y + .19) < .012)
                olive = np.where(edge[..., None], vc * .7, olive)
                zipper = (z > 0) & (np.abs(x) < .006)
                olive = np.where(zipper[..., None], np.array([48., 50, 30]), olive)
                teeth = (z > 0) & (np.abs(x) < .0025) & (((y * 400) % 2) < 1)
                olive = np.where(teeth[..., None], np.array([120., 124, 112]), olive)
                pocket = (z > 0) & (np.abs(np.abs(x) - .11) < .045) & (np.abs(y + .09) < .045) & (np.abs(np.abs(np.abs(x) - .11) - .045) < .006)
                olive = np.where(pocket[..., None], vc * .66, olive)
                if vest_col is not None: paint = np.where(vest[..., None], olive, paint)
            shading = np.where(sleeve, shade, vest_shade ** .6)
        else:
            col = CHARCOAL if outfit == 'sunday_unc' else TAN
            if bottom: col = np.asarray(bottom['colour'], float)
            paint = np.broadcast_to(col, rgb.shape).copy()
            # cargo pockets on the outer thighs with a flap line
            outer = n[..., 0] * np.sign(x) > .45
            px, py = z - .02, y + .40
            inside = outer & (np.abs(px) < .07) & (np.abs(py) < .09)
            border = inside & ((np.abs(np.abs(px) - .07) < .008) | (np.abs(np.abs(py) - .09) < .008) | (np.abs(py - .05) < .006))
            paint[inside] = col * 1.06
            paint[border] = col * .66
            waist = y > -.06
            paint[waist] = col * .82
            shading = shade
        paint = paint * shading[..., None]
        rgb[mask] = paint[mask]
    return rgba_image(rgb)


def paint_boots(source, boot=None, sole=None, glove=None, palm=None):
    """128 boot (upper half) and glove (lower half) map in the source layout (colours: Equip Gear variants)."""
    BOOT_, SOLE_ = (np.asarray(boot, float) if boot is not None else BOOT), (np.asarray(sole, float) if sole is not None else SOLE)
    GLOVE_ = np.asarray(glove, float) if glove is not None else MUSTARD
    PALM_ = np.asarray(palm, float) if palm is not None else MUSTARD * .55 + BLACK * .45
    size = 128
    base = source['textures'][1].astype(float)[..., :3]
    shade = fold_shading(base, 3, .9)
    rgb = base.copy()
    maps, mask = part_maps(source, 'BootsA', size)
    mask = dilate(maps, mask, 3)
    y = maps['p'][..., 1]
    boot = np.where((y < -1.05)[..., None], SOLE_, BOOT_)
    trim = (y > -1.06) & (y < -1.045)
    boot[trim] = SOLE_ * .6
    rgb[mask] = (boot * shade[..., None])[mask]
    maps, mask = part_maps(source, 'HandsA', size)
    mask = dilate(maps, mask, 3)
    n = maps['n']
    glove = np.broadcast_to(GLOVE_, rgb.shape).copy()
    palm = n[..., 1] < -.55
    glove[palm] = PALM_
    glove = glove * (fold_shading(base, 1.5, .5, .72, 1.22) * (.9 + .1 * np.clip(n[..., 1] + .5, 0, 1)))[..., None]
    rgb[mask] = glove[mask]
    # unused lower-half background: keep a neutral dark so filtering never pulls colour
    return rgba_image(rgb)


def paint_face(face_image, size=128):
    """Authored face artwork reduced to the roster's head-texture density, lightly posterised."""
    img = face_image.convert('RGB').resize((size * 2, size * 2), Image.LANCZOS).filter(ImageFilter.MedianFilter(3))
    img = img.resize((size, size), Image.LANCZOS)
    rgb = np.asarray(img).astype(float)
    lum = luminance(rgb)[..., None]
    rgb = lum + (rgb - lum) * .82          # the painted roster skin is less saturated than the photo
    rgb = rgb * .9 + 6
    return rgba_image(rgb)


def skin_tone(face_rgba):
    rgb = np.asarray(face_rgba).astype(float)[..., :3]
    ring = np.concatenate([rgb[4:12, 10:-10].reshape(-1, 3), rgb[40:90, 2:8].reshape(-1, 3), rgb[40:90, -8:-2].reshape(-1, 3)])
    return np.median(ring, 0)


def paint_board(source, reference):
    """K2 Alchemist top/base islands plus recoloured source bindings (u .79-1)."""
    size = 128
    rgb = np.zeros((size, size, 3)) + 20
    ref = reference.convert('RGBA')
    w, h = ref.size
    for island, (xlo, xhi) in [('top', (.308, .507)), ('base', (.524, .737))]:
        u0, v0, u1, v1 = BOARD[island]
        crop = ref.crop((int(xlo * w), int(.066 * h), int(xhi * w), int(.936 * h)))
        px0, px1 = int(round(u0 * size)), int(round(u1 * size))
        crop = crop.resize((px1 - px0, size), Image.LANCZOS)
        arr = np.asarray(crop).astype(float)
        a = arr[..., 3:4] / 255
        fill = np.array([14., 16, 16]) if island == 'top' else np.array([10., 60, 45])
        rgb[:, px0:px1] = arr[..., :3] * a + fill * (1 - a)
    # dark sidewall strip
    s0, s1 = int(BOARD['sidewall'][0] * size), int(round(BOARD['sidewall'][2] * size))
    rgb[:, s0:s1] = [30, 30, 32]
    # bindings: the source strip, reds/oranges to dark grey, plastics pushed to white
    src = source['textures'][3].astype(float)[..., :3]
    b0 = int(.785 * size)
    strip = src[:, b0:]
    lum = luminance(strip)[..., None]
    sat = strip.max(-1, keepdims=True) - strip.min(-1, keepdims=True)
    white = np.clip(lum * 1.08 + 18, 0, 236) * np.array([.98, .99, 1.0])
    grey = np.clip(lum * .45 + 30, 0, 90) * np.array([1, 1, 1.04])
    rgb[:, b0:] = np.where(sat > 60, grey, white)
    return rgba_image(rgb)


def paint_gear(skin, outfit):
    size = 128
    rng = np.random.default_rng(3)
    rgb = np.zeros((size, size, 3)) + 30
    vs, us = np.mgrid[0:size, 0:size] + .5

    def box(name):
        u0, v0, u1, v1 = GEAR[name]
        return slice(int(v0 * size), int(v1 * size)), slice(int(u0 * size), int(u1 * size))

    def local(name):
        sy, sx = box(name)
        return (us[sy, sx] - sx.start) / (sx.stop - sx.start), (vs[sy, sx] - sy.start) / (sy.stop - sy.start)

    # cap crown: six panels (u around), worn toward the top
    lu, lv = local('cap')
    seam = np.abs(((lu * 6) % 1) - .5) > .46
    crown = NAVY * (1.08 - .18 * lv)[..., None] * (1 + rng.normal(0, .025, lu.shape))[..., None]
    crown[seam] = NAVY * .62
    # back of the cap (u ~ .5): the strapback opening arch over a tan leather strap with a brass buckle,
    # and a stitched sweatband line around the lower edge, like the originals' painted cap sides/backs
    arch = (np.abs(lu - .5) < .075) & (lv < .46 - 40 * (lu - .5) ** 2)
    crown[arch] = NAVY * .35
    strap = (np.abs(lu - .5) < .095) & (np.abs(lv - .17) < .05)
    crown[strap] = np.array([122., 88, 52])
    crown[(np.abs(lu - .5) < .014) & (np.abs(lv - .17) < .06)] = np.array([170., 140, 60])
    crown[(np.abs(lv - .26) < .015) & ~strap & ~arch & (((lu * 60) % 1) < .55)] = NAVY * 1.35
    rgb[box('cap')] = crown
    lu, lv = local('bill')
    bill = NAVY * (1.05 - .1 * lv)[..., None]
    stitch = (np.abs(lv - .78) < .06) & ((lu * 40) % 1 < .6)
    bill[stitch] = NAVY * 1.5
    rgb[box('bill')] = bill
    rgb[box('bill_under')] = NAVY * .55
    # the yellow uphill-arrow patch
    lu, lv = local('patch')
    patch = np.broadcast_to(np.array([196., 150, 52]), lu.shape + (3,)).copy()
    border = (lu < .08) | (lu > .92) | (lv < .1) | (lv > .9)
    patch[border] = [120, 88, 30]
    pts = [(.14, .72), (.36, .52), (.52, .62), (.84, .30)]
    for (ax, ay), (bx, by) in zip(pts, pts[1:]):
        t = np.clip(((lu - ax) * (bx - ax) + (lv - ay) * (by - ay)) / ((bx - ax) ** 2 + (by - ay) ** 2), 0, 1)
        dist = np.hypot(lu - (ax + t * (bx - ax)), lv - (ay + t * (by - ay)))
        patch[dist < .055] = [34, 30, 36]
    for hx, hy in [(.62, .30), (.84, .54)]:
        t = np.clip(((lu - .84) * (hx - .84) + (lv - .30) * (hy - .30)) / ((hx - .84) ** 2 + (hy - .30) ** 2), 0, 1)
        dist = np.hypot(lu - (.84 + t * (hx - .84)), lv - (.30 + t * (hy - .30)))
        patch[dist < .055] = [34, 30, 36]
    rgb[box('patch')] = patch
    lu, lv = local('skin')
    rgb[box('skin')] = skin * (1.02 - .1 * lv)[..., None]
    # hair: solid painted clumps, strands running down v, darker toward the tips
    lu, lv = local('hair')
    streak = np.zeros_like(lu)
    for f, a in [(9, .10), (23, .07), (47, .05)]:
        streak += a * np.sin(2 * np.pi * (lu * f + rng.random()) + 2.5 * lv * rng.random())
    hair = HAIR * (1.12 + streak - .35 * lv ** 1.5)[..., None]
    rgb[box('hair')] = hair
    lu, lv = local('knit')
    knit_col = np.array([30., 32, 32])
    rgb[box('knit')] = knit_col * (1 + .25 * (((lu * 24) % 1) < .5))[..., None]
    for name, col in [('canvas', [118, 102, 70]), ('olive', OLIVE), ('dark', [34, 36, 36]), ('green', [34, 70, 44]),
                      ('wood', [124, 82, 44]), ('metal', [128, 134, 128])]:
        lu, lv = local(name)
        rgb[box(name)] = np.array(col, float) * (1.04 - .1 * lv)[..., None]
    lu, lv = local('amber')
    rgb[box('amber')] = np.array([112., 72, 26]) * (.62 + .45 * (1 - lv))[..., None]
    lu, lv = local('flannel')
    sy, sx = box('flannel')
    rgb[sy, sx] = buffalo(us[sy, sx], vs[sy, sx], 8)
    return rgba_image(rgb)


def region_uv(table, name, local_uv):
    u0, v0, u1, v1 = table[name]
    local_uv = np.asarray(local_uv, float)
    # keep a half-texel margin so filtering stays inside the swatch
    pad = .5 / 128
    return np.column_stack([u0 + pad + np.clip(local_uv[:, 0], 0, 1) * (u1 - u0 - 2 * pad),
                            v0 + pad + np.clip(local_uv[:, 1], 0, 1) * (v1 - v0 - 2 * pad)])


def packers_decal(sheet):
    """PACKERS lettering and oval G from the wardrobe sheet's chest crop; the jacket green keyed out."""
    w, h = sheet.size
    crop = sheet.convert('RGB').crop((int(.164 * w), int(.248 * h), int(.207 * w), int(.321 * h))).resize((48, 80), Image.LANCZOS)
    arr = np.asarray(crop).astype(float)
    greenish = (arr[..., 1] > arr[..., 0] + 8) & (arr[..., 1] >= arr[..., 2])
    bg = np.median(arr[greenish], 0)
    dist = np.linalg.norm(arr - bg, axis=-1)
    alpha = np.clip((dist - 40) * 6, 0, 255)
    return np.dstack([arr, alpha])
