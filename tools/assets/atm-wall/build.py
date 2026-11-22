# Builds the 1989 wall ATM (tools/assets/atm-wall/kit.json) in headless Blender:
#   blender --background --factory-startup --python build.py -- <job.json>
# job.json = { kit, root, out, blender_lib }. Writes into <out>:
#   ATMWall.fbx            LOD0..LOD2 and the UBX_ box colliders
#   tex/<Mat>_BCR.tif, tex/<Mat>_NMO.tif   Enfusion channel packing (the same writer as the bank vault kit)
#   review/*.png           renders an agent reads
#   report.json            models, materials, colliders, triangles, bounds
#
# The machine is plain geometry: boxes and a few wedges. Everything with text or a drawn control (the
# bank sign, the screen, the keypad, the side keys, the card and ticket slots, the cash slot) is one
# atlas, ATM_Face, painted here with a 5x7 pixel font; the beige paint, brushed steel and black plastic
# are tiling materials. Space: build in Blender axes (X right, Y forward, Z up); B(x, y, z) takes
# Enfusion axes (x right, y up, z forward). The FBX export lands Blender (X, Y, Z) on Enfusion (-x, z, y).
import bpy, bmesh, sys, json, os, math, struct
import numpy as np
from mathutils import Vector, Matrix

job = json.load(open(sys.argv[sys.argv.index("--") + 1]))
kit, root, out = job["kit"], job["root"], job["out"]
L = kit["layout_m"]
os.makedirs(os.path.join(out, "tex"), exist_ok=True)
os.makedirs(os.path.join(out, "review"), exist_ok=True)
report = {"models": {}, "materials": {}, "notes": []}
rng = np.random.default_rng(1989)
BACK = L["back_z"]


def B(x, y, z):
    # x is negated: the engine import shows the model mirrored left to right (the first build read
    # BANQUE backwards in the world), and (-x, z, y) is a rotation of Enfusion space, not a reflection.
    return Vector((-x, z, y))


def b2e(v):
    return [round(-float(v[0]), 4), round(float(v[2]), 4), round(float(v[1]), 4)]


# ================================================================ textures
def write_tiff(path, rgba):
    # Baseline uncompressed 8-bit RGBA TIFF with unassociated alpha (the same writer as clean.py).
    h, w, _ = rgba.shape
    data = rgba.tobytes()
    tags = [(256, 4, 1, w), (257, 4, 1, h), (258, 3, 4, None), (259, 3, 1, 1), (262, 3, 1, 2), (273, 4, 1, None),
            (277, 3, 1, 4), (278, 4, 1, h), (279, 4, 1, len(data)), (284, 3, 1, 1), (338, 3, 1, 2)]
    ifd_at = 8
    bits_at = ifd_at + 2 + len(tags) * 12 + 4
    data_at = bits_at + 8
    with open(path, "wb") as f:
        f.write(struct.pack("<2sHI", b"II", 42, ifd_at))
        f.write(struct.pack("<H", len(tags)))
        for tag, typ, count, value in tags:
            if tag == 258:
                value = bits_at
            if tag == 273:
                value = data_at
            if typ == 3 and count == 1:
                f.write(struct.pack("<HHIHH", tag, typ, count, value, 0))
            else:
                f.write(struct.pack("<HHII", tag, typ, count, value))
        f.write(struct.pack("<I", 0))
        f.write(struct.pack("<4H", 8, 8, 8, 8))
        f.write(data)


def noise(h, w, sx, sy=None):
    # Periodic (tileable) gaussian noise, zero mean, unit deviation; sx, sy in pixels.
    sy = sx if sy is None else sy
    g0 = rng.standard_normal((h, w))
    fy = np.fft.fftfreq(h)[:, None]
    fx = np.fft.fftfreq(w)[None, :]
    g = np.exp(-2 * (np.pi ** 2) * ((fx * sx) ** 2 + (fy * sy) ** 2))
    n = np.real(np.fft.ifft2(np.fft.fft2(g0) * g))
    return (n - n.mean()) / (n.std() + 1e-9)


def blur(a, s):
    # Gaussian blur of a 2D array (periodic), sigma s pixels.
    h, w = a.shape
    fy = np.fft.fftfreq(h)[:, None]
    fx = np.fft.fftfreq(w)[None, :]
    return np.real(np.fft.ifft2(np.fft.fft2(a) * np.exp(-2 * (np.pi ** 2) * (s ** 2) * (fx ** 2 + fy ** 2))))


def normal_from_height(hf, strength):
    # OpenGL-style normal map in 0..1 from a periodic height field (rows run down the image).
    dx = (np.roll(hf, -1, 1) - np.roll(hf, 1, 1)) * 0.5 * strength
    dy = (np.roll(hf, 1, 0) - np.roll(hf, -1, 0)) * 0.5 * strength
    n = np.dstack([-dx, -dy, np.ones_like(hf)])
    n /= np.linalg.norm(n, axis=2, keepdims=True)
    return n * 0.5 + 0.5


def to_linear(c):
    # Workbench's colour map import treats the TIFF as linear and encodes it to sRGB, so an sRGB-authored
    # colour comes out about twice as light in game (the annex vault, 2026-11-14): decode it here.
    c = np.clip(c, 0, 1)
    return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)


TEX = {}  # material name -> {"base_png": path, "tile_m": metres per texture repeat}


def save_material(name, base, rough, metal, normal, tile_m, em=None):
    # em: the emitted light colour per pixel (sRGB), written as <name>_EM.tif for the emat's EmissiveMap.
    h, w = base.shape[:2]
    bcr = np.dstack([to_linear(base[:, :, :3]), rough])
    nmo = np.dstack([normal[:, :, 0], 1.0 - normal[:, :, 1], metal, np.ones((h, w))])
    write_tiff(os.path.join(out, "tex", name + "_BCR.tif"), (np.clip(bcr, 0, 1) * 255 + 0.5).astype(np.uint8))
    write_tiff(os.path.join(out, "tex", name + "_NMO.tif"), (np.clip(nmo, 0, 1) * 255 + 0.5).astype(np.uint8))
    if em is not None:
        emr = np.dstack([to_linear(em), np.ones((h, w))])
        write_tiff(os.path.join(out, "tex", name + "_EM.tif"), (np.clip(emr, 0, 1) * 255 + 0.5).astype(np.uint8))
    png = os.path.join(out, "tex", name + "_base.png")
    img = bpy.data.images.new(name + "_base", w, h)
    img.pixels.foreach_set(np.dstack([np.clip(base[:, :, :3], 0, 1), np.ones((h, w))])[::-1].astype(np.float32).ravel())
    img.filepath_raw = png
    img.file_format = "PNG"
    img.save()
    TEX[name] = {"base_png": png, "tile_m": tile_m}
    report["materials"][name] = {"px": [w, h], "tile_m": tile_m, "mean_base": [round(float(v), 3) for v in base[:, :, :3].mean(axis=(0, 1))],
                                 "mean_rough": round(float(rough.mean()), 3), "mean_metal": round(float(metal.mean()), 3)}


