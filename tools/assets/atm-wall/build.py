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


def save_material(name, base, rough, metal, normal, tile_m):
    h, w = base.shape[:2]
    bcr = np.dstack([to_linear(base[:, :, :3]), rough])
    nmo = np.dstack([normal[:, :, 0], 1.0 - normal[:, :, 1], metal, np.ones((h, w))])
    write_tiff(os.path.join(out, "tex", name + "_BCR.tif"), (np.clip(bcr, 0, 1) * 255 + 0.5).astype(np.uint8))
    write_tiff(os.path.join(out, "tex", name + "_NMO.tif"), (np.clip(nmo, 0, 1) * 255 + 0.5).astype(np.uint8))
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

# Region rectangles on the atlas, in pixels (x, y from the top left, w, h). One pixel is one millimetre.
R_FASCIA = (0, 0, 620, 860)    # the whole beige front, y 0.80 to 1.66 m
R_HEADER = (640, 0, 700, 170)  # the sign plate, 0.66 x 0.16 m used
R_SCREEN = (640, 190, 260, 200)
R_KEYPAD = (640, 410, 300, 140)
R_CASH = (960, 190, 280, 140)

PAINT = np.array([0.80, 0.74, 0.62])  # aged cream paint, sRGB
STEEL = 0.56
PLASTIC = np.array([0.075, 0.078, 0.08])

# ---------------------------------------------------------------- tiling: beige paint
base = np.ones((tpx, tpx, 3)) * PAINT
base *= (1 + noise(tpx, tpx, 60) * 0.025 + noise(tpx, tpx, 4) * 0.012)[:, :, None]
scr = np.zeros((tpx, tpx))
for _ in range(90):
    x, y = rng.uniform(0, tpx, 2)
    a = rng.uniform(0, math.pi)
    for t in range(int(rng.uniform(0.3, 1.0) * 40)):
        scr[int(y + math.sin(a) * t) % tpx, int(x + math.cos(a) * t) % tpx] = 1.0
base = base * (1 - scr[:, :, None] * 0.25) + scr[:, :, None] * 0.04
save_material("ATM_Paint", np.clip(base, 0, 1), np.full((tpx, tpx), 0.6) - scr * 0.15, np.zeros((tpx, tpx)) + scr * 0.3,
              normal_from_height(noise(tpx, tpx, 3) * 0.2 - scr, 0.7), 1.0)

# ---------------------------------------------------------------- tiling: brushed steel (the base, the plate, the housing)
streak = noise(tpx, tpx, 90, 0.8)
base = np.ones((tpx, tpx, 3)) * STEEL
base += (streak * 0.035 + noise(tpx, tpx, 150) * 0.025)[:, :, None]
save_material("ATM_Steel", np.clip(base, 0, 1), np.clip(0.42 + streak * 0.05, 0, 1), np.full((tpx, tpx), 0.85),
              normal_from_height(streak * 0.15, 0.7), 1.0)

# ---------------------------------------------------------------- tiling: black plastic and rubber (bezels, slots, rims)
spx = 128
save_material("ATM_Dark", np.clip(np.ones((spx, spx, 3)) * PLASTIC + noise(spx, spx, 3)[:, :, None] * 0.008, 0, 1),
              np.full((spx, spx), 0.7), np.zeros((spx, spx)), normal_from_height(noise(spx, spx, 2) * 0.2, 0.5), 0.5)

# ---------------------------------------------------------------- the face atlas
cv = Canvas(AW, AH, PAINT)
cv.base *= (1 + noise(AH, AW, 60) * 0.025 + noise(AH, AW, 4) * 0.012)[:, :, None]
cv.rough = 0.6 - 0.02 * noise(AH, AW, 5)

