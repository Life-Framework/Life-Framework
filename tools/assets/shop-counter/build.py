# Builds the 1989 village shop counter (tools/assets/shop-counter/kit.json) in headless Blender:
#   blender --background --factory-startup --python build.py -- <job.json>
# Two models that share materials: ShopCounter (the counter alone) and ShopCounterTill (the counter with a 1980s
# electromechanical cash register, a service bell, a receipt spike and a change mat). Both keep the vanilla
# CounterShop_01 footprint so a placed counter keeps its seat. Helpers: tools/assets/pipeline/blender/kitlib.py.
# All printed text is US English.
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
# Panel veneer: a plain mid-tone oak, soft wide grain, satin. 2026-11-24 (#4903): the mid oak read near-black in the dim
# shops; the honey fix then glared orange with hard vertical streaks (4 px grain, strong rings, deep normal). Now a calmer,
# greyer brown with broad low-contrast grain, so it reads as a plain 1980s counter at eye height.
base, rough, hgt = K.wood(px, (0.86, 0.75, 0.60), grain_px=10, ring=0.04, rough=0.80)
save_material("SC_Lambris", base, rough, 0.0, normal_from_height(hgt, 0.25), 1.0)

# Frame and mouldings: the same oak stained darker, brown rather than red.
base, rough, hgt = K.wood(px, (0.68, 0.57, 0.44), grain_px=8, ring=0.05, rough=0.82)
save_material("SC_WoodDark", base, rough, 0.0, normal_from_height(hgt, 0.3), 1.0)

# Worktop: beige speckled laminate, worn a little paler where hands rest, a few scratches.
b = np.ones((px, px, 3)) * np.array([0.66, 0.61, 0.52])
spk = noise(px, px, 0.7)
b *= (1 + spk[:, :, None] * 0.035 + noise(px, px, 25)[:, :, None] * 0.02)
dots = noise(px, px, 0.8) > 2.2
b[dots] = b[dots] * 0.72
wear = np.clip(noise(px, px, 60), 0, None) * 0.05
b *= (1 + wear[:, :, None])
scr = np.zeros((px, px))
for _ in range(60):
    x, y = K.rng.uniform(0, px, 2)
    a = K.rng.uniform(0, math.pi)
    for t in range(int(K.rng.uniform(0.2, 1.0) * 30)):
        scr[int(y + math.sin(a) * t) % px, int(x + math.cos(a) * t) % px] = 1.0
b = b * (1 - scr[:, :, None] * 0.08)
save_material("SC_Top", np.clip(b, 0, 1), 0.5 + scr * 0.15 - wear, 0.0, normal_from_height(spk * 0.05 - scr * 0.3, 0.6), 1.0)

# Brushed steel (edge, kick plate, crank, bell dome, spike), black rubber and plastic.
b, r, m, h = K.plain(256, (0.62, 0.63, 0.64), 0.38, 0.95, grain=0.02)
streak = noise(256, 256, 60, 0.6)
save_material("SC_Trim", np.clip(b + streak[:, :, None] * 0.03, 0, 1), r + streak * 0.03, m, normal_from_height(streak * 0.1, 0.5), 0.5)
b, r, m, h = K.plain(128, (0.06, 0.06, 0.06), 0.75, 0.0)
save_material("SC_Dark", b, r, m, normal_from_height(h, 0.5), 0.5)
# The register casing: a hammered grey-green enamel on steel.
b, r, m, h = K.plain(256, (0.46, 0.49, 0.45), 0.42, 0.5, grain=0.03, bump=0.4, feature=2)
save_material("SC_Plastic", b, r, m, normal_from_height(h, 0.8), 0.25)
k = np.ones((256, 256, 3)) * np.array([0.60, 0.45, 0.30]) * (1 + noise(256, 256, 2, 12)[:, :, None] * 0.05 + noise(256, 256, 20)[:, :, None] * 0.04)
save_material("SC_Kraft", np.clip(k, 0, 1), 0.85, 0.0, normal_from_height(noise(256, 256, 2) * 0.1, 0.4), 0.5)