# ---------------------------------------------------------------- a 5x7 pixel font (uppercase, digits, a few marks)
FONT = {
    "A": "01110 10001 10001 11111 10001 10001 10001", "B": "11110 10001 10001 11110 10001 10001 11110",
    "C": "01110 10001 10000 10000 10000 10001 01110", "D": "11110 10001 10001 10001 10001 10001 11110",
    "E": "11111 10000 10000 11110 10000 10000 11111", "F": "11111 10000 10000 11110 10000 10000 10000",
    "G": "01110 10001 10000 10111 10001 10001 01111", "H": "10001 10001 10001 11111 10001 10001 10001",
    "I": "01110 00100 00100 00100 00100 00100 01110", "J": "00111 00010 00010 00010 00010 10010 01100",
    "K": "10001 10010 10100 11000 10100 10010 10001", "L": "10000 10000 10000 10000 10000 10000 11111",
    "M": "10001 11011 10101 10101 10001 10001 10001", "N": "10001 11001 10101 10011 10001 10001 10001",
    "O": "01110 10001 10001 10001 10001 10001 01110", "P": "11110 10001 10001 11110 10000 10000 10000",
    "Q": "01110 10001 10001 10001 10101 10010 01101", "R": "11110 10001 10001 11110 10100 10010 10001",
    "S": "01111 10000 10000 01110 00001 00001 11110", "T": "11111 00100 00100 00100 00100 00100 00100",
    "U": "10001 10001 10001 10001 10001 10001 01110", "V": "10001 10001 10001 10001 10001 01010 00100",
    "W": "10001 10001 10001 10101 10101 11011 10001", "X": "10001 10001 01010 00100 01010 10001 10001",
    "Y": "10001 10001 01010 00100 00100 00100 00100", "Z": "11111 00001 00010 00100 01000 10000 11111",
    "0": "01110 10001 10011 10101 11001 10001 01110", "1": "00100 01100 00100 00100 00100 00100 01110",
    "2": "01110 10001 00001 00010 00100 01000 11111", "3": "11110 00001 00001 01110 00001 00001 11110",
    "4": "00010 00110 01010 10010 11111 00010 00010", "5": "11111 10000 11110 00001 00001 10001 01110",
    "6": "00110 01000 10000 11110 10001 10001 01110", "7": "11111 00001 00010 00100 01000 01000 01000",
    "8": "01110 10001 10001 01110 10001 10001 01110", "9": "01110 10001 10001 01111 00001 00010 01100",
    "-": "00000 00000 00000 11111 00000 00000 00000", ".": "00000 00000 00000 00000 00000 01100 01100",
    "*": "00000 00100 10101 01110 10101 00100 00000", " ": "00000 00000 00000 00000 00000 00000 00000",
    "/": "00001 00010 00010 00100 01000 01000 10000", "<": "00010 00100 01000 10000 01000 00100 00010",
    ">": "01000 00100 00010 00001 00010 00100 01000",
}
GLYPH = {k: np.array([[int(c) for c in row] for row in v.split()], bool) for k, v in FONT.items()}


def text_mask(s, scale, gap=1):
    # A boolean image of the string: 5x7 glyphs scaled up, `gap` font pixels between letters.
    cols = []
    for ch in s:
        g = np.kron(GLYPH[ch], np.ones((scale, scale), bool))
        cols.append(g)
        cols.append(np.zeros((7 * scale, gap * scale), bool))
    return np.hstack(cols[:-1])


class Canvas:
    """The atlas: base colour, roughness, metal and a height field, drawn with plain rectangles and text."""

    def __init__(self, w, h, base_rgb):
        self.w, self.h = w, h
        self.base = np.ones((h, w, 3)) * np.array(base_rgb)
        self.rough = np.full((h, w), 0.55)
        self.metal = np.zeros((h, w))
        self.height = np.zeros((h, w))

    def rect(self, x0, y0, x1, y1, rgb, rough=None, metal=None, height=None):
        x0, y0, x1, y1 = int(x0), int(y0), int(x1), int(y1)
        self.base[y0:y1, x0:x1] = rgb
        if rough is not None:
            self.rough[y0:y1, x0:x1] = rough
        if metal is not None:
            self.metal[y0:y1, x0:x1] = metal
        if height is not None:
            self.height[y0:y1, x0:x1] = height

    def text(self, s, x, y, scale, rgb, centre_in=None, height=None, gap=1):
        # (x, y) is the top left; centre_in = (x0, x1) centres the string between two columns.
        m = text_mask(s, scale, gap)
        th, tw = m.shape
        if centre_in:
            x = int((centre_in[0] + centre_in[1] - tw) / 2)
        x, y = int(x), int(y)
        sub = (slice(y, y + th), slice(x, x + tw))
        self.base[sub][m] = rgb
        if height is not None:
            self.height[sub][m] = height
        return m, (x, y, tw, th)

    def circle(self, cx, cy, r, rgb, ring=None):
        yy, xx = np.mgrid[0:self.h, 0:self.w]
        d = np.hypot(xx - cx, yy - cy)
        mask = d <= r if ring is None else (d <= r) & (d >= r - ring)
        self.base[mask] = rgb
        return mask


tpx = kit["texture_px"]["tile"]
AW, AH = kit["texture_px"]["atlas"]


PAINT = np.array([0.70, 0.66, 0.56])  # beige brushed steel, sRGB
STEEL = 0.56
PLASTIC = np.array([0.075, 0.078, 0.08])

# ---------------------------------------------------------------- tiling: beige paint
base = np.ones((tpx, tpx, 3)) * PAINT
base *= (1 + noise(tpx, tpx, 60) * 0.025 + noise(tpx, tpx, 4) * 0.012 + noise(tpx, tpx, 0.6, 120) * 0.008)[:, :, None]
scr = np.zeros((tpx, tpx))
for _ in range(35):
    x, y = rng.uniform(0, tpx, 2)
    a = rng.uniform(0, math.pi)
    for t in range(int(rng.uniform(0.3, 1.0) * 40)):
        scr[int(y + math.sin(a) * t) % tpx, int(x + math.cos(a) * t) % tpx] = 1.0