# A: the fascia front (620 x 860 mm, top = y 1.66 m). Row = (1.66 - y) m * 1000, column = (x + 0.31) m * 1000.
x0, y0, fw, fh = R_FASCIA
# grime along the lower edge and the seam under the keypad
fas = cv.base[y0:y0 + fh, x0:x0 + fw]
fas[-70:] *= np.linspace(1.0, 0.9, 70)[:, None, None]
for yy in (170, 440):  # panel seams
    cv.base[y0 + yy:y0 + yy + 2, x0:x0 + fw] *= 0.78
    cv.height[y0 + yy:y0 + yy + 2, x0:x0 + fw] = -1.5
# four corner screws
for sx, sy in ((22, 22 + 170), (fw - 22, 22 + 170), (22, fh - 22), (fw - 22, fh - 22)):
    cv.circle(x0 + sx, y0 + sy, 7, (0.46, 0.46, 0.45))
    cv.base[y0 + sy - 1:y0 + sy + 1, x0 + sx - 5:x0 + sx + 5] = (0.18, 0.18, 0.18)
    cv.height[y0 + sy - 7:y0 + sy + 7, x0 + sx - 7:x0 + sx + 7] += 0.8
# side keys: four each side of the screen (the screen spans rows 190 to 390)
for sx in (75, fw - 75):
    for r in (215, 265, 315, 365):
        cv.rect(x0 + sx - 32, y0 + r - 16, x0 + sx + 32, y0 + r + 16, (0.20, 0.205, 0.21), rough=0.5, metal=0.1, height=2.0)
        cv.rect(x0 + sx - 32, y0 + r - 16, x0 + sx + 32, y0 + r - 13, (0.34, 0.34, 0.35), height=2.0)
        cv.rect(x0 + sx - 9, y0 + r - 2, x0 + sx + 9, y0 + r + 2, (0.85, 0.85, 0.8))
# the card slot (right of the keypad) and the ticket slot (left): rows 500 to 620
for cx, label in ((fw - 75, "CARTE"), (75, "TICKET")):
    cv.text(label, 0, y0 + 503, 2, (0.18, 0.18, 0.17), centre_in=(x0 + cx - 50, x0 + cx + 50))
    cv.rect(x0 + cx - 50, y0 + 528, x0 + cx + 50, y0 + 592, (0.14, 0.14, 0.145), rough=0.45, metal=0.15, height=-1.0)
    cv.rect(x0 + cx - 36, y0 + 556, x0 + cx + 36, y0 + 564, (0.01, 0.01, 0.01), rough=0.9, height=-3.0)
    for k in range(5):  # a small arrow pointing at the slot
        cv.rect(x0 + cx - 12 + k * 3, y0 + 538 + k * 2, x0 + cx + 12 - k * 3, y0 + 540 + k * 2, (0.8, 0.8, 0.75))
# a plain blue badge strip above the cash housing ("RETRAIT") on the fascia strip under it
cv.text("RETRAIT", 0, y0 + 815, 3, (0.18, 0.18, 0.17), centre_in=(x0, x0 + fw))

# B: the screen, 260 x 200: green phosphor text on black glass with scanlines and a vignette
x0, y0, sw, sh = R_SCREEN
scr_rgb = np.zeros((sh, sw, 3)) + np.array([0.012, 0.03, 0.016])
glow = np.zeros((sh, sw))
for s, y, sc in (("BONJOUR", 28, 4), ("INTRODUISEZ", 92, 3), ("VOTRE CARTE", 128, 3), ("S.V.P.", 164, 2)):
    m = text_mask(s.replace("S.V.P.", "S.V.P"), sc)
    th, tw = m.shape
    xx = (sw - tw) // 2
    glow[y:y + th, xx:xx + tw][m] = 1.0