# ---------------------------------------------------------------- the till atlas (1 px = 1 mm)
AW, AH = kit["texture_px"]["till"]
CASE = np.array([0.46, 0.49, 0.45])
IVORY = np.array([0.88, 0.85, 0.76])
cv = Canvas(AW, AH, CASE, rough=0.45)
cv.metal[:] = 0.5
R_DRAWER = (0, 0, 400, 110)      # the cash drawer front (clerk side)
R_DISP = (420, 0, 160, 90)       # the amount tower window (both faces)
R_KRAFT = (600, 0, 200, 200)     # kraft paper (bags, wrapping roll)
R_MAT = (0, 130, 250, 180)       # the rubber change mat
R_ROLL = (260, 130, 120, 120)    # a paper roll end
R_SLIP = (400, 130, 70, 100)     # a receipt slip
R_WIN = (480, 130, 110, 60)      # the journal window on the casing top
R_TOTAL = (820, 0, 50, 140)      # the long TOTAL bar key top
R_KEYS = (0, 330, 10 * 30, 30)   # key tops "0".."9", 30 px cells
R_DEPT = (0, 370, 4 * 30, 30)    # four colored department key tops

x0, y0, w, h = R_DRAWER
cv.rect(x0, y0, x0 + w, y0 + h, CASE * 0.92, rough=0.45, metal=0.5)
cv.rect(x0 + 8, y0 + 8, x0 + w - 8, y0 + h - 8, CASE, height=0.5)
cv.rect(x0 + 120, y0 + 20, x0 + 280, y0 + 34, np.array([0.70, 0.70, 0.68]), rough=0.3, metal=1.0, height=1.0)  # pull
cv.circle(x0 + 340, y0 + 60, 10, np.array([0.72, 0.62, 0.36]), rough=0.3, metal=1.0, height=0.8)             # brass lock
cv.rect(x0 + 338, y0 + 54, x0 + 342, y0 + 66, np.array([0.1, 0.1, 0.1]))
cv.text("CASH", 0, y0 + 64, 2, IVORY, centre_in=(x0 + 120, x0 + 280))

x0, y0, w, h = R_DISP
cv.rect(x0, y0, x0 + w, y0 + h, np.array([0.08, 0.08, 0.08]), rough=0.2, metal=0.0)
cv.text("AMOUNT", 0, y0 + 6, 2, IVORY, centre_in=(x0, x0 + w))
for i, ch in enumerate("12.50"):   # white tab digits on black flags, as the pop-up tabs read
    cx = x0 + 18 + i * 26
    cv.rect(cx, y0 + 30, cx + 22, y0 + 80, np.array([0.92, 0.90, 0.84]), rough=0.5)
    cv.text(ch, cx + 1, y0 + 41, 4, np.array([0.08, 0.08, 0.08]), centre_in=(cx, cx + 22))

x0, y0, w, h = R_KRAFT
kk = np.ones((h, w, 3)) * np.array([0.60, 0.45, 0.30]) * (1 + noise(h, w, 2, 12)[:, :, None] * 0.05)
cv.paste(x0, y0, np.clip(kk, 0, 1), rough=np.full((h, w), 0.85), metal=np.zeros((h, w)))

x0, y0, w, h = R_MAT
cv.rect(x0, y0, x0 + w, y0 + h, np.array([0.10, 0.22, 0.14]), rough=0.85, metal=0.0)
for yy in range(y0 + 10, y0 + h - 5, 12):
    for xx in range(x0 + 10, x0 + w - 5, 12):
        cv.circle(xx, yy, 3, np.array([0.12, 0.26, 0.16]), height=1.0)