base = base * (1 - scr[:, :, None] * 0.25) + scr[:, :, None] * 0.04
# weathering: faint rain streaks (tall thin noise) and a few paint chips down to grey primer
rain = np.clip(noise(tpx, tpx, 1.5, 70), 0, None)
base *= (1 - rain * 0.05)[:, :, None]
chip = (noise(tpx, tpx, 5) > 2.6).astype(float)
chip = np.clip(blur(chip, 0.8) * 1.6, 0, 1)
base = base * (1 - chip[:, :, None]) + chip[:, :, None] * np.array([0.42, 0.42, 0.40])
save_material("ATM_Paint", np.clip(base, 0, 1), np.full((tpx, tpx), 0.45) - scr * 0.1 + rain * 0.04, np.full((tpx, tpx), 0.35) + scr * 0.3 + chip * 0.4,
              normal_from_height(noise(tpx, tpx, 3) * 0.2 - scr - chip * 0.6, 0.7), 1.0)

# ---------------------------------------------------------------- tiling: brushed steel (the base, the plate, the housing)
streak = noise(tpx, tpx, 90, 0.8)
base = np.ones((tpx, tpx, 3)) * STEEL
base += (streak * 0.035 + noise(tpx, tpx, 150) * 0.025)[:, :, None]
# street grime: the texture repeats every metre of height and the bottom rows are the first 10 cm,
# so the plinth and the cabinet foot pick up road splash; blotchy dull patches everywhere else
foot = np.clip((np.arange(tpx) - tpx * 0.88) / (tpx * 0.12), 0, 1)[:, None] * np.ones((1, tpx))
dirt = np.clip(foot * (0.7 + noise(tpx, tpx, 8) * 0.3), 0, 1) + np.clip(noise(tpx, tpx, 40), 0, None) * 0.15
base = base * (1 - dirt[:, :, None] * 0.35) + dirt[:, :, None] * np.array([0.10, 0.09, 0.07]) * 0.35
save_material("ATM_Steel", np.clip(base, 0, 1), np.clip(0.42 + streak * 0.05 + dirt * 0.35, 0, 1), np.clip(0.85 - dirt * 0.5, 0, 1),
              normal_from_height(streak * 0.15, 0.7), 1.0)

# ---------------------------------------------------------------- tiling: black plastic and rubber (bezels, slots, rims)
spx = 128
save_material("ATM_Dark", np.clip(np.ones((spx, spx, 3)) * PLASTIC + noise(spx, spx, 3)[:, :, None] * 0.008, 0, 1),
              np.full((spx, spx), 0.7), np.zeros((spx, spx)), normal_from_height(noise(spx, spx, 2) * 0.2, 0.5), 0.5)

# ---------------------------------------------------------------- the face atlas (1 px = 1 mm)
# The kiosk's recessed fascia, the keypad shelf top, the card reader, the camera, the cash slot housing.
BODY, PLI, OPEN, HD, SIGN, SC, SH = L["body"], L["plinth"], L["opening"], L["header"], L["sign"], L["screen"], L["shelf"]
FW = int(round(OPEN["half_width"] * 2000))         # 500
FH = int(round((OPEN["y1"] - OPEN["y0"]) * 1000))  # 1080
SHELF_LEN = int(round(math.hypot(SH["front_z"] - SH["back_z"], SH["back_top"] - SH["front_top"]) * 1000))
SHELF_W = int(round(SH["half_width"] * 2000))
R_FASCIA = (0, 0, FW, FH)
R_SHELF = (0, 1100, SHELF_W, SHELF_LEN)
R_CARD = (0, 1260, 100, 54)
R_CAM = (120, 1260, 60, 30)
R_CASH = (0, 1330, 320, 80)
R_GREEN = (200, 1260, 20, 20)
GREEN = np.array([0.10, 0.36, 0.20])     # the painted trim green
FASCIA_RGB = np.array([0.20, 0.19, 0.17])  # dark warm grey enamel inside the recess
CREAM = np.array([0.86, 0.84, 0.76])

cv = Canvas(AW, AH, FASCIA_RGB)
cv.rough[:] = 0.5

# A: the fascia, rows down from the top of the opening (y1), columns from its left edge
x0, y0, fw, fh = R_FASCIA
cv.base[y0:y0 + fh, x0:x0 + fw] *= (1 + noise(fh, fw, 50) * 0.04 + noise(fh, fw, 3) * 0.015)[:, :, None]
cv.rect(x0, y0, x0 + fw, y0 + 78, (0.13, 0.125, 0.115), rough=0.45)          # the top band
cv.text("BANQUE", x0 + 24, y0 + 24, 4, (0.86, 0.84, 0.76))
cv.text("ST-PHILIPPE", x0 + 168, y0 + 34, 2, GREEN * 1.6)
cv.text("DAB", 0, y0 + 34, 2, (0.6, 0.6, 0.56), centre_in=(x0 + fw - 80, x0 + fw - 20))
cv.rect(x0, y0 + 78, x0 + fw, y0 + 82, GREEN, height=0.5)
# the screen bezel and the two steel rails beside it
cv.rect(x0 + 108, y0 + 116, x0 + fw - 108, y0 + 344, (0.05, 0.05, 0.05), rough=0.6, height=1.5)
for rx in (100, fw - 106):
    cv.rect(x0 + rx, y0 + 112, x0 + rx + 6, y0 + 348, (0.62, 0.62, 0.62), rough=0.3, metal=0.9, height=2.0)
# four side keys each side, level with the screen menu lines
SIDE_ROWS = (155, 200, 245, 290)
for sx in (62, fw - 62):
    for r in SIDE_ROWS:
        cv.rect(x0 + sx - 20, y0 + r - 13, x0 + sx + 20, y0 + r + 13, (0.30, 0.30, 0.31), rough=0.4, metal=0.6, height=2.5)
        cv.rect(x0 + sx - 20, y0 + r - 13, x0 + sx + 20, y0 + r - 10, (0.55, 0.55, 0.56), height=2.5)
