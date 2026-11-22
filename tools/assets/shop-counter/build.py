# Builds the 1989 village shop counter (tools/assets/shop-counter/kit.json) in headless Blender:
#   blender --background --factory-startup --python build.py -- <job.json>
# Two models that share materials: ShopCounter (the counter alone) and ShopCounterTill (the counter with a cash
# register and a change mat). Both keep the vanilla CounterShop_01 footprint so a placed counter keeps its seat.
# Shared texture, geometry and export helpers: tools/assets/pipeline/blender/kitlib.py.
import sys, json, math
import numpy as np

job = json.load(open(sys.argv[sys.argv.index("--") + 1]))
sys.path.insert(0, job["blender_lib"])
import kitlib as K
from kitlib import noise, blur, normal_from_height, save_material, Canvas, Part

kit, out = K.init(job)
L = kit["layout_m"]
HW, BZ, FZ, TY, TT = L["half_width"], L["back_z"], L["front_z"], L["top_y"], L["top_thick"]
TX, TZ = L["till"]["x"], L["till"]["z"]
px = kit["texture_px"]["tile"]

# ================================================================ materials
# Varnished oak tongue-and-groove (lambris): vertical boards 10 cm wide with a dark V groove between them.
base, rough, hgt = K.wood(px, (0.50, 0.33, 0.19), grain_px=5, ring=0.12, rough=0.74, varnish=0.0)
cols = np.arange(px) % (px // 10)
groove = ((cols < 3) | (cols > px // 10 - 2)).astype(float)[None, :] * np.ones((px, 1))
board = (np.arange(px) // (px // 10))[None, :] * np.ones((px, 1))
base *= (1 + ((board * 0.37) % 1 - 0.5) * 0.10)[:, :, None]  # each board a slightly different tone
base = base * (1 - groove[:, :, None] * 0.55)
save_material("SC_Lambris", base, rough + groove * 0.2, 0.0, normal_from_height(hgt - blur(groove, 0.8) * 1.5, 1.0), 1.0)

# Dark-stained oak: the frame, the mouldings, the shelves.
base, rough, hgt = K.wood(px, (0.26, 0.15, 0.09), grain_px=4, ring=0.15, rough=0.72, varnish=0.0)
save_material("SC_WoodDark", base, rough, 0.0, normal_from_height(hgt, 0.8), 1.0)

# Brown wood-grain laminate worktop: flat, a fine print grain, a little worn pale where hands rest.
base, rough, hgt = K.wood(px, (0.36, 0.24, 0.15), grain_px=3, ring=0.08, along="u", rough=0.62, varnish=0.0)
wear = np.clip(noise(px, px, 60), 0, None) * 0.08
base = base * (1 + wear[:, :, None])
scr = np.zeros((px, px))
for _ in range(60):
    x, y = K.rng.uniform(0, px, 2)
    a = K.rng.uniform(0, math.pi)
    for t in range(int(K.rng.uniform(0.2, 1.0) * 30)):
        scr[int(y + math.sin(a) * t) % px, int(x + math.cos(a) * t) % px] = 1.0
base = base * (1 - scr[:, :, None] * 0.12)
save_material("SC_Top", base, rough + scr * 0.15 + wear, 0.0, normal_from_height(hgt * 0.15 - scr * 0.3, 0.6), 1.0)

# Aluminium edge strip, the kick plinth's black rubber.
b, r, m, h = K.plain(256, (0.66, 0.67, 0.68), 0.35, 0.95, grain=0.02)
streak = noise(256, 256, 60, 0.6)
save_material("SC_Trim", np.clip(b + streak[:, :, None] * 0.02, 0, 1), r + streak * 0.03, m, normal_from_height(streak * 0.1, 0.5), 0.5)
b, r, m, h = K.plain(128, (0.06, 0.06, 0.06), 0.8, 0.0)
save_material("SC_Dark", b, r, m, normal_from_height(h, 0.5), 0.5)

# ---------------------------------------------------------------- the till atlas (1 px = 1 mm)
AW, AH = kit["texture_px"]["till"]
CREAM = np.array([0.80, 0.76, 0.66])
KEYGREY = np.array([0.22, 0.22, 0.23])
cv = Canvas(AW, AH, CREAM, rough=0.5)
cv.base *= (1 + noise(AH, AW, 3) * 0.01)[:, :, None]
R_KEYS = (0, 0, 360, 270)       # the sloped keyboard, row 0 at the top (away from the clerk)
R_DRAWER = (0, 290, 360, 90)    # the cash drawer front
R_DISP = (380, 0, 150, 60)      # a figure display window (both faces of the post, the clerk's on the body)
R_KRAFT = (380, 80, 200, 200)   # kraft paper (bags and the wrapping-paper roll)
R_MAT = (600, 0, 250, 180)      # the rubber change mat
R_ROLL = (380, 300, 120, 120)   # the receipt roll's end (paper with a core)
R_GREY = (540, 300, 40, 40)     # plain key grey (the body's back plate)

# keyboard: a dark grey key field with cream numerals, coloured department keys, a big red total key
x0, y0, w, h = R_KEYS
cv.rect(x0, y0, x0 + w, y0 + h, KEYGREY, rough=0.6)
cv.rect(x0 + 8, y0 + 8, x0 + w - 8, y0 + 70, np.array([0.10, 0.11, 0.10]), rough=0.3)   # the clerk's display window
dsp = Canvas(w - 40, 40, (0.02, 0.05, 0.03))
dsp.text("0012.50", 0, 6, 4, (0.25, 0.95, 0.45), centre_in=(0, w - 40))
cv.base[y0 + 20:y0 + 60, x0 + 20:x0 + w - 20] = dsp.base
cv.em[y0 + 20:y0 + 60, x0 + 20:x0 + w - 20] = dsp.base * 0.7
KW, KH, GAP = 34, 30, 8


def key(kx, ky, rgb, label=None, lrgb=(0.15, 0.15, 0.15), kw=KW, kh=KH):
    cv.rect(kx, ky, kx + kw, ky + kh, rgb * 0.85, rough=0.45, height=0.6)
    cv.rect(kx + 3, ky + 2, kx + kw - 3, ky + kh - 5, rgb, rough=0.4, height=1.0)
    if label:
        cv.text(label, kx, ky + 8, 2, lrgb, centre_in=(kx, kx + kw), height=1.0)


pad = [["7", "8", "9"], ["4", "5", "6"], ["1", "2", "3"], ["0", "00", "."]]
for r_, row in enumerate(pad):
    for c_, lab in enumerate(row):
        key(x0 + 16 + c_ * (KW + GAP), y0 + 86 + r_ * (KH + GAP), CREAM, lab)
DEPT = [np.array(c) for c in ((0.70, 0.18, 0.15), (0.20, 0.35, 0.65), (0.22, 0.55, 0.28), (0.85, 0.68, 0.18))]
for r_ in range(4):
    for c_ in range(2):
        key(x0 + 160 + c_ * (KW + GAP), y0 + 86 + r_ * (KH + GAP), DEPT[(r_ + c_) % 4], str(r_ * 2 + c_ + 1), (0.95, 0.95, 0.9))
key(x0 + 252, y0 + 86, np.array([0.55, 0.55, 0.55]), "X", (0.1, 0.1, 0.1), kw=40)
key(x0 + 300, y0 + 86, np.array([0.55, 0.55, 0.55]), "C", (0.1, 0.1, 0.1), kw=40)
key(x0 + 252, y0 + 124, np.array([0.30, 0.45, 0.60]), "ST", (0.95, 0.95, 0.9), kw=88)
cv.rect(x0 + 252, y0 + 162, x0 + 340, y0 + 238, np.array([0.60, 0.14, 0.12]) * 0.85, height=0.6)
cv.rect(x0 + 255, y0 + 164, x0 + 337, y0 + 233, np.array([0.70, 0.17, 0.14]), rough=0.4, height=1.0)
cv.text("TOTAL", x0 + 252, y0 + 194, 2, (0.95, 0.93, 0.88), centre_in=(x0 + 252, x0 + 340), height=1.0)
cv.circle(x0 + 330, y0 + 252, 8, np.array([0.75, 0.75, 0.72]), metal=1.0, rough=0.3, height=0.8)  # the mode key lock
cv.rect(x0 + 326, y0 + 251, x0 + 334, y0 + 253, np.array([0.1, 0.1, 0.1]))

# drawer: cream front, a recessed finger lip along the top, a round key lock and a coin-slot paper slit
x0, y0, w, h = R_DRAWER
cv.rect(x0, y0, x0 + w, y0 + h, CREAM * 0.95, rough=0.5)
cv.rect(x0 + 10, y0 + 6, x0 + w - 10, y0 + 16, CREAM * 0.55, height=-1.0)
cv.circle(x0 + w - 40, y0 + 50, 9, np.array([0.70, 0.70, 0.68]), metal=1.0, rough=0.3, height=0.6)
cv.rect(x0 + w - 42, y0 + 45, x0 + w - 38, y0 + 55, np.array([0.1, 0.1, 0.1]))
cv.rect(x0, y0 + h - 4, x0 + w, y0 + h, CREAM * 0.6)

# figure display window: green digits on black, it glows
x0, y0, w, h = R_DISP
cv.rect(x0, y0, x0 + w, y0 + h, np.array([0.03, 0.04, 0.035]), rough=0.15)
d = Canvas(w - 16, h - 20, (0.02, 0.05, 0.03))
d.text("12.50", 0, 6, 4, (0.25, 0.95, 0.45), centre_in=(0, w - 16))
cv.base[y0 + 10:y0 + h - 10, x0 + 8:x0 + w - 8] = d.base
cv.em[y0 + 10:y0 + h - 10, x0 + 8:x0 + w - 8] = d.base * 0.8

# kraft paper: brown with long fibres and a faint fold line
x0, y0, w, h = R_KRAFT
k = np.ones((h, w, 3)) * np.array([0.60, 0.45, 0.30]) * (1 + noise(h, w, 2, 12)[:, :, None] * 0.05 + noise(h, w, 20)[:, :, None] * 0.04)
cv.paste(x0, y0, np.clip(k, 0, 1), rough=np.full((h, w), 0.85))
cv.rect(x0, y0 + h // 2, x0 + w, y0 + h // 2 + 2, np.array([0.50, 0.37, 0.24]))

# change mat: dark green rubber with raised pimples
x0, y0, w, h = R_MAT
cv.rect(x0, y0, x0 + w, y0 + h, np.array([0.10, 0.22, 0.14]), rough=0.85)
for yy in range(y0 + 10, y0 + h - 5, 12):
    for xx in range(x0 + 10, x0 + w - 5, 12):
        cv.circle(xx, yy, 3, np.array([0.12, 0.26, 0.16]), height=1.0)
cv.rect(x0, y0, x0 + w, y0 + 6, np.array([0.07, 0.15, 0.10]))
cv.rect(x0, y0 + h - 6, x0 + w, y0 + h, np.array([0.07, 0.15, 0.10]))

# receipt roll end: white paper rings round a grey card core
x0, y0, w, h = R_ROLL
cv.rect(x0, y0, x0 + w, y0 + h, np.array([0.92, 0.91, 0.87]))
for rr in range(20, 60, 3):
    cv.circle(x0 + w / 2, y0 + h / 2, rr, np.array([0.86, 0.85, 0.80]), ring=1)
cv.circle(x0 + w / 2, y0 + h / 2, 18, np.array([0.45, 0.43, 0.40]))
cv.circle(x0 + w / 2, y0 + h / 2, 12, np.array([0.12, 0.12, 0.12]))
cv.rect(R_GREY[0], R_GREY[1], R_GREY[0] + R_GREY[2], R_GREY[1] + R_GREY[3], KEYGREY, rough=0.6)
cv.save("SC_Till", strength=1.2, em=True)
b, r, m, h = K.plain(256, CREAM, 0.5, 0.0, grain=0.015)
save_material("SC_Plastic", b, r, m, normal_from_height(h, 0.3), 0.5)
k = np.ones((256, 256, 3)) * np.array([0.60, 0.45, 0.30]) * (1 + noise(256, 256, 2, 12)[:, :, None] * 0.05 + noise(256, 256, 20)[:, :, None] * 0.04)
save_material("SC_Kraft", np.clip(k, 0, 1), 0.85, 0.0, normal_from_height(noise(256, 256, 2) * 0.1, 0.4), 0.5)


# ================================================================ geometry
def counter(p, level):
    # the worktop: laminate top, aluminium edge, dark wood underside
    edge = {"+z": "SC_Trim", "-z": "SC_Trim", "+x": "SC_Trim", "-x": "SC_Trim", "-y": "SC_WoodDark"}
    p.box((-HW, TY - TT, BZ), (HW, TY, FZ), "SC_Top", mats=edge)
    cx = HW - 0.03          # carcass half width
    cf = FZ - 0.037         # carcass front plane (0.56)
    cb = BZ + 0.035         # carcass back edge (-0.49)
    yb, yt = 0.08, TY - TT  # carcass bottom and top
    # kick plinth, set back 3 cm
    p.box((-cx + 0.02, 0.0, cb + 0.03), (cx - 0.02, yb, cf - 0.03), "SC_Dark", skip=("-y", "+y"))
    if level == 2:
        p.box((-cx, yb, cb), (cx, yt, cf), "SC_WoodDark", mats={"+z": "SC_Lambris", "+x": "SC_Lambris", "-x": "SC_Lambris"}, skip=("+y",))
        return
    # the carcass: two sides, the front, the bottom, a middle shelf; open toward the clerk (-z)
    t = 0.02
    p.box((-cx, yb, cb), (-cx + t, yt, cf), "SC_WoodDark", skip=("+y",))
    p.box((cx - t, yb, cb), (cx, yt, cf), "SC_WoodDark", skip=("+y",))
    p.box((-cx + t, yb, cf - t), (cx - t, yt, cf), "SC_WoodDark", skip=("+y", "+x", "-x"))
    p.box((-cx + t, yb, cb), (cx - t, yb + t, cf - t), "SC_WoodDark", skip=("-y", "+x", "-x", "+z"))
    p.box((-cx + t, 0.50, cb + 0.02), (cx - t, 0.50 + t, cf - t), "SC_WoodDark", skip=("+x", "-x", "+z"))
    p.box((-cx + t, yt - 0.06, cb), (cx - t, yt, cb + t), "SC_WoodDark", skip=("+y", "+x", "-x"))  # the top rail
    # front: three raised lambris panels in the dark frame, a moulding under the top
    pw = (2 * (cx - 0.04) - 2 * 0.06) / 3
    for i in range(3):
        a = -cx + 0.04 + i * (pw + 0.06)
        if level == 0:
            p.box((a, 0.16, cf), (a + pw, 0.90, cf + 0.010), "SC_Lambris", mats={"+x": "SC_WoodDark", "-x": "SC_WoodDark", "+y": "SC_WoodDark", "-y": "SC_WoodDark"}, skip=("-z",))
        else:
            p.face([K.B(a, 0.16, cf + 0.001), K.B(a + pw, 0.16, cf + 0.001), K.B(a + pw, 0.90, cf + 0.001), K.B(a, 0.90, cf + 0.001)], "SC_Lambris", want=K.B(0, 0, 1))
    # the sides: one panel each
    for s in (-1, 1):
        if level == 0:
            lo, hi = sorted((s * cx, s * (cx + 0.010)))
            p.box((lo, 0.16, cb + 0.06), (hi, 0.90, cf - 0.06), "SC_Lambris", mats={"+z": "SC_WoodDark", "-z": "SC_WoodDark", "+y": "SC_WoodDark", "-y": "SC_WoodDark"}, skip=("-x",) if s > 0 else ("+x",))
    if level == 0:
        p.box((-cx - 0.012, 0.93, cb), (cx + 0.012, 0.965, cf + 0.018), "SC_WoodDark", skip=("-z",))  # moulding
        p.box((-cx - 0.006, yb, cb), (cx + 0.006, yb + 0.03, cf + 0.008), "SC_WoodDark", skip=("-z", "-y"))  # skirting
        # the clerk's shelves: a stack of folded kraft bags, a wrapping-paper roll
        p.box((-0.60, yb + t, -0.40), (-0.28, yb + t + 0.09, -0.10), "SC_Till",
              regions={"+y": ("SC_Till", R_KRAFT), "-z": ("SC_Till", R_KRAFT), "+x": ("SC_Till", R_KRAFT), "-x": ("SC_Till", R_KRAFT), "+z": ("SC_Till", R_KRAFT)}, skip=("-y",))
        p.cyl((-0.62, 0.52 + 0.05, -0.30), 0.05, 0.55, "x", "SC_Kraft", segs=10, cap_mat="SC_Till", cap_region=R_ROLL)


def till(p, level):
    # an early-electronic cash register on the clerk's half of the top, its keyboard toward the clerk (-z)
    x0, x1 = TX - 0.18, TX + 0.18
    z0, z1 = TZ - 0.21, TZ + 0.21
    y0 = TY
    yd = y0 + 0.09   # drawer top
    if level == 2:
        p.box((x0, y0, z0), (x1, yd + 0.10, z1), "SC_Plastic", regions={"-z": ("SC_Till", R_DRAWER)}, skip=("-y",))
        return
    # the drawer
    p.box((x0, y0, z0), (x1, yd, z1), "SC_Plastic", regions={"-z": ("SC_Till", R_DRAWER)}, skip=("-y", "+y"))
    # the body: flat back, a sloped keyboard toward the clerk, a short lip at the front
    prof = [(z0, yd), (z1, yd), (z1, yd + 0.12), (z0 + 0.25, yd + 0.12), (z0, yd + 0.035)]
    p.prism(prof, x0 + 0.005, x1 - 0.005, "SC_Plastic", skip_edges=(3,))
    (za, ya), (zb, yb) = prof[3], prof[4]
    # the keyboard quad: bl at the clerk's near left; facing +z the clerk has +x on the left (kitlib box "-z")
    ny, nz = abs(za - zb), -abs(ya - yb)
    p.quad("SC_Till", (x1 - 0.005, yb, zb), (x0 + 0.005, yb, zb), (x0 + 0.005, ya, za), (x1 - 0.005, ya, za), (0, ny, nz), R_KEYS)
    # the receipt roll behind the keyboard, and the display post toward the customer
    if level == 0:
        p.cyl((x0 + 0.03, yd + 0.12 + 0.035, z0 + 0.32), 0.035, 0.10, "x", "SC_Plastic", segs=10, cap_mat="SC_Till", cap_region=R_ROLL)
    px_, pz = x1 - 0.06, z1 - 0.05
    p.box((px_ - 0.012, yd + 0.12, pz - 0.012), (px_ + 0.012, yd + 0.20, pz + 0.012), "SC_Dark", skip=("-y",))
    hb = (px_ - 0.08, yd + 0.20, pz - 0.03), (px_ + 0.08, yd + 0.26, pz + 0.03)
    p.box(*hb, "SC_Dark", regions={"+z": ("SC_Till", R_DISP), "-z": ("SC_Till", R_DISP)})
    # the change mat on the customer half, in front of the till
    mz0 = 0.15
    p.box((TX - 0.125, TY, mz0), (TX + 0.125, TY + 0.004, mz0 + 0.18), "SC_Dark",
          regions={"+y": ("SC_Till", R_MAT)}, skip=("-y",))


def make(name, level, with_till):
    p = Part("%s_L%d" % (name, level))
    counter(p, level)
    if with_till:
        till(p, level)
    return p.finish()


COUNTER_BOX = ((-HW, 0.0, BZ), (HW, TY, FZ))
for name, with_till in (("ShopCounter", False), ("ShopCounterTill", True)):
    lods = [make(name, lv, with_till) for lv in range(3)]
    boxes = [COUNTER_BOX]
    if with_till:
        boxes.append(((TX - 0.18, TY, TZ - 0.21), (TX + 0.18, TY + 0.23, TZ + 0.21)))
    K.export_model(name, lods, boxes)

# ================================================================ review renders
K.review_setup()
for name in ("ShopCounter", "ShopCounterTill"):
    K.show_only([o for o in __import__("bpy").data.objects if o.name == name + "_LOD0"])
    K.shot(name + "_front.png", (0, 0.6, 0), (0, 0.15, 1), 4.0)
    K.shot(name + "_three_quarter.png", (0, 0.6, 0), (0.8, 0.45, 1), 3.6)
    K.shot(name + "_clerk.png", (0, 0.7, 0), (-0.6, 0.7, -1), 3.4)
    if name == "ShopCounterTill":
        K.shot("till_close_clerk.png", (TX, TY + 0.15, TZ), (0.0, 0.9, -1), 1.0)
        K.shot("till_close_customer.png", (TX, TY + 0.15, TZ), (-0.3, 0.4, 1), 1.2)
    for lv in (1, 2):
        K.show_only([o for o in __import__("bpy").data.objects if o.name == "%s_LOD%d" % (name, lv)])
        K.shot("%s_lod%d.png" % (name, lv), (0, 0.6, 0), (0.8, 0.45, 1), 3.6, res=(600, 540))
K.finish("shop-counter")
