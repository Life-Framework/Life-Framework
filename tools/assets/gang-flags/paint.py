# The twenty preset gang flags (#4219, gangs C1, decision N20). Kind "material", run by the generate stage:
#   blender --background --factory-startup --python paint.py -- <job.json>
# job.json = { spec, raw, px, variants: [{ name, plan }] }. Writes <raw>/<Variant>/tex_{base_color,roughness,
# metallic,normal}.png for each variant, square px by px. The vanilla flag cloth (Flag_1_2.xob) maps the whole
# texture onto a 2 : 1 cloth, so every design is laid out on a 2 x 1 canvas and squeezed into the square: a
# disc is round on the cloth. Flat dyed cloth only: bands, a cross, a chevron, discs, a star. No lettering and
# no real-world group, party, or national symbols; each flag is built from the LAYERS table below.
import bpy, json, math, os, sys
import numpy as np

job = json.load(open(sys.argv[sys.argv.index("--") + 1]))
raw, px = job["raw"], job["px"]

# Dyed cloth colours (sRGB 0..1), a little off pure so nothing reads as a screen colour.
C = {
    "red": (0.62, 0.09, 0.08), "crimson": (0.48, 0.05, 0.08), "navy": (0.07, 0.12, 0.30), "white": (0.90, 0.88, 0.82),
    "black": (0.05, 0.05, 0.05), "green": (0.10, 0.34, 0.16), "dkgreen": (0.06, 0.20, 0.11), "yellow": (0.88, 0.70, 0.12),
    "gold": (0.72, 0.52, 0.10), "copper": (0.70, 0.36, 0.14), "brown": (0.25, 0.14, 0.08), "grey": (0.42, 0.43, 0.44),
    "silver": (0.68, 0.69, 0.70), "purple": (0.30, 0.10, 0.38), "tan": (0.72, 0.60, 0.40), "teal": (0.06, 0.38, 0.40),
    "blue": (0.10, 0.28, 0.62), "orange": (0.85, 0.40, 0.08), "rust": (0.55, 0.22, 0.10), "cream": (0.86, 0.80, 0.62),
    "ash": (0.62, 0.62, 0.58),
}

# Canvas: x 0..2 (hoist to fly), y 0..1 (top to bottom).
Y, X = np.mgrid[0:px, 0:px]
x = (X + 0.5) / px * 2.0
y = (Y + 0.5) / px


def star(cx, cy, r, n=5, inner=0.4):
    """Mask of an n-point star."""
    ang = np.arctan2(y - cy, x - cx) + math.pi / 2
    sector = (2 * math.pi) / n
    t = (ang % sector) / sector
    # Boundary radius between outer tip (t = 0) and inner notch (t = 0.5), linear in angle.
    edge = 1.0 - np.abs(t * 2.0 - 1.0)
    rr = r * (inner + (1.0 - inner) * (1.0 - edge))
    return np.hypot(x - cx, y - cy) <= rr


def tri(p0, p1, p2):
    """Mask of a triangle."""
    def s(a, b):
        return (x - b[0]) * (a[1] - b[1]) - (a[0] - b[0]) * (y - b[1])
    d1, d2, d3 = s(p0, p1), s(p1, p2), s(p2, p0)
    neg = (d1 < 0) | (d2 < 0) | (d3 < 0)
    pos = (d1 > 0) | (d2 > 0) | (d3 > 0)
    return ~(neg & pos)


def box(x0, y0, x1, y1):
    return (x >= x0) & (x < x1) & (y >= y0) & (y < y1)


def disc(cx, cy, r):
    return np.hypot(x - cx, y - cy) <= r


def diamond(cx, cy, rx, ry):
    return (np.abs(x - cx) / rx + np.abs(y - cy) / ry) <= 1.0


def band(p, w):
    """Diagonal band: |x - 2y... distance to the line from the hoist's top corner to the fly's bottom corner|."""
    d = (y * 2.0 - x) / math.sqrt(5.0)  # 0 on the main diagonal (0,0)-(2,1)
    return np.abs(d - p) <= w


def other_band(p, w):
    d = ((1.0 - y) * 2.0 - x) / math.sqrt(5.0)  # the other diagonal (0,1)-(2,0)
    return np.abs(d - p) <= w