# the receipt slot (right, under the shelf) and the cash slot label
cv.text("RECU", 0, y0 + 458, 2, (0.62, 0.62, 0.58), centre_in=(x0 + 300, x0 + 420))
cv.rect(x0 + 300, y0 + 478, x0 + 420, y0 + 500, (0.40, 0.40, 0.41), rough=0.35, metal=0.8, height=1.0)
cv.rect(x0 + 308, y0 + 486, x0 + 412, y0 + 492, (0.01, 0.01, 0.01), rough=0.9, height=-2.0)
cv.rect(x0 + 300, y0 + 500, x0 + 420, y0 + 503, GREEN)
cv.text("BILLETS", x0 + 92, y0 + 535, 2, (0.62, 0.62, 0.58))
# the branch sticker, lower left: small, slightly yellowed, a corner lifting
st = (x0 + 30, y0 + 720, x0 + 200, y0 + 778)
cv.rect(*st, (0.84, 0.82, 0.72), rough=0.7, height=0.3)
cv.rect(st[0], st[1], st[0] + 4, st[3], GREEN)
for i, line in enumerate(("BANQUE DE ST-PHILIPPE", "AGENCE 01 - EVERON", "ASSISTANCE 24H/24")):
    cv.text(line, st[0] + 10, st[1] + 8 + i * 16, 1, (0.12, 0.12, 0.12))
cv.base[st[3] - 10:st[3], st[2] - 12:st[2]] *= 0.8
cv.text("SAINT-PHILIPPE - OUVERT 24H/24", 0, y0 + 1030, 2, (0.42, 0.41, 0.38), centre_in=(x0, x0 + fw))
# the green trim round the recess
for a in ((0, 0, fw, 5), (0, fh - 5, fw, fh), (0, 0, 5, fh), (fw - 5, 0, fw, fh)):
    cv.rect(x0 + a[0], y0 + a[1], x0 + a[2], y0 + a[3], GREEN, rough=0.45, height=1.0)
# weathering: rain runs from the top band down, finger grime under the shelf and round the cash slot
fas = cv.base[y0:y0 + fh, x0:x0 + fw]
yy, xx = np.mgrid[0:fh, 0:fw]
streaks = np.zeros((fh, fw))
for sx0 in rng.uniform(8, fw - 8, 18):
    start = int(rng.choice([82, 348, 503, 0]))
    length = rng.uniform(100, 380)
    col = np.exp(-((xx - sx0) / rng.uniform(1.5, 4.5)) ** 2)
    streaks = np.maximum(streaks, col * np.clip(1 - (yy - start) / length, 0, 1) * (yy >= start) * rng.uniform(0.4, 1))
fas *= (1 - streaks * 0.22)[:, :, None]
for gx, gy, rx_, ry_ in ((fw / 2, 600, 190, 45), (360, 490, 70, 25)):
    g = np.clip(np.exp(-(((xx - gx) / rx_) ** 2 + ((yy - gy) / ry_) ** 2)) * (0.6 + 0.4 * noise(fh, fw, 6)), 0, 1)
    fas[:] = fas * (1 - g[:, :, None] * 0.18)
cv.rough[y0:y0 + fh, x0:x0 + fw] += streaks * 0.08
edge = (np.minimum(np.minimum(xx, fw - 1 - xx), np.minimum(yy, fh - 1 - yy)) < 4) & (noise(fh, fw, 3) > 1.2)
fas[edge] = (0.45, 0.45, 0.44)

# B: the shelf top: brushed steel; the keypad on the left (digits, then VALID, CORR, ANNUL), its plate dark
x0, y0, kw, kh = R_SHELF
streak2 = noise(kh, kw, 60, 0.8)
cv.base[y0:y0 + kh, x0:x0 + kw] = (0.56 + streak2 * 0.03)[:, :, None]
cv.rough[y0:y0 + kh, x0:x0 + kw] = 0.4
cv.metal[y0:y0 + kh, x0:x0 + kw] = 0.85
cv.rect(x0, y0 + kh - 4, x0 + kw, y0 + kh, GREEN)  # the front lip
KEY_W, KEY_H, PITCH_X, PITCH_Y, KX0, KY0 = 28, 18, 32, 22, 24, 26
cv.rect(x0 + 16, y0 + 18, x0 + KX0 + 4 * PITCH_X + 4, y0 + KY0 + 4 * PITCH_Y + 4, (0.08, 0.08, 0.085), rough=0.6, metal=0.0)
digits = [["1", "2", "3"], ["4", "5", "6"], ["7", "8", "9"], ["", "0", ""]]
FUNC = ((0.18, 0.62, 0.26), (0.86, 0.72, 0.18), (0.74, 0.17, 0.15))
KEYS = []
for r in range(4):
    for c in range(4):
        kx, ky = KX0 + c * PITCH_X, KY0 + r * PITCH_Y
        if c == 3:
            if r == 3:
                continue
            cv.rect(x0 + kx, y0 + ky, x0 + kx + KEY_W, y0 + ky + KEY_H, FUNC[r], rough=0.5, metal=0.0)
        else:
            cv.rect(x0 + kx, y0 + ky, x0 + kx + KEY_W, y0 + ky + KEY_H, (0.62, 0.62, 0.63), rough=0.35, metal=0.9)
            if digits[r][c]:
                cv.text(digits[r][c], 0, y0 + ky + 2, 2, (0.07, 0.07, 0.07), centre_in=(x0 + kx, x0 + kx + KEY_W))
            wy, wx = np.mgrid[0:KEY_H, 0:KEY_W]
            w = np.exp(-(((wx - 14) / 10) ** 2 + ((wy - 9) / 6) ** 2)) * rng.uniform(0.4, 1.0)
            cv.base[y0 + ky:y0 + ky + KEY_H, x0 + kx:x0 + kx + KEY_W] *= (1 - w * 0.12)[:, :, None]
        KEYS.append((kx, ky))
g = np.clip(noise(kh, kw, 12), 0, None) * 0.12
cv.base[y0:y0 + kh, x0:x0 + kw] *= (1 - g)[:, :, None]

# C: the card reader front: dark grey, a green-lipped slot, a small green arrow
x0, y0, w_, h_ = R_CARD
cv.rect(x0, y0, x0 + w_, y0 + h_, (0.16, 0.16, 0.165), rough=0.45, metal=0.3)
cv.rect(x0 + 12, y0 + 22, x0 + w_ - 12, y0 + 30, (0.01, 0.01, 0.01), rough=0.9, height=-2.0)
cv.rect(x0 + 12, y0 + 30, x0 + w_ - 12, y0 + 33, GREEN * 1.5)
for k in range(5):
    cv.rect(x0 + 40 + k * 2, y0 + 6 + k * 2, x0 + 60 - k * 2, y0 + 8 + k * 2, (0.3, 0.8, 0.4))
cv.text("CARTE", 0, y0 + 40, 1, (0.7, 0.7, 0.7), centre_in=(x0, x0 + w_))