g1 = blur(glow, 1.0)
g2 = blur(glow, 5.0)
lum = np.clip(g1 * 1.15 + g2 * 1.4, 0, 1.2)
scr_rgb += lum[:, :, None] * np.array([0.10, 0.92, 0.22])
yy, xx = np.mgrid[0:sh, 0:sw]
vig = 1 - 0.55 * (((xx - sw / 2) / (sw / 2)) ** 4 + ((yy - sh / 2) / (sh / 2)) ** 4)
scan = 1 - 0.18 * ((yy % 3) == 0)
scr_rgb *= (vig * scan)[:, :, None]
cv.base[y0:y0 + sh, x0:x0 + sw] = np.clip(scr_rgb, 0, 1)
cv.rough[y0:y0 + sh, x0:x0 + sw] = 0.22
cv.height[y0:y0 + sh, x0:x0 + sw] = 0.0

# C: the keypad deck, 300 x 140: dark plate, steel keys with black digits, three coloured function keys
x0, y0, kw, kh = R_KEYPAD
cv.rect(x0, y0, x0 + kw, y0 + kh, (0.09, 0.09, 0.095), rough=0.6, metal=0.0, height=0.0)
digits = [["1", "2", "3"], ["4", "5", "6"], ["7", "8", "9"], [".", "0", "*"]]
for r in range(4):
    for c in range(3):
        kx, ky = x0 + 4 + c * 74, y0 + 4 + r * 33
        cv.rect(kx, ky, kx + 62, ky + 27, (0.60, 0.60, 0.61), rough=0.38, metal=0.9, height=3.0)
        cv.rect(kx, ky, kx + 62, ky + 3, (0.78, 0.78, 0.78), height=3.0)
        cv.text(digits[r][c], 0, ky + 3, 3, (0.07, 0.07, 0.07), centre_in=(kx, kx + 62))
for r, (label, rgb) in enumerate((("VALID", (0.18, 0.62, 0.26)), ("CORR", (0.86, 0.72, 0.18)), ("ANNUL", (0.74, 0.17, 0.15)))):
    kx, ky = x0 + 4 + 3 * 74, y0 + 4 + r * 33
    cv.rect(kx, ky, kx + 62, ky + 27, rgb, rough=0.5, metal=0.0, height=3.0)
    cv.text(label, 0, ky + 6, 2, (0.06, 0.06, 0.06), centre_in=(kx, kx + 62))

# D: the bank sign plate, deep blue with a plain coin-and-columns emblem and BANQUE. No brand.
x0, y0, hw_, hh = R_HEADER
cv.rect(x0, y0, x0 + 660, y0 + 160, (0.07, 0.15, 0.33), rough=0.45, metal=0.05)
cv.base[y0:y0 + 160, x0:x0 + 660] *= (1 + noise(160, 660, 30) * 0.02)[:, :, None]
cv.rect(x0 + 8, y0 + 8, x0 + 652, y0 + 11, (0.9, 0.9, 0.85), height=1.0)
cv.rect(x0 + 8, y0 + 149, x0 + 652, y0 + 152, (0.9, 0.9, 0.85), height=1.0)
ecx, ecy = x0 + 90, y0 + 80
cv.circle(ecx, ecy, 52, (0.93, 0.93, 0.88), ring=6)
for k in (-24, 0, 24):  # three columns inside the coin
    cv.rect(ecx + k - 6, ecy - 22, ecx + k + 6, ecy + 22, (0.93, 0.93, 0.88))
cv.rect(ecx - 34, ecy - 30, ecx + 34, ecy - 22, (0.93, 0.93, 0.88))
cv.rect(ecx - 34, ecy + 22, ecx + 34, ecy + 30, (0.93, 0.93, 0.88))
cv.text("BANQUE", x0 + 175, y0 + 28, 8, (0.93, 0.93, 0.88), height=1.0)
cv.text("DISTRIBUTEUR DE BILLETS", x0 + 175, y0 + 100, 3, (0.78, 0.82, 0.9), gap=1)