# name -> (field, [(mask, colour), ...]); later layers draw over earlier ones.
LAYERS = {
    "Ember": ("crimson", [(tri((0, 0), (0, 1), (0.9, 0.5)), "black"), (tri((0, 0.18), (0, 0.82), (0.6, 0.5)), "orange")]),
    "Harbor": ("navy", [(box(0, 0.38, 2, 0.62), "white"), (box(0, 0.47, 2, 0.53), "red")]),
    "Marrow": ("cream", [(disc(1.0, 0.5, 0.36), "red"), (disc(1.0, 0.5, 0.24), "black")]),
    "Thornfield": ("dkgreen", [(band(0, 0.07), "yellow"), (other_band(0, 0.07), "yellow")]),
    "Copperhead": ("copper", [(box(0, 0, 0.45, 1), "brown"), (star(1.25, 0.5, 0.3), "brown")]),
    "Nightjar": ("black", [(diamond(1.0, 0.5, 0.62, 0.36), "grey"), (diamond(1.0, 0.5, 0.36, 0.2), "black")]),
    "Salt": ("white", [(box(0, 0.12, 2, 0.28), "blue"), (box(0, 0.42, 2, 0.58), "blue"), (box(0, 0.72, 2, 0.88), "blue")]),
    "Gallows": ("black", [(box(1.0, 0, 2, 1), "red")]),
    "Lantern": ("yellow", [(disc(1.0, 0.5, 0.26), "red")]),
    "Ironside": ("grey", [(box(0.55, 0, 0.85, 1), "black"), (box(0, 0.35, 2, 0.65), "black")]),
    "Cinder": ("orange", [(tri((0, 1), (2, 1), (2, 0)), "black")]),
    "Vesper": ("purple", [(star(1.0, 0.52, 0.34), "gold")]),
    "Quarry": ("tan", [(tri((0, 0), (0, 1), (1.1, 0.5)), "brown")]),
    "Rook": ("red", [(box(0, 0, 0.7, 0.5), "white"), (box(0.7, 0.5, 2, 0.62), "black")]),
    "Tidewater": ("teal", [(diamond(1.0, 0.5, 0.5, 0.4), "white")]),
    "Brass": ("gold", [(box(0, 0.22, 2, 0.34), "black"), (box(0, 0.66, 2, 0.78), "black")]),
    "Wolfsbane": ("dkgreen", [(box(0.8, 0, 1.2, 1), "white")]),
    "Ashen": ("ash", [(((np.floor(x / 0.25) + np.floor(y / 0.25)) % 2) == 0, "black")]),
    "Rustler": ("rust", [(band(0, 0.1), "cream"), (other_band(0, 0.1), "cream")]),
    "Lowtide": ("blue", [(box(0, 0.5, 2, 1), "yellow"), (disc(1.0, 0.5, 0.2), "white")]),
}


def weave():
    """A faint woven grain so a flat colour does not read as plastic; the same for every flag."""
    rng = np.random.default_rng(1989)
    warp = np.sin(X * 2.0 * math.pi / 4.0) * 0.5 + np.sin(Y * 2.0 * math.pi / 4.0) * 0.5
    return 1.0 + 0.035 * warp + 0.03 * (rng.random((px, px)) - 0.5)


def save(path, rgb, data):
    rgba = np.ones((px, px, 4), np.float32)
    rgba[:, :, :3] = rgb
    im = bpy.data.images.new(os.path.basename(path), px, px, alpha=False, is_data=True)
    im.pixels.foreach_set(rgba[::-1].ravel())
    im.filepath_raw = path
    im.file_format = "PNG"
    im.save()
    bpy.data.images.remove(im)


grain = weave()
for v in job["variants"]:
    name = v["name"]
    if name not in LAYERS:
        raise SystemExit("no design for variant %s in paint.py LAYERS" % name)
    field, layers = LAYERS[name]
    img = np.zeros((px, px, 3), np.float32)
    img[:, :] = C[field]
    for mask, col in layers:
        img[mask] = C[col]
    # The top and bottom hem: a thin darker edge, the way a sewn flag has.
    hem = (y < 0.012) | (y > 0.988)
    img[hem] *= 0.8
    img = np.clip(img * grain[:, :, None], 0.0, 1.0)
    out = os.path.join(raw, name)
    os.makedirs(out, exist_ok=True)
    save(os.path.join(out, "tex_base_color.png"), img, False)
    save(os.path.join(out, "tex_roughness.png"), np.dstack([np.full((px, px), 0.88, np.float32)] * 3), True)
    save(os.path.join(out, "tex_metallic.png"), np.zeros((px, px, 3), np.float32), True)
    save(os.path.join(out, "tex_normal.png"), np.dstack([np.full((px, px), 0.5, np.float32), np.full((px, px), 0.5, np.float32), np.ones((px, px), np.float32)]), True)
    print("painted", name)