# D: the camera: a grey housing with a dark lens
x0, y0, w_, h_ = R_CAM
cv.rect(x0, y0, x0 + w_, y0 + h_, (0.42, 0.42, 0.42), rough=0.4, metal=0.6)
cv.circle(x0 + 20, y0 + 15, 10, (0.02, 0.02, 0.025))
cv.circle(x0 + 20, y0 + 15, 10, (0.25, 0.25, 0.27), ring=2)
cv.circle(x0 + 47, y0 + 15, 3, (0.5, 0.1, 0.08))

# E: the cash slot housing: steel, a wide dark slot, a green lip
x0, y0, w_, h_ = R_CASH
streak3 = noise(h_, w_, 70, 0.8)
cv.base[y0:y0 + h_, x0:x0 + w_] = (0.52 + streak3 * 0.03)[:, :, None]
cv.rough[y0:y0 + h_, x0:x0 + w_] = 0.4
cv.metal[y0:y0 + h_, x0:x0 + w_] = 0.85
cv.rect(x0 + 14, y0 + 22, x0 + w_ - 14, y0 + 52, (0.008, 0.008, 0.008), rough=0.9, metal=0.0, height=-4.0)
cv.rect(x0 + 14, y0 + 52, x0 + w_ - 14, y0 + 56, GREEN)
cv.rect(*np.add(R_GREEN, (0, 0, R_GREEN[0], R_GREEN[1])), GREEN, rough=0.45)

atlas_n = normal_from_height(blur(cv.height, 0.7) + noise(AH, AW, 2) * 0.06, 1.2)
save_material("ATM_Face", np.clip(cv.base, 0, 1), np.clip(cv.rough, 0, 1), np.clip(cv.metal, 0, 1), atlas_n, None)

# ---------------------------------------------------------------- the glow atlas: the lit header sign and the CRT
GW, GH = kit["texture_px"]["glow"]
G_SIGN = (0, 0, 580, 200)
G_SCREEN = (0, 220, 260, 200)
gb = np.zeros((GH, GW, 3))
em = np.zeros((GH, GW, 3))
# the sign: a cream backlit panel, green lettering, a thin green rule; no brand, only the village bank
sg = Canvas(580, 200, CREAM)
sg.base *= (1 + noise(200, 580, 30) * 0.015)[:, :, None]
sg.rect(10, 10, 570, 13, GREEN)
sg.rect(10, 187, 570, 190, GREEN)
sg.text("BANQUE", 0, 26, 9, GREEN, centre_in=(0, 580))
sg.text("DE SAINT-PHILIPPE", 0, 100, 3, GREEN, centre_in=(0, 580))
sg.rect(110, 132, 470, 134, GREEN * 0.8)
sg.text("DISTRIBUTEUR DE BILLETS 24H", 0, 146, 2, (0.30, 0.30, 0.28), centre_in=(0, 580))
gb[0:200, 0:580] = sg.base
em[0:200, 0:580] = sg.base * 0.85
# the screen: green phosphor on black, a menu that lines up with the side keys
sw, sh = G_SCREEN[2], G_SCREEN[3]
glow = np.zeros((sh, sw))


def put(s, x, y, sc, right=False):
    m = text_mask(s, sc)
    th, tw = m.shape
    x = sw - tw - x if right else x
    glow[y:y + th, x:x + tw][m] = 1.0