# E: the cash housing front, 280 x 140: steel with a dark slot and BILLETS
x0, y0, cw, ch = R_CASH
streak2 = noise(ch, cw, 70, 0.8)
cv.base[y0:y0 + ch, x0:x0 + cw] = (0.55 + streak2 * 0.03)[:, :, None]
cv.rough[y0:y0 + ch, x0:x0 + cw] = 0.42
cv.metal[y0:y0 + ch, x0:x0 + cw] = 0.85
cv.text("BILLETS", 0, y0 + 14, 3, (0.07, 0.07, 0.07), centre_in=(x0, x0 + cw))
cv.rect(x0 + 24, y0 + 56, x0 + cw - 24, y0 + 112, (0.30, 0.30, 0.31), rough=0.4, metal=0.8, height=0.5)
cv.rect(x0 + 34, y0 + 66, x0 + cw - 34, y0 + 102, (0.008, 0.008, 0.008), rough=0.9, metal=0.0, height=-4.0)

atlas_n = normal_from_height(blur(cv.height, 0.7) + noise(AH, AW, 2) * 0.06, 1.2)
save_material("ATM_Face", np.clip(cv.base, 0, 1), np.clip(cv.rough, 0, 1), np.clip(cv.metal, 0, 1), atlas_n, None)


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

    def quad(self, mat, bl, br, tr, tl, normal, region):
        # An atlas-mapped quad. Corners and normal in Enfusion axes; region (x, y, w, h) in atlas pixels.
        rx, ry, rw, rh = region
        uvs = [(rx / AW, 1 - (ry + rh) / AH), ((rx + rw) / AW, 1 - (ry + rh) / AH), ((rx + rw) / AW, 1 - ry / AH), (rx / AW, 1 - ry / AH)]
        self.face([B(*p) for p in (bl, br, tr, tl)], mat, want=B(*normal), uvs=uvs)

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


# ================================================================ the machine, one function, three levels of detail
P, PL, FA, CA, HD, SC = L["plate"], L["plinth"], L["fascia"], L["cabinet"], L["header"], L["screen"]
DECK = [(-0.06, 1.02), (0.02, 1.02), (0.02, 1.05), (-0.06, 1.17)]   # (z, y): the keypad wedge, slope = edge 2
HOOD = [(-0.06, 1.50), (0.03, 1.52), (0.03, 1.555), (-0.06, 1.555)]  # the small hood over the screen
CASH = {"x": 0.14, "y0": 0.86, "y1": 1.00, "front": 0.0}
FRONT_FASCIA_Z = FA["front_z"]