cv.text("THANK YOU", 0, y0 + h // 2 - 7, 2, np.array([0.80, 0.72, 0.30]), centre_in=(x0, x0 + w))

x0, y0, w, h = R_ROLL
cv.rect(x0, y0, x0 + w, y0 + h, np.array([0.92, 0.91, 0.87]), metal=0.0, rough=0.8)
for rr in range(20, 60, 3):
    cv.circle(x0 + w / 2, y0 + h / 2, rr, np.array([0.86, 0.85, 0.80]), ring=1)
cv.circle(x0 + w / 2, y0 + h / 2, 18, np.array([0.45, 0.43, 0.40]))
cv.circle(x0 + w / 2, y0 + h / 2, 12, np.array([0.12, 0.12, 0.12]))

x0, y0, w, h = R_SLIP
cv.rect(x0, y0, x0 + w, y0 + h, np.array([0.93, 0.92, 0.88]), rough=0.85, metal=0.0)
for i in range(8):
    cv.rect(x0 + 8, y0 + 10 + i * 10, x0 + 8 + int(K.rng.uniform(25, 55)), y0 + 13 + i * 10, np.array([0.35, 0.35, 0.40]))

x0, y0, w, h = R_WIN
cv.rect(x0, y0, x0 + w, y0 + h, np.array([0.20, 0.21, 0.20]), rough=0.1, metal=0.0)
cv.rect(x0 + 10, y0 + 8, x0 + w - 10, y0 + h - 8, np.array([0.80, 0.79, 0.74]) * 0.7, rough=0.1)
for i in range(4):
    cv.rect(x0 + 18, y0 + 14 + i * 9, x0 + 70, y0 + 16 + i * 9, np.array([0.3, 0.3, 0.35]))

x0, y0, w, h = R_TOTAL
cv.rect(x0, y0, x0 + w, y0 + h, np.array([0.72, 0.16, 0.13]), rough=0.4, metal=0.0, height=0.6)
lab = Canvas(h, w, np.array([0.72, 0.16, 0.13]))
lab.text("TOTAL", 0, 18, 2, IVORY, centre_in=(0, h))
cv.base[y0:y0 + h, x0:x0 + w] = np.rot90(lab.base, 1)   # read along the bar from the clerk's side


def keytop(cx, cy, rgb, label, lrgb):
    c = Canvas(30, 30, rgb * 0.8)
    c.circle(15, 15, 13, rgb)
    c.text(label, 0, 8, 2, lrgb, centre_in=(0, 30))
    sub = (slice(cy, cy + 30), slice(cx, cx + 30))
    cv.base[sub] = np.rot90(c.base, 2)   # +y faces are upright for the customer side; the clerk reads them turned
    cv.metal[sub] = 0.0
    cv.rough[sub] = 0.35
    cv.height[sub] = c.base.mean(axis=2) * 0  # flat; the key itself is geometry


for d in range(10):
    keytop(R_KEYS[0] + d * 30, R_KEYS[1], IVORY, str(d), np.array([0.1, 0.1, 0.1]))
for i, col in enumerate(((0.20, 0.38, 0.65), (0.22, 0.55, 0.28), (0.85, 0.68, 0.18), (0.55, 0.55, 0.55))):
    keytop(R_DEPT[0] + i * 30, R_DEPT[1], np.array(col), "ABCD"[i], np.array([0.95, 0.95, 0.9]))
cv.save("SC_Till", strength=1.0, em=True)


def cell(region, i):
    return (region[0] + i * 30 + 2, region[1] + 2, 26, 26)


# ================================================================ geometry
def counter(p, level):
    edge = {"+z": "SC_Trim", "-z": "SC_Trim", "+x": "SC_Trim", "-x": "SC_Trim", "-y": "SC_WoodDark"}
    p.box((-HW, TY - TT, BZ), (HW, TY, FZ), "SC_Top", mats=edge)
    cx = HW - 0.03                    # carcass half width
    cf = FZ - 0.055                   # carcass front plane: the panels sit here
    cb = BZ + 0.035
    yt = TY - TT
    kick = 0.10                       # kick plate height
    if level == 2:
        p.box((-cx, 0.0, cb), (cx, yt, cf + 0.02), "SC_WoodDark", mats={"+z": "SC_Lambris"}, skip=("+y", "-y"))
        return
    t = 0.02
    # carcass: sides, front, bottom, a middle shelf; open toward the clerk (-z)
    p.box((-cx, kick, cb), (-cx + t, yt, cf), "SC_WoodDark", skip=("+y",))
    p.box((cx - t, kick, cb), (cx, yt, cf), "SC_WoodDark", skip=("+y",))
    p.box((-cx + t, kick, cf - t), (cx - t, yt, cf), "SC_Lambris", mats={"-z": "SC_WoodDark"}, skip=("+y", "+x", "-x"))
    p.box((-cx + t, kick, cb), (cx - t, kick + t, cf - t), "SC_WoodDark", skip=("-y", "+x", "-x", "+z"))
    p.box((-cx + t, 0.50, cb + 0.02), (cx - t, 0.50 + t, cf - t), "SC_WoodDark", skip=("+x", "-x", "+z"))
    p.box((-cx + t, yt - 0.06, cb), (cx - t, yt, cb + t), "SC_WoodDark", skip=("+y", "+x", "-x"))
    # plinth: black, set back, and a brushed steel kick plate across the front and sides
    p.box((-cx + 0.01, 0.0, cb + 0.02), (cx - 0.01, kick, cf - 0.005), "SC_Dark", skip=("-y", "+y", "+z"))
    p.box((-cx - 0.004, 0.0, cf - 0.006), (cx + 0.004, kick, cf + 0.024), "SC_Trim", skip=("-y", "-z"))
    # the front frame, proud of the panels: two end stiles, two middle stiles, a bottom rail, a top rail
    fp = 0.022                        # frame depth in front of the panels
    st = 0.07                         # stile width
    y0, y1 = kick, 0.86
    nb = 3
    bay = (2 * cx - (nb + 1) * st) / nb
    for i in range(nb + 1):
        a = -cx + i * (st + bay)
        p.box((a, y0, cf), (a + st, y1, cf + fp), "SC_WoodDark", skip=("-z",))
    p.box((-cx, y0, cf), (cx, y0 + 0.07, cf + fp), "SC_WoodDark", skip=("-z",))
    p.box((-cx, y1 - 0.07, cf), (cx, y1, cf + fp), "SC_WoodDark", skip=("-z",))
    if level == 0:
        # a bevelled moulding inside each panel opening
        m = 0.014
        for i in range(nb):
            a = -cx + st + i * (st + bay)
            b0, b1 = y0 + 0.07, y1 - 0.07
            for lo, hi in (((a, b0, cf), (a + m, b1, cf + 0.010)), ((a + bay - m, b0, cf), (a + bay, b1, cf + 0.010)),
                           ((a + m, b0, cf), (a + bay - m, b0 + m, cf + 0.010)), ((a + m, b1 - m, cf), (a + bay - m, b1, cf + 0.010))):
                p.box(lo, hi, "SC_WoodDark", skip=("-z",))
    # the moulded top rail: a deep fascia board under the top, a bead, and a cove step
    p.box((-cx - 0.005, y1, cf), (cx + 0.005, yt, cf + fp + 0.008), "SC_WoodDark", skip=("-z",))
    if level == 0:
        p.prism([(cf + fp + 0.008, y1 - 0.004), (cf + fp + 0.024, y1 + 0.02), (cf + fp + 0.024, y1 + 0.035), (cf + fp + 0.008, y1 + 0.035)],
                -cx - 0.005, cx + 0.005, "SC_WoodDark")
        p.box((-cx - 0.005, yt - 0.03, cf + fp + 0.008), (cx + 0.005, yt - 0.012, cf + fp + 0.018), "SC_WoodDark", skip=("-z",))
    # the sides: the same frame with one recessed panel each
    for s in (-1, 1):
        lo_x, hi_x = sorted((s * cx, s * (cx + fp)))
        out = "-x" if s > 0 else "+x"
        p.box((lo_x, y0, cb), (hi_x, y1, cb + st), "SC_WoodDark", skip=(out,))
        p.box((lo_x, y0, cf + fp - st), (hi_x, y1, cf + fp), "SC_WoodDark", skip=(out,))
        p.box((lo_x, y0, cb + st), (hi_x, y0 + 0.07, cf + fp - st), "SC_WoodDark", skip=(out,))
        p.box((lo_x, y1 - 0.07, cb + st), (hi_x, yt, cf + fp - st), "SC_WoodDark", skip=(out,))
        pl = sorted((s * cx, s * (cx + 0.004)))
        p.box((pl[0], y0 + 0.07, cb + st), (pl[1], y1 - 0.07, cf + fp - st), "SC_Lambris", skip=(out,))
    if level == 0:
        # the clerk's shelves: a stack of folded kraft bags, a wrapping-paper roll
        p.box((-0.60, kick + t, -0.40), (-0.28, kick + t + 0.09, -0.10), "SC_Kraft", skip=("-y",))
        p.cyl((-0.62, 0.52 + 0.05, -0.30), 0.05, 0.55, "x", "SC_Kraft", segs=10, cap_mat="SC_Till", cap_region=R_ROLL)


def register(p, level):
    # A 1980s electromechanical register on the clerk's half: a cash drawer, a terraced keyboard of round keys toward
    # the clerk (-z), a long TOTAL bar, a hand crank on the right side, a journal window, a pop-up AMOUNT tower.
    x0, x1 = TX - 0.21, TX + 0.21
    z0, z1 = TZ - 0.27, TZ + 0.21     # -0.49 .. -0.01
    y0 = TY
    yd = y0 + 0.12                    # drawer top
    if level == 2:
        p.box((x0, y0, z0), (x1, yd + 0.20, z1), "SC_Plastic", regions={"-z": ("SC_Till", R_DRAWER)}, skip=("-y",))
        return
    p.box((x0, y0, z0), (x1, yd, z1), "SC_Plastic", regions={"-z": ("SC_Till", R_DRAWER)}, skip=("-y",))
    # the keyboard: five terraces rising away from the clerk
    rows, rd, rh = 5, 0.045, 0.028
    kz0 = z0 + 0.01
    for r in range(rows):
        za, zb = kz0 + r * rd, kz0 + (r + 1) * rd
        ya = yd + (r + 1) * rh
        p.box((x0 + 0.005, yd, za), (x1 - 0.005, ya, zb), "SC_Plastic", skip=("-y",))
        if level == 0:
            for c in range(6):          # six amount columns from the clerk's left (+x), digits 1..5 rising away
                kx = x1 - 0.057 - c * 0.048
                d = r + 1
                p.box((kx, ya, za + 0.008), (kx + 0.032, ya + 0.012, za + 0.038), "SC_Dark",
                      regions={"+y": ("SC_Till", cell(R_KEYS, d % 10))}, skip=("-y",))
            dk = x1 - 0.057 - 6 * 0.048
            p.box((dk, ya, za + 0.008), (dk + 0.032, ya + 0.012, za + 0.038), "SC_Dark",
                  regions={"+y": ("SC_Till", cell(R_DEPT, r % 4))}, skip=("-y",))
    # the TOTAL bar along the right of the keys, stepping with the terraces
    tz0, tz1 = kz0 + 0.005, kz0 + rows * rd - 0.005
    tx = x0 + 0.012                  # on the clerk's right
    if level == 0:
        p.box((tx, yd + rows * rh, tz0), (tx + 0.035, yd + rows * rh + 0.015, tz1), "SC_Plastic",
              regions={"+y": ("SC_Till", R_TOTAL)}, skip=("-y",))
    # the head: full height behind the keyboard, a journal window on top
    hz0 = kz0 + rows * rd
    ht = yd + rows * rh + 0.06
    p.box((x0, yd, hz0), (x1, ht, z1), "SC_Plastic", skip=("-y",))
    p.quad("SC_Till", (x0 + 0.03, ht + 0.001, hz0 + 0.10), (x0 + 0.17, ht + 0.001, hz0 + 0.10), (x0 + 0.17, ht + 0.001, hz0 + 0.03),
           (x0 + 0.03, ht + 0.001, hz0 + 0.03), (0, 1, 0), R_WIN)
    # the AMOUNT tower on the head, readable from both sides
    tw0, tw1 = TX - 0.02, x1 - 0.01
    p.box((tw0, ht, z1 - 0.10), (tw1, ht + 0.11, z1 - 0.01), "SC_Plastic",
          regions={"+z": ("SC_Till", R_DISP), "-z": ("SC_Till", R_DISP)})
    p.box((tw0 - 0.005, ht + 0.11, z1 - 0.105), (tw1 + 0.005, ht + 0.125, z1 - 0.005), "SC_Trim")
    if level == 0:
        # the hand crank on the right side: a hub, an arm and a wooden grip
        hx = x1
        p.cyl((hx, yd + 0.08, TZ + 0.05), 0.035, 0.015, "x", "SC_Trim", segs=10)
        p.box((hx + 0.015, yd + 0.07, TZ + 0.045), (hx + 0.027, yd + 0.20, TZ + 0.06), "SC_Trim")
        p.cyl((hx + 0.027, yd + 0.19, TZ + 0.052), 0.012, 0.05, "x", "SC_WoodDark", segs=8)
        # a receipt slip curling out of the head, a paper roll seen at the side
        p.box((x0 + 0.06, ht, hz0 + 0.005), (x0 + 0.12, ht + 0.04, hz0 + 0.008), "SC_Till",
              regions={"-z": ("SC_Till", R_SLIP), "+z": ("SC_Till", R_SLIP)})
        p.cyl((x0 - 0.04, yd + 0.10, hz0 + 0.07), 0.035, 0.04, "x", "SC_Kraft", segs=10, cap_mat="SC_Till", cap_region=R_ROLL)


def top_items(p, level):
    # a service bell on the customer half, a receipt spike by the clerk, the change mat in front of the till
    bx, bz = -0.42, 0.30
    p.cyl((bx, TY, bz), 0.045, 0.012, "y", "SC_Dark", segs=12 if level == 0 else 8)
    p.cyl((bx, TY + 0.012, bz), 0.042, 0.035, "y", "SC_Trim", segs=12 if level == 0 else 8, r2=0.012)
    if level == 0:
        p.cyl((bx, TY + 0.047, bz), 0.006, 0.012, "y", "SC_Trim", segs=6)
        sx, sz = -0.12, -0.32
        p.cyl((sx, TY, sz), 0.035, 0.012, "y", "SC_Dark", segs=10)
        p.cyl((sx, TY + 0.012, sz), 0.0025, 0.15, "y", "SC_Trim", segs=5, r2=0.0008)
        for i, (hgt, dx) in enumerate(((0.02, 0.0), (0.035, 0.006), (0.048, -0.005))):
            p.box((sx - 0.035 + dx, TY + 0.012 + hgt, sz - 0.05), (sx + 0.035 + dx, TY + 0.0135 + hgt, sz + 0.05), "SC_Till",
                  regions={"+y": ("SC_Till", R_SLIP), "-y": ("SC_Till", R_SLIP)})
    p.box((TX - 0.125, TY, 0.12), (TX + 0.125, TY + 0.004, 0.30), "SC_Dark", regions={"+y": ("SC_Till", R_MAT)}, skip=("-y",))


def make(name, level, with_till):
    p = Part("%s_L%d" % (name, level))
    counter(p, level)
    if with_till:
        register(p, level)
        top_items(p, level)
    return p.finish()


COUNTER_BOX = ((-HW, 0.0, BZ), (HW, TY, FZ))
for name, with_till in (("ShopCounter", False), ("ShopCounterTill", True)):
    lods = [make(name, lv, with_till) for lv in range(3)]
    boxes = [COUNTER_BOX]
    if with_till:
        boxes.append(((TX - 0.21, TY, TZ - 0.27), (TX + 0.21, TY + 0.40, TZ + 0.21)))
    K.export_model(name, lods, boxes)

# ================================================================ review renders
import bpy
K.review_setup()
for name in ("ShopCounter", "ShopCounterTill"):
    K.show_only([o for o in bpy.data.objects if o.name == name + "_LOD0"])
    K.shot(name + "_front.png", (0, 0.6, 0), (0, 0.15, 1), 4.0)
    K.shot(name + "_three_quarter.png", (0, 0.6, 0), (0.8, 0.45, 1), 3.6)
    K.shot(name + "_clerk.png", (0, 0.7, 0), (-0.6, 0.7, -1), 3.4)
    if name == "ShopCounterTill":
        K.shot("till_close_clerk.png", (TX, TY + 0.15, TZ), (0.0, 0.8, -1), 1.1)
        K.shot("till_close_customer.png", (TX, TY + 0.2, TZ), (-0.3, 0.3, 1), 1.3)
        K.shot("front_close.png", (-0.3, 0.6, 0.6), (0.3, 0.1, 1), 1.6)
    for lv in (1, 2):
        K.show_only([o for o in bpy.data.objects if o.name == "%s_LOD%d" % (name, lv)])
        K.shot("%s_lod%d.png" % (name, lv), (0, 0.6, 0), (0.8, 0.45, 1), 3.6, res=(600, 540))
K.finish("shop-counter")