put("BANQUE DE ST-PHILIPPE", (sw - 21 * 6) // 2, 4, 1)
for i, (l, r) in enumerate((("<RETRAIT", "100 F>"), ("<SOLDE", "200 F>"), ("<RELEVE", "500 F>"), ("<AUTRE", "1000 F>"))):
    y = SIDE_ROWS[i] - 130 - 7  # fascia row of the key, less the screen's top row, less half a glyph
    put(l, 6, y, 2)
    put(r, 6, y, 2, right=True)
put("CHOISISSEZ VOTRE OPERATION", (sw - 26 * 6) // 2, 186, 1)
lum = np.clip(blur(glow, 0.8) * 1.15 + blur(glow, 4.0) * 1.3, 0, 1.2)
scr_rgb = np.zeros((sh, sw, 3)) + np.array([0.012, 0.03, 0.016]) + lum[:, :, None] * np.array([0.10, 0.92, 0.22])
yy, xx = np.mgrid[0:sh, 0:sw]
scr_rgb *= ((1 - 0.5 * (((xx - sw / 2) / (sw / 2)) ** 4 + ((yy - sh / 2) / (sh / 2)) ** 4)) * (1 - 0.18 * ((yy % 3) == 0)))[:, :, None]
gb[220:220 + sh, 0:sw] = np.clip(scr_rgb, 0, 1)
em[220:220 + sh, 0:sw] = np.clip(scr_rgb, 0, 1)
g_rough = np.full((GH, GW), 0.25)
g_rough[0:200, 0:580] = 0.4
save_material("ATM_Glow", np.clip(gb, 0, 1), g_rough, np.zeros((GH, GW)),
              normal_from_height(noise(GH, GW, 2) * 0.03, 0.5), None, em=np.clip(em, 0, 1))

def blender_material(name):
    m = bpy.data.materials.get(name)
    if m:
        return m
    m = bpy.data.materials.new(name)  # the FBX SourceMaterial name the .xob.meta maps to the .emat
    m.use_nodes = True
    tex = m.node_tree.nodes.new("ShaderNodeTexImage")
    tex.image = bpy.data.images.load(TEX[name]["base_png"])
    m.node_tree.links.new(tex.outputs["Color"], m.node_tree.nodes["Principled BSDF"].inputs["Base Color"])
    m.node_tree.nodes.active = tex
    return m


# ================================================================ geometry
class Part:
    """A bmesh with a material per face and box-projected UVs (in metres / the material's tile)."""

    def __init__(self, name):
        self.name = name
        self.bm = bmesh.new()
        self.uv = self.bm.loops.layers.uv.new("UVMap")
        self.auto = self.bm.faces.layers.int.new("auto")
        self.mats = []

    def mi(self, mat):
        if mat not in self.mats:
            self.mats.append(mat)
        return self.mats.index(mat)

    def face(self, pts, mat, want=None, uvs=None):
        vs = [self.bm.verts.new(p) for p in pts]
        f = self.bm.faces.new(vs)
        f.normal_update()
        if want is not None and f.normal.dot(want) < 0:
            f.normal_flip()
        f.material_index = self.mi(mat)
        if uvs:
            by_vert = dict(zip(vs, uvs))  # a flip reverses the loops, so map the UVs by vertex
            for loop in f.loops:
                loop[self.uv].uv = by_vert[loop.vert]
        else:
            f[self.auto] = 1
        return f

    def box(self, lo, hi, mat, mats=None, skip=()):
        # lo, hi in Enfusion axes. mats maps a side ("+x", "-x", "+y", "-y", "+z", "-z") to a material.
        x0, y0, z0 = lo
        x1, y1, z1 = hi
        c = {
            "+x": ([(x1, y0, z0), (x1, y1, z0), (x1, y1, z1), (x1, y0, z1)], B(1, 0, 0)),
            "-x": ([(x0, y0, z0), (x0, y0, z1), (x0, y1, z1), (x0, y1, z0)], B(-1, 0, 0)),
            "+y": ([(x0, y1, z0), (x0, y1, z1), (x1, y1, z1), (x1, y1, z0)], B(0, 1, 0)),
            "-y": ([(x0, y0, z0), (x1, y0, z0), (x1, y0, z1), (x0, y0, z1)], B(0, -1, 0)),
            "+z": ([(x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)], B(0, 0, 1)),
            "-z": ([(x0, y0, z0), (x0, y1, z0), (x1, y1, z0), (x1, y0, z0)], B(0, 0, -1)),
        }
        for side, (pts, n) in c.items():
            if side in skip:
                continue
            self.face([B(*p) for p in pts], (mats or {}).get(side, mat), want=n)

    def prism(self, profile, x0, x1, mat, skip_edges=(), skip_caps=False):
        # A convex profile of (z, y) points, extruded along x from x0 to x1. Edge i joins point i to i+1.
        n = len(profile)
        centre = B((x0 + x1) / 2, sum(p[1] for p in profile) / n, sum(p[0] for p in profile) / n)
        for i in range(n):
            if i in skip_edges:
                continue
            (za, ya), (zb, yb) = profile[i], profile[(i + 1) % n]
            pts = [B(x0, ya, za), B(x1, ya, za), B(x1, yb, zb), B(x0, yb, zb)]
            mid = sum(pts, Vector()) / 4
            self.face(pts, mat, want=mid - centre)
        if not skip_caps:
            for x in (x0, x1):
                pts = [B(x, y, z) for z, y in profile]
                mid = sum(pts, Vector()) / n
                self.face(pts, mat, want=mid - centre)

    def quad(self, mat, bl, br, tr, tl, normal, region, size=None):
        # An atlas-mapped quad. Corners and normal in Enfusion axes; region (x, y, w, h) in atlas pixels
        # of the face atlas, or of an atlas of `size` (w, h) such as the glow atlas.
        rx, ry, rw, rh = region
        aw, ah = size or (AW, AH)
        uvs = [(rx / aw, 1 - (ry + rh) / ah), ((rx + rw) / aw, 1 - (ry + rh) / ah), ((rx + rw) / aw, 1 - ry / ah), (rx / aw, 1 - ry / ah)]
        self.face([B(*p) for p in (bl, br, tr, tl)], mat, want=B(*normal), uvs=uvs)

    def slab(self, corners, normal, thick, top_mat, side_mat, region=None, size=None):
        # A thin raised plate on a surface: corners (bl, br, tr, tl) on the surface in Enfusion axes,
        # lifted `thick` metres along `normal`. The top is atlas-mapped from `region`, the sides tile.
        nv = Vector(normal).normalized()
        lo = [Vector(c) for c in corners]
        hi = [c + nv * thick for c in lo]
        if region:
            self.quad(top_mat, *[tuple(c) for c in hi], tuple(nv), region, size)
        else:
            self.face([B(*c) for c in hi], top_mat, want=B(*nv))
        centre = sum(lo + hi, Vector()) / 8
        for i in range(4):
            a, b = i, (i + 1) % 4
            pts = [lo[a], lo[b], hi[b], hi[a]]
            mid = sum(pts, Vector()) / 4
            self.face([B(*c) for c in pts], side_mat, want=B(*(mid - centre)))

    def finish(self):
        # Box-projected UVs for every face that has none, then one mesh object.
        for f in self.bm.faces:
            if not f[self.auto]:
                continue
            mat = self.mats[f.material_index]
            tile = TEX[mat]["tile_m"] or 1.0
            n = f.normal
            ax = max(range(3), key=lambda k: abs(n[k]))
            for loop in f.loops:
                p = loop.vert.co
                if ax == 0:
                    u, v = p.y, p.z
                elif ax == 1:
                    u, v = p.x, p.z
                else:
                    u, v = p.x, p.y
                loop[self.uv].uv = (u / tile, v / tile)
        bmesh.ops.remove_doubles(self.bm, verts=self.bm.verts, dist=0.0001)
        me = bpy.data.meshes.new(self.name)
        self.bm.to_mesh(me)
        self.bm.free()
        o = bpy.data.objects.new(self.name, me)
        bpy.context.scene.collection.objects.link(o)
        for mat in self.mats:
            o.data.materials.append(blender_material(mat))
        return o


def tris(o):
    return sum(len(p.vertices) - 2 for p in o.data.polygons)


def bounds(o):
    co = np.array([v.co[:] for v in o.data.vertices])
    return co.min(axis=0), co.max(axis=0)


def collider(model, i, lo, hi, cfg):
    # One UBX_ box, its origin at its own centre (wiki: FBX Import).
    name = "UBX_%s_%02d" % (model, i)
    bm2 = bmesh.new()
    bmesh.ops.create_cube(bm2, size=1)
    a, b = B(*lo), B(*hi)
    size = Vector((abs(b.x - a.x), abs(b.y - a.y), abs(b.z - a.z)))
    for v in bm2.verts:
        v.co = Vector((v.co.x * size.x, v.co.y * size.y, v.co.z * size.z))
    me = bpy.data.meshes.new(name)
    bm2.to_mesh(me)
    bm2.free()
    o = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(o)
    o.location = (a + b) / 2
    o["usage"] = cfg["layerPreset"]
    o.display_type = "WIRE"
    o.hide_render = True
    return o


# ================================================================ the kiosk, one function, three levels of detail
# A free-standing cash machine against a wall: black plinth, beige brushed-steel body, a recessed fascia with
# green trim, a sloped keypad shelf with a card reader, a cash slot housing, a lit header box with a camera under it.
FZ = BODY["front_z"] - OPEN["recess"]  # the fascia plane inside the recess
SH_PROFILE = [(SH["back_z"], SH["back_bottom"]), (SH["front_z"], SH["front_bottom"]), (SH["front_z"], SH["front_top"]),
              (SH["back_z"], SH["back_top"])]  # (z, y): bottom, front lip, sloped top (edge 2), back against the fascia
GS = (GW, GH)


def fpx(col):  # fascia atlas column -> x
    return -OPEN["half_width"] + col / 1000.0


def fpy(row):  # fascia atlas row -> y
    return OPEN["y1"] - row / 1000.0


def make(level):
    p = Part("atm_L%d" % level)
    bw, ow = BODY["half_width"], OPEN["half_width"]
    bf = BODY["front_z"]
    # plinth, black
    p.box((-PLI["half_width"], 0.0, BACK), (PLI["half_width"], PLI["height"], PLI["front_z"]), "ATM_Dark", skip=("-y",))
    # the body: below, above and beside the opening it is full depth; the opening is recessed
    if level <= 1:
        p.box((-bw, PLI["height"], BACK), (bw, OPEN["y0"], bf), "ATM_Paint")
        p.box((-bw, OPEN["y1"], BACK), (bw, HD["y0"], bf), "ATM_Paint")
        for s in (-1, 1):
            a, b = sorted((s * ow, s * bw))
            p.box((a, OPEN["y0"], BACK), (b, OPEN["y1"], bf), "ATM_Paint", skip=("+y", "-y"))
        # the recess walls, painted green
        g = R_GREEN
        p.quad("ATM_Face", (-ow, OPEN["y0"], FZ), (ow, OPEN["y0"], FZ), (ow, OPEN["y0"], bf), (-ow, OPEN["y0"], bf), (0, 1, 0), g)
        p.quad("ATM_Face", (-ow, OPEN["y1"], bf), (ow, OPEN["y1"], bf), (ow, OPEN["y1"], FZ), (-ow, OPEN["y1"], FZ), (0, -1, 0), g)
        p.quad("ATM_Face", (-ow, OPEN["y0"], bf), (-ow, OPEN["y0"], FZ), (-ow, OPEN["y1"], FZ), (-ow, OPEN["y1"], bf), (1, 0, 0), g)
        p.quad("ATM_Face", (ow, OPEN["y0"], FZ), (ow, OPEN["y0"], bf), (ow, OPEN["y1"], bf), (ow, OPEN["y1"], FZ), (-1, 0, 0), g)
        # the box behind the recess (closes the body between the side columns)
        p.box((-ow, OPEN["y0"], BACK), (ow, OPEN["y1"], FZ), "ATM_Paint", skip=("+z", "+y", "-y", "+x", "-x"))
        fz = FZ
    else:
        p.box((-bw, PLI["height"], BACK), (bw, HD["y0"], bf), "ATM_Paint", skip=("+z",))
        p.quad("ATM_Paint", (-bw, PLI["height"], bf), (bw, PLI["height"], bf), (bw, HD["y0"], bf), (-bw, HD["y0"], bf), (0, 0, 1),
               (0, 0, 1, 1), (1, 1))
        fz = bf + 0.001
    p.quad("ATM_Face", (-ow, OPEN["y0"], fz), (ow, OPEN["y0"], fz), (ow, OPEN["y1"], fz), (-ow, OPEN["y1"], fz), (0, 0, 1), R_FASCIA)
    # the screen glass, just proud of its bezel
    sx = SC["half_width"]
    sz = fz + 0.003
    p.quad("ATM_Glow", (-sx, SC["y0"], sz), (sx, SC["y0"], sz), (sx, SC["y1"], sz), (-sx, SC["y1"], sz), (0, 0, 1), G_SCREEN, GS)
    # the header box and its lit sign panel
    hw = HD["half_width"]
    p.box((-hw, HD["y0"], BACK), (hw, HD["y1"], HD["front_z"]), "ATM_Paint", skip=("+z",) if level == 2 else ())
    if level == 2:
        p.quad("ATM_Paint", (-hw, HD["y0"], HD["front_z"]), (hw, HD["y0"], HD["front_z"]), (hw, HD["y1"], HD["front_z"]),
               (-hw, HD["y1"], HD["front_z"]), (0, 0, 1), (0, 0, 1, 1), (1, 1))
    gz = HD["front_z"] + 0.002
    sgw = SIGN["half_width"]
    p.quad("ATM_Glow", (-sgw, SIGN["y0"], gz), (sgw, SIGN["y0"], gz), (sgw, SIGN["y1"], gz), (-sgw, SIGN["y1"], gz), (0, 0, 1), G_SIGN, GS)
    # the sloped shelf: steel, its top is the keypad atlas
    shw = SH["half_width"]
    p.prism(SH_PROFILE, -shw, shw, "ATM_Steel", skip_edges=(2, 3))
    (z2, y2), (z3, y3) = SH_PROFILE[2], SH_PROFILE[3]
    nz, ny = (y3 - y2), -(z3 - z2)
    p.quad("ATM_Face", (-shw, y2, z2), (shw, y2, z2), (shw, y3, z3), (-shw, y3, z3), (0, ny, nz), R_SHELF)

    def dk(px, py):  # a shelf atlas pixel -> a point on the sloped top (row 0 is the back edge)
        t = py / R_SHELF[3]
        return (-shw + px / 1000.0, y2 + (y3 - y2) * (1 - t), z2 + (z3 - z2) * (1 - t))

    if level == 0:
        for kx, ky in KEYS:  # raised keys, 5 mm, their tops the drawn keys
            corners = [dk(kx, ky + KEY_H), dk(kx + KEY_W, ky + KEY_H), dk(kx + KEY_W, ky), dk(kx, ky)]
            p.slab(corners, (0, ny, nz), 0.005, "ATM_Face", "ATM_Steel", region=(R_SHELF[0] + kx, R_SHELF[1] + ky, KEY_W, KEY_H))
    if level <= 1:
        # the card reader: a dark block on the right of the shelf, its front the reader face
        cr = L["card_reader"]
        p.box((cr["x0"], cr["y0"], cr["z0"]), (cr["x1"], cr["y1"], cr["z1"]), "ATM_Dark", skip=("+z",))
        p.quad("ATM_Face", (cr["x0"], cr["y0"], cr["z1"]), (cr["x1"], cr["y0"], cr["z1"]), (cr["x1"], cr["y1"], cr["z1"]),
               (cr["x0"], cr["y1"], cr["z1"]), (0, 0, 1), R_CARD)
        # the cash slot housing, proud of the fascia
        cs = L["cash"]
        corners = [(fpx(cs["col0"]), fpy(cs["row1"]), FZ), (fpx(cs["col1"]), fpy(cs["row1"]), FZ),
                   (fpx(cs["col1"]), fpy(cs["row0"]), FZ), (fpx(cs["col0"]), fpy(cs["row0"]), FZ)]
        p.slab(corners, (0, 0, 1), cs["depth"], "ATM_Face", "ATM_Steel", region=R_CASH)
    if level == 0:
        # the security camera on the top rail, under the header
        cm = L["camera"]
        p.box((cm["x0"], cm["y0"], bf), (cm["x1"], cm["y1"], bf + cm["depth"]), "ATM_Steel", skip=("+z",))
        p.quad("ATM_Face", (cm["x0"], cm["y0"], bf + cm["depth"]), (cm["x1"], cm["y0"], bf + cm["depth"]),
               (cm["x1"], cm["y1"], bf + cm["depth"]), (cm["x0"], cm["y1"], bf + cm["depth"]), (0, 0, 1), R_CAM)
        # a drip lip along the header's lower front edge
        p.box((-hw, HD["y0"] - 0.008, HD["front_z"] - 0.03), (hw, HD["y0"], HD["front_z"]), "ATM_Steel")
    return p.finish()


# ================================================================ models, export
lod_objs = [make(i) for i in range(len(kit["lod_ratios"]) + 1)]
for i, o in enumerate(lod_objs):
    o.name = "ATMWall_LOD%d" % i
    o.data.name = "ATMWall_LOD%d" % i
cfg = kit["colliders"]["ATMWall"]
body = (-PLI["half_width"], 0.0, BACK), (PLI["half_width"], HD["y1"], BODY["front_z"])
front = (-SH["half_width"], SH["front_bottom"], BODY["front_z"]), (SH["half_width"], L["card_reader"]["y1"], SH["front_z"])
cols = [collider("ATMWall", 1, *body, cfg), collider("ATMWall", 2, *front, cfg)]

export = list(lod_objs) + cols
info = {"lods": [{"name": o.name, "tris": tris(o)} for o in lod_objs], "colliders": [c.name for c in cols],
        "materials": [s.material.name for s in lod_objs[0].material_slots]}
lo, hi = bounds(lod_objs[0])
info["bounds"] = {"min": b2e(lo), "max": b2e(hi)}
bpy.context.view_layer.update()
bpy.ops.object.select_all(action="DESELECT")
for o in export:
    o.select_set(True)
bpy.context.view_layer.objects.active = lod_objs[0]
fbx = os.path.join(out, "ATMWall.fbx")
bpy.ops.export_scene.fbx(filepath=fbx, check_existing=False, use_selection=True,
                         object_types={"MESH", "OTHER", "EMPTY"}, use_custom_props=True, add_leaf_bones=False, bake_anim=False)
info["fbx"] = fbx
report["models"]["ATMWall"] = info

# ================================================================ review renders
scene = bpy.context.scene
scene.render.engine = "BLENDER_WORKBENCH"
scene.display.shading.light = "STUDIO"
scene.display.shading.color_type = "TEXTURE"
scene.display.shading.show_cavity = True
scene.world = bpy.data.worlds.new("w")
scene.world.color = (0.32, 0.33, 0.35)
# A stub wall behind the machine, so the review shows it seated.
wall = bpy.data.objects.new("wall", bpy.data.meshes.new("wall"))
wbm = bmesh.new()
bmesh.ops.create_cube(wbm, size=1)
for v in wbm.verts:
    v.co = Vector((v.co.x * 2.4, v.co.y * 0.3, v.co.z * 2.6)) + B(0, 1.3, BACK - 0.15)
wbm.to_mesh(wall.data)
wbm.free()
wall.data.materials.append(bpy.data.materials.new("wallmat"))
wall.data.materials[0].diffuse_color = (0.62, 0.6, 0.55, 1)
scene.collection.objects.link(wall)
for o in bpy.data.objects:
    if o.type == "MESH":
        o.hide_render = not (o.name.endswith("_LOD0") or o is wall)
cam_data = bpy.data.cameras.new("cam")
cam = bpy.data.objects.new("cam", cam_data)
scene.collection.objects.link(cam)
scene.camera = cam


def shot(file, target, direction, dist, ortho=None, res=(1000, 1100), lens=35):
    d = B(*direction).normalized()
    cam.location = B(*target) + d * dist
    cam.rotation_euler = (-d).to_track_quat("-Z", "Y").to_euler()
    cam_data.type = "ORTHO" if ortho else "PERSP"
    if ortho:
        cam_data.ortho_scale = ortho
    cam_data.lens = lens
    cam_data.clip_start, cam_data.clip_end = 0.01, 50
    scene.render.resolution_x, scene.render.resolution_y = res
    scene.render.filepath = file
    bpy.ops.render.render(write_still=True)


rv = os.path.join(out, "review")
shot(os.path.join(rv, "front.png"), (0, 0.95, 0), (0, 0, 1), 6, ortho=2.0)
shot(os.path.join(rv, "three_quarter.png"), (0, 0.95, -0.1), (0.7, 0.2, 1), 3.6, res=(1000, 1100))
shot(os.path.join(rv, "three_quarter_left.png"), (0, 0.95, -0.1), (-0.7, 0.2, 1), 3.6, res=(1000, 1100))
shot(os.path.join(rv, "side.png"), (0, 0.95, -0.15), (1, 0, 0), 6, ortho=2.0)
shot(os.path.join(rv, "face_close.png"), (0, 1.3, 0), (0.1, 0.08, 1), 1.6, res=(1200, 1000))
shot(os.path.join(rv, "head_close.png"), (0, 1.75, 0.1), (0.15, 0.1, 1), 1.4, res=(1200, 700))
shot(os.path.join(rv, "keypad_close.png"), (0, 1.0, 0.1), (0.3, 0.7, 1), 1.0, res=(1200, 900))
shot(os.path.join(rv, "street.png"), (0, 1.0, 0.1), (0.25, 0.12, 1), 3.2, res=(1000, 1100))
for i in range(1, len(lod_objs)):
    for o in lod_objs:
        o.hide_render = o is not lod_objs[i]
    shot(os.path.join(rv, "lod%d.png" % i), (0, 0.95, -0.1), (0.7, 0.2, 1), 3.6, res=(800, 900))

bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out, "atm-wall.blend"))
with open(os.path.join(out, "report.json"), "w") as f:
    json.dump(report, f, indent=1, default=float)
print("LF_KIT_BUILD ok", json.dumps({k: [l["tris"] for l in v["lods"]] for k, v in report["models"].items()}))