def make(level):
    p = Part("atm_L%d" % level)
    body_back = BACK + P["depth"] if level == 0 else BACK  # the body stands on the mounting plate (dropped in the LODs)
    if level == 0:
        p.box((-P["half_width"], P["y0"], BACK), (P["half_width"], P["y1"], BACK + P["depth"]), "ATM_Steel")
    # plinth and lower cabinet: brushed steel
    p.box((-PL["half_width"], 0.0, body_back), (PL["half_width"], PL["height"], PL["front_z"]), "ATM_Steel", skip=("-y",))
    p.box((-CA["half_width"], CA["y0"], body_back), (CA["half_width"], CA["y1"], CA["front_z"]), "ATM_Steel")
    if level == 0:
        for i in range(6):  # vent louvres on the cabinet front
            y = 0.22 + i * 0.075
            p.box((-0.22, y, CA["front_z"]), (0.22, y + 0.014, CA["front_z"] + 0.006), "ATM_Dark")
    # fascia: painted steel, the front is the atlas
    p.box((-FA["half_width"], FA["y0"], body_back), (FA["half_width"], FA["y1"], FA["front_z"]), "ATM_Paint", skip=("+z",))
    p.quad("ATM_Face", (-FA["half_width"], FA["y0"], FRONT_FASCIA_Z), (FA["half_width"], FA["y0"], FRONT_FASCIA_Z),
           (FA["half_width"], FA["y1"], FRONT_FASCIA_Z), (-FA["half_width"], FA["y1"], FRONT_FASCIA_Z), (0, 0, 1), R_FASCIA)
    # the sign box on top: painted steel, the front is the sign plate
    p.box((-HD["half_width"], HD["y0"], body_back), (HD["half_width"], HD["y1"], HD["front_z"]), "ATM_Paint", skip=("+z",))
    hx = HD["half_width"]
    sign_region = (R_HEADER[0], R_HEADER[1], 660, 160)
    p.quad("ATM_Face", (-hx, HD["y0"], HD["front_z"]), (hx, HD["y0"], HD["front_z"]), (hx, HD["y1"], HD["front_z"]),
           (-hx, HD["y1"], HD["front_z"]), (0, 0, 1), sign_region)
    # the screen: a glass quad, recessed behind a dark bezel
    sx = SC["half_width"]
    sz = FRONT_FASCIA_Z + 0.004 if level == 0 else FRONT_FASCIA_Z + 0.005
    p.quad("ATM_Face", (-sx, SC["y0"], sz), (sx, SC["y0"], sz), (sx, SC["y1"], sz), (-sx, SC["y1"], sz), (0, 0, 1),
           (R_SCREEN[0], R_SCREEN[1], R_SCREEN[2], R_SCREEN[3]))
    if level == 0:
        bz0, bz1 = FRONT_FASCIA_Z, FRONT_FASCIA_Z + 0.025
        for lo, hi in (((-0.16, SC["y1"], bz0), (0.16, 1.50, bz1)), ((-0.16, 1.24, bz0), (0.16, SC["y0"], bz1)),
                       ((-0.16, SC["y0"], bz0), (-sx, SC["y1"], bz1)), ((sx, SC["y0"], bz0), (0.16, SC["y1"], bz1))):
            p.box(lo, hi, "ATM_Dark")
        p.prism(HOOD, -0.17, 0.17, "ATM_Steel", skip_edges=(3,))
    # the keypad wedge and the cash housing
    deck_x = 0.15
    if level <= 1:
        p.prism(DECK, -deck_x, deck_x, "ATM_Dark", skip_edges=(2, 3))
        (z2, y2), (z3, y3) = DECK[2], DECK[3]
        nz, ny = (y3 - y2), -(z3 - z2)  # the slope's outward normal, toward +z and up
        p.quad("ATM_Face", (-deck_x, y2, z2), (deck_x, y2, z2), (deck_x, y3, z3), (-deck_x, y3, z3), (0, ny, nz), R_KEYPAD)
        c = CASH
        p.box((-c["x"], c["y0"], body_back), (c["x"], c["y1"], c["front"]), "ATM_Steel", skip=("+z",))
        p.quad("ATM_Face", (-c["x"], c["y0"], c["front"]), (c["x"], c["y0"], c["front"]), (c["x"], c["y1"], c["front"]),
               (-c["x"], c["y1"], c["front"]), (0, 0, 1), R_CASH)
    return p.finish()


# ================================================================ models, export
lod_objs = [make(i) for i in range(len(kit["lod_ratios"]) + 1)]
for i, o in enumerate(lod_objs):
    o.name = "ATMWall_LOD%d" % i
    o.data.name = "ATMWall_LOD%d" % i
cfg = kit["colliders"]["ATMWall"]
body = (-PL["half_width"], 0.0, BACK), (PL["half_width"], HD["y1"], FRONT_FASCIA_Z)
front = (-0.17, 0.86, FRONT_FASCIA_Z), (0.17, 1.56, 0.03)
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
shot(os.path.join(rv, "head_close.png"), (0, 1.62, 0), (0.15, 0.1, 1), 1.4, res=(1200, 700))
for i in range(1, len(lod_objs)):
    for o in lod_objs:
        o.hide_render = o is not lod_objs[i]
    shot(os.path.join(rv, "lod%d.png" % i), (0, 0.95, -0.1), (0.7, 0.2, 1), 3.6, res=(800, 900))

bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out, "atm-wall.blend"))
with open(os.path.join(out, "report.json"), "w") as f:
    json.dump(report, f, indent=1, default=float)
print("LF_KIT_BUILD ok", json.dumps({k: [l["tris"] for l in v["lods"]] for k, v in report["models"].items()}))
