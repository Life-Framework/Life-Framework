# Builds the mine-yard ore furnace (tools/assets/smelter-furnace/kit.json) in headless Blender:
#   blender --background --factory-startup --python build.py -- <job.json>
# One model, SmelterFurnace: brick hearth and body, iron angles and straps, a cast-iron fire door, an ore hopper,
# a round chimney. Helpers: tools/assets/pipeline/blender/kitlib.py.
import sys, json, math
import numpy as np

job = json.load(open(sys.argv[sys.argv.index("--") + 1]))
sys.path.insert(0, job["blender_lib"])
import kitlib as K
from kitlib import noise, blur, normal_from_height, save_material, Canvas, Part

kit, out = K.init(job)
L = kit["layout_m"]
H, HE, TOP, DO, HO, CH = L["half"], L["hearth"], L["body_top"], L["door"], L["hopper"], L["chimney"]
px = kit["texture_px"]["tile"]

# ================================================================ materials
# Red brick in stretcher bond, 1 m a repeat: bricks 22 x 6.5 cm with 1 cm mortar, soot blotches.
bw, bh, mo = 0.23, 0.075, 0.01
yy, xx = np.mgrid[0:px, 0:px] / px
row = np.floor(yy / bh)
xs = xx + (row % 2) * bw / 2
col = np.floor(xs / bw)
mort = ((yy % bh) < mo) | ((xs % bw) < mo)
seed = (row * 31 + col * 17) % 97 / 97.0
brick = np.array([0.60, 0.30, 0.20])
base = np.ones((px, px, 3)) * brick
base *= (0.82 + seed[:, :, None] * 0.3) * (1 + noise(px, px, 3)[:, :, None] * 0.05)
base[mort] = np.array([0.52, 0.50, 0.46]) * (1 + noise(px, px, 2)[mort, None] * 0.05)
soot = np.clip(noise(px, px, 50) * 0.6 + 0.2, 0, 1) ** 1.5
base *= (1 - soot[:, :, None] * 0.3)
hgt = np.where(mort, -1.0, 0.0) + noise(px, px, 2) * 0.15
save_material("SF_Brick", np.clip(base, 0, 1), np.where(mort, 0.95, 0.85), 0.0, normal_from_height(blur(hgt, 0.8), 2.0), 1.0)

# Cast and wrought iron: near black, a dull sheen, rust where water sits.
b = np.ones((px, px, 3)) * np.array([0.13, 0.12, 0.115]) * (1 + noise(px, px, 4)[:, :, None] * 0.08)
rust = np.clip(noise(px, px, 25) - 0.6, 0, 1) * 1.6
b = b * (1 - rust[:, :, None]) + rust[:, :, None] * np.array([0.36, 0.18, 0.09])
save_material("SF_Iron", np.clip(b, 0, 1), np.clip(0.6 + rust * 0.35, 0, 1), np.clip(0.75 - rust, 0, 1),
              normal_from_height(noise(px, px, 3) * 0.3 + rust * 0.4, 1.0), 0.5)

# Rusted sheet steel: the hopper and the chimney.
r = np.clip(noise(px, px, 18) * 0.5 + 0.55, 0, 1)
b = np.ones((px, px, 3)) * np.array([0.20, 0.17, 0.15])
b = b * (1 - r[:, :, None]) + r[:, :, None] * np.array([0.40, 0.21, 0.11]) * (1 + noise(px, px, 3)[:, :, None] * 0.1)
streak = np.clip(noise(px, px, 1.5, 60), 0, None) * 0.08
b *= (1 - streak[:, :, None])
save_material("SF_Rust", np.clip(b, 0, 1), np.clip(0.7 + r * 0.25, 0, 1), np.clip(0.6 - r * 0.5, 0, 1),
              normal_from_height(noise(px, px, 2) * 0.2 + r * 0.3, 1.0), 1.0)

# The door atlas (1 px = 1 mm): the fire door, the ash pit, the hopper's inside (coal and ore).
AW, AH = kit["texture_px"]["door"]
cv = Canvas(AW, AH, (0.11, 0.105, 0.10), rough=0.65)
cv.metal[:] = 0.6
cv.base *= (1 + noise(AH, AW, 3)[:, :, None] * 0.08)
DW, DH = int(DO["half_width"] * 2000), int((DO["y1"] - DO["y0"]) * 1000)  # 400 x 360
R_DOOR = (0, 0, DW, DH)
cv.rect(8, 8, DW - 8, DH - 8, height=0.6)                       # the raised panel
cv.rect(20, 20, DW - 20, DH - 20, height=0.3)
for x in range(20, DW - 10, 45):                                # rivets round the edge
    for y in (14, DH - 14):
        cv.circle(x, y, 4, height=1.0)
for y in range(40, DH - 20, 45):
    for x in (14, DW - 14):
        cv.circle(x, y, 4, height=1.0)
