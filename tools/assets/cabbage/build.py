# Builds the harvested cabbage head (tools/assets/cabbage/kit.json) in headless Blender:
#   blender --background --factory-startup --python build.py -- <job.json>
# One model, Cabbage: a squashed head of tight leaves, five loose outer leaves cupped round it, a cut stalk.
# Helpers: tools/assets/pipeline/blender/kitlib.py.
import sys, json, math
import numpy as np

job = json.load(open(sys.argv[sys.argv.index("--") + 1]))
sys.path.insert(0, job["blender_lib"])
import kitlib as K
from kitlib import noise, blur, normal_from_height, save_material, Part

kit, out = K.init(job)
L = kit["layout_m"]
R, SQ, LR = L["head_r"], L["head_squash"], L["leaf_r"]
px = kit["texture_px"]["tile"]

# ================================================================ materials
def leaf_tex(rgb, vein_rgb, vein_px):
    yy, xx = np.mgrid[0:px, 0:px]
    veins = (np.abs(((xx + noise(px, px, 20) * 6) % vein_px) - vein_px / 2) < 1.2).astype(float)
    veins = np.clip(blur(veins, 0.8) * 1.5, 0, 1)
    b = np.ones((px, px, 3)) * np.array(rgb) * (1 + noise(px, px, 10)[:, :, None] * 0.06 + noise(px, px, 2)[:, :, None] * 0.02)
    b = b * (1 - veins[:, :, None] * 0.35) + veins[:, :, None] * np.array(vein_rgb) * 0.35
    bumps = noise(px, px, 3) * 0.4 + blur(noise(px, px, 1), 1) * 0.2
    return np.clip(b, 0, 1), 0.55 - veins * 0.1, normal_from_height(bumps - veins * 0.5, 1.2)


b, r, n = leaf_tex((0.50, 0.60, 0.38), (0.78, 0.84, 0.66), 40)
save_material("CB_Head", b, r, 0.0, n, 0.12)
b, r, n = leaf_tex((0.26, 0.40, 0.22), (0.62, 0.72, 0.52), 28)
save_material("CB_Leaf", b, r, 0.0, n, 0.15)


# ================================================================ geometry
def ellipsoid(p, mat, rx, ry, rz, cy, segs, rings, lo=0.0, hi=1.0, wob=0.0, flip=False):
    # Rings from the bottom (lo=0) to the top (hi=1) of an ellipsoid centred at (0, cy, 0); open ends are capped by
    # nothing. wob adds a lumpy leaf edge; flip adds the inside faces too (for the open outer leaves).
    def pt(i, j):
        t = lo + (hi - lo) * j / rings
        phi = math.pi * (t - 0.5)
        a = 2 * math.pi * i / segs
        w = 1 + wob * math.sin(a * 5 + j * 1.7) * math.cos(phi)
        return (math.cos(a) * math.cos(phi) * rx * w, cy + math.sin(phi) * ry, math.sin(a) * math.cos(phi) * rz * w)
    centre = K.B(0, cy, 0)
    for j in range(rings):
        for i in range(segs):
            q = [pt(i, j), pt(i + 1, j), pt(i + 1, j + 1), pt(i, j + 1)]
            if j == 0 and lo == 0.0:
                q = [pt(i, 0), pt(i + 1, j + 1), pt(i, j + 1)]
            elif j == rings - 1 and hi == 1.0:
                q = [pt(i, j), pt(i + 1, j), pt(i, j + 1)]
            vs = [K.B(*c) for c in q]
            mid = sum(vs, K.Vector((0, 0, 0))) / len(vs)
            p.face(vs, mat, want=mid - centre)
            if flip:
                p.face(vs[::-1], mat, want=centre - mid)


def make(level):
    p = Part("cabbage_L%d" % level)
    segs, rings = (16, 10) if level == 0 else ((10, 6) if level == 1 else (6, 4))
    cy = R * SQ + 0.012
    ellipsoid(p, "CB_Head", R, R * SQ, R, cy, segs, rings, wob=0.04)
    if level <= 1:
        # five outer leaves: open shells cupped round the lower two thirds, each turned and tilted a little
        for k in range(5 if level == 0 else 3):
            a = 2 * math.pi * k / 5
            for j in range(4):
                for i in range(4):
                    def lp(ii, jj):
                        u = (ii / 4 - 0.5) * 1.0          # across the leaf, radians round the head
                        v = jj / 4                          # from the stalk up the side
                        phi = -1.0 + v * 1.2
                        rr = LR * (1 + 0.45 * v * v) * (1 - 0.15 * abs(u))
                        ang = a + u
                        return (math.cos(ang) * math.cos(phi) * rr, cy * 0.85 + math.sin(phi) * rr * 0.95, math.sin(ang) * math.cos(phi) * rr)
                    q = [lp(i, j), lp(i + 1, j), lp(i + 1, j + 1), lp(i, j + 1)]
                    vs = [K.B(*c) for c in q]
                    mid = sum(vs, K.Vector((0, 0, 0))) / 4
                    p.face(vs, "CB_Leaf", want=mid - K.B(0, cy, 0))
                    p.face(vs[::-1], "CB_Leaf", want=K.B(0, cy, 0) - mid)
    # the cut stalk
    p.cyl((0, 0.0, 0), 0.016, 0.03, "y", "CB_Leaf", segs=8 if level == 0 else 5, caps=(True, False))
    return p.finish()


lods = [make(lv) for lv in range(3)]
h = 2 * R * SQ + 0.012
K.export_model("Cabbage", lods, [((-R * 0.9, 0.0, -R * 0.9), (R * 0.9, h, R * 0.9))])

K.review_setup()
K.show_only([lods[0]])
K.shot("three_quarter.png", (0, 0.08, 0), (0.7, 0.6, 1), 0.7, res=(700, 700))
K.shot("side.png", (0, 0.08, 0), (1, 0.1, 0), 0.7, res=(700, 700))
K.show_only([lods[1]])
K.shot("lod1.png", (0, 0.08, 0), (0.7, 0.6, 1), 0.7, res=(500, 500))
K.finish("cabbage")