# the peep slot: three glowing slits behind a slide
for i in range(3):
    x0 = DW // 2 - 70 + i * 50
    cv.rect(x0, 70, x0 + 36, 92, (1.0, 0.45, 0.10), rough=0.9, metal=0.0, height=-1.0)
    cv.em[70:92, x0:x0 + 36] = np.array([1.0, 0.38, 0.06])
cv.rect(DW // 2 - 90, 60, DW // 2 + 90, 66, height=1.0)
cv.rect(DW // 2 - 90, 96, DW // 2 + 90, 102, height=1.0)
# the latch bar and handle on the right, a heat-tinted patch round the slot
cv.rect(DW - 110, DH // 2 - 10, DW - 30, DH // 2 + 10, (0.20, 0.19, 0.18), height=1.2)
cv.circle(DW - 40, DH // 2, 16, (0.22, 0.20, 0.18), height=1.4)
tint = np.exp(-(((np.arange(AW)[None, :] - DW / 2) / 140) ** 2 + ((np.arange(AH)[:, None] - 80) / 70) ** 2))
cv.base *= (1 + tint[:, :, None] * np.array([0.6, 0.25, 0.0]) * 0.5)
cv.rect(DW, 0, AW, AH, (0.11, 0.105, 0.10))
# ash pit: a dark opening with embers
R_ASH = (0, 380, 300, 60)
x0, y0, w, h = R_ASH
cv.rect(x0, y0, x0 + w, y0 + h, (0.03, 0.025, 0.02), rough=0.95, metal=0.0, height=-1.0)
emb = (noise(h, w, 2) > 1.6)
sub = cv.base[y0:y0 + h, x0:x0 + w]
sub[emb] = np.array([0.9, 0.35, 0.05])
cv.em[y0:y0 + h, x0:x0 + w][emb] = np.array([0.8, 0.25, 0.03])
# the hopper's fill: lumps of dark ore and coal
R_FILL = (320, 380, 120, 120)
x0, y0, w, h = R_FILL
lumps = noise(h, w, 4)
cv.paste(x0, y0, np.clip(np.ones((h, w, 3)) * np.array([0.10, 0.09, 0.085]) * (1 + lumps[:, :, None] * 0.35)
                         + (lumps > 1.2)[:, :, None] * np.array([0.25, 0.12, 0.06]), 0, 1),
         rough=np.full((h, w), 0.8), metal=np.zeros((h, w)), height=lumps * 0.6)
cv.save("SF_Door", strength=1.5, em=True)


# ================================================================ geometry
def frustum(p, cz, y0, y1, h0, h1, mat, inner_mat, fill=None):
    # An open square funnel: bottom half-size h0 at y0, top half-size h1 at y1, centred on (0, cz).
    c0 = [(-h0, -h0), (h0, -h0), (h0, h0), (-h0, h0)]
    c1 = [(-h1, -h1), (h1, -h1), (h1, h1), (-h1, h1)]
    for i in range(4):
        j = (i + 1) % 4
        pts = [(c0[i][0], y0, cz + c0[i][1]), (c0[j][0], y0, cz + c0[j][1]), (c1[j][0], y1, cz + c1[j][1]), (c1[i][0], y1, cz + c1[i][1])]
        mx = sum(q[0] for q in pts) / 4
        mz = sum(q[2] for q in pts) / 4 - cz
        p.face([K.B(*q) for q in pts], mat, want=K.B(mx, 0, mz))
        p.face([K.B(*q) for q in pts[::-1]], inner_mat, want=K.B(-mx, 0, -mz))
    if fill:
        f = h0 + (h1 - h0) * 0.6
        fy = y0 + (y1 - y0) * 0.6
        p.quad("SF_Door", (-f, fy, cz + f), (f, fy, cz + f), (f, fy, cz - f), (-f, fy, cz - f), (0, 1, 0), fill)


def make(level):
    p = Part("furnace_L%d" % level)
    hh, hy = HE["half"], HE["height"]
    # the hearth and the brick body
    p.box((-hh, 0, -hh), (hh, hy, hh), "SF_Brick", skip=("-y",))
    p.box((-H, hy, -H), (H, TOP, H), "SF_Brick", skip=("-y", "+y"))
    # the iron cap plate
    p.box((-H - 0.02, TOP, -H - 0.02), (H + 0.02, TOP + 0.04, H + 0.02), "SF_Iron")
    # the fire door, proud of the brick, and the ash pit below it
    dz = H + 0.025
    dw = DO["half_width"]
    if level <= 1:
        p.box((-dw, DO["y0"], H), (dw, DO["y1"], dz), "SF_Iron", skip=("-z",), regions={"+z": ("SF_Door", R_DOOR)})
        p.box((-dw - 0.04, DO["y0"] - 0.03, H), (dw + 0.04, DO["y1"] + 0.03, H + 0.008), "SF_Iron", skip=("-z", "+z"))
    else:
        p.quad("SF_Door", (-dw, DO["y0"], H + 0.002), (dw, DO["y0"], H + 0.002), (dw, DO["y1"], H + 0.002), (-dw, DO["y1"], H + 0.002), (0, 0, 1), R_DOOR)
    p.quad("SF_Door", (-0.15, hy + 0.03, H + 0.001), (0.15, hy + 0.03, H + 0.001), (0.15, hy + 0.09, H + 0.001), (-0.15, hy + 0.09, H + 0.001), (0, 0, 1), R_ASH)
    if level == 0:
        # hinge pins on the left, iron corner angles and two straps round the body
        for y in (DO["y0"] + 0.05, DO["y1"] - 0.08):
            p.cyl((-dw - 0.03, y, dz - 0.005), 0.012, 0.05, "y", "SF_Iron", segs=6)
        a = 0.05
        for sx in (-1, 1):
            for sz in (-1, 1):
                x0, x1 = sorted((sx * (H - a + 0.01), sx * (H + 0.008)))
                z0, z1 = sorted((sz * (H - a + 0.01), sz * (H + 0.008)))
                p.box((x0, hy, z0), (x1, TOP, z1), "SF_Iron", skip=("-y", "+y"))
        for y in (0.30, 0.95):
            p.box((-H - 0.012, y, -H - 0.012), (H + 0.012, y + 0.045, H + 0.012), "SF_Iron", skip=("-y", "+y"))
        # a tie rod head (round plate) on each side
        for sx in (-1, 1):
            p.cyl((sx * (H + 0.012), 0.62, 0), 0.04, sx * 0.015, "x", "SF_Iron", segs=8)
    # the ore hopper on the cap
    frustum(p, HO["z"], HO["y0"] - 0.04 + 0.04, HO["y1"], HO["bottom_half"], HO["top_half"], "SF_Rust", "SF_Rust",
            fill=R_FILL if level <= 1 else None)
    if level == 0:
        t = HO["top_half"]
        p.box((-t - 0.015, HO["y1"] - 0.02, HO["z"] - t - 0.015), (t + 0.015, HO["y1"], HO["z"] + t + 0.015), "SF_Iron", skip=("+y", "-y"))
    # the chimney: a brick collar, a round iron pipe, a rain cap
    cz = CH["z"]
    p.box((-0.17, TOP + 0.04, cz - 0.17), (0.17, TOP + 0.22, cz + 0.17), "SF_Brick", skip=("-y",))
    segs = 12 if level == 0 else (8 if level == 1 else 6)
    p.cyl((0, TOP + 0.22, cz), CH["r"], CH["top"] - TOP - 0.22, "y", "SF_Rust", segs=segs, caps=(False, False))
    p.cyl((0, CH["top"] - 0.02, cz), CH["r"] + 0.015, 0.06, "y", "SF_Iron", segs=segs, caps=(False, True))
    if level <= 1:
        p.cyl((0, CH["top"] + 0.12, cz), 0.22, 0.10, "y", "SF_Iron", segs=segs, r2=0.03, caps=(True, False))
        for i in range(3):
            a = 2 * math.pi * i / 3
            x, z = math.cos(a) * 0.12, cz + math.sin(a) * 0.12
            p.box((x - 0.006, CH["top"] + 0.03, z - 0.006), (x + 0.006, CH["top"] + 0.13, z + 0.006), "SF_Iron")
    return p.finish()


lods = [make(lv) for lv in range(3)]
boxes = [((-HE["half"], 0, -HE["half"]), (HE["half"], TOP + 0.04, HE["half"])),
         ((-HO["top_half"], TOP + 0.04, HO["z"] - HO["top_half"]), (HO["top_half"], HO["y1"], HO["z"] + HO["top_half"])),
         ((-0.17, TOP + 0.04, CH["z"] - 0.17), (0.17, CH["top"] + 0.2, CH["z"] + 0.17))]
K.export_model("SmelterFurnace", lods, boxes)

# ================================================================ review renders
import bpy
K.review_setup()
K.show_only([lods[0]])
K.shot("front.png", (0, 1.2, 0), (0, 0.1, 1), 5.0)
K.shot("three_quarter.png", (0, 1.2, 0), (0.8, 0.35, 1), 5.0)
K.shot("back.png", (0, 1.2, 0), (-0.7, 0.4, -1), 5.0)
K.shot("door_close.png", (0, 0.55, 0.5), (0.2, 0.1, 1), 1.4)
K.shot("hopper_top.png", (0, 1.5, 0), (0.3, 1.0, 0.6), 2.0)
for lv in (1, 2):
    K.show_only([lods[lv]])
    K.shot("lod%d.png" % lv, (0, 1.2, 0), (0.8, 0.35, 1), 5.0, res=(600, 540))
K.finish("smelter-furnace")
