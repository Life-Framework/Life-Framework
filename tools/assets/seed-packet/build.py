# Builds the cabbage seed packet (tools/assets/seed-packet/kit.json) in headless Blender:
#   blender --background --factory-startup --python build.py -- <job.json>
# One model, SeedPacket: a thin paper envelope with a printed front and back (one atlas, US English) and a glued
# flap on the back. Helpers: tools/assets/pipeline/blender/kitlib.py.
import sys, json, math
import numpy as np

job = json.load(open(sys.argv[sys.argv.index("--") + 1]))
sys.path.insert(0, job["blender_lib"])
import kitlib as K
from kitlib import noise, blur, normal_from_height, save_material, Canvas, Part

kit, out = K.init(job)
L = kit["layout_m"]
HW, HH, HT = L["half_width"], L["half_height"], L["half_thick"]
AW, AH = kit["texture_px"]["atlas"]

# ================================================================ the print atlas (2 px = 1 mm)
CREAM = np.array([0.93, 0.90, 0.80])
GREEN = np.array([0.16, 0.42, 0.20])
INK = np.array([0.12, 0.14, 0.12])
FW, FH = int(HW * 4000), int(HH * 4000)  # 180 x 280
cv = Canvas(AW, AH, CREAM, rough=0.85)
cv.base *= (1 + noise(AH, AW, 1.5)[:, :, None] * 0.015 + noise(AH, AW, 30)[:, :, None] * 0.02)
R_FRONT = (0, 0, FW, FH)
R_BACK = (200, 0, FW, FH)
R_FLAP = (0, 300, FW, 70)
R_EDGE = (400, 300, 40, 40)

# front: a green band with the name, a painted savoy cabbage, SEEDS and the weight
x0 = 0
cv.rect(x0, 0, x0 + FW, 58, GREEN)
cv.rect(x0, 58, x0 + FW, 62, np.array([0.80, 0.64, 0.20]))
cv.text("CABBAGE", 0, 16, 3, CREAM, centre_in=(x0, x0 + FW))
cx, cy = x0 + FW / 2, 140
yy, xx = np.mgrid[0:AH, 0:AW]
d = np.hypot(xx - cx, (yy - cy) * 1.1)
ang = np.arctan2(yy - cy, xx - cx)
for r, col in ((64, (0.30, 0.52, 0.26)), (52, (0.42, 0.64, 0.32)), (40, (0.55, 0.74, 0.40)), (26, (0.70, 0.84, 0.52))):
    wob = r + 5 * np.sin(ang * 7 + r)
    m = d <= wob
    cv.base[m] = np.array(col) * (1 + noise(AH, AW, 3)[m, None] * 0.05)
veins = (np.abs(np.sin(ang * 9)) < 0.08) & (d < 60) & (d > 10)
cv.base[veins] = cv.base[veins] * 0.6 + np.array([0.85, 0.92, 0.75]) * 0.4
rim = (d > 60) & (d < 66 + 5 * np.sin(ang * 7 + 64))
cv.base[rim] = cv.base[rim] * 0.7
cv.text("SEEDS", 0, 222, 3, GREEN, centre_in=(x0, x0 + FW))
cv.text("NET WT 2 G", 0, 258, 1, INK, centre_in=(x0, x0 + FW))
cv.rect(x0, FH - 6, x0 + FW, FH, GREEN)

# back: the planting guide
x0 = R_BACK[0]
cv.rect(x0, 76, x0 + FW, 104, GREEN)   # below the 3.5 cm flap
cv.text("PLANTING GUIDE", 0, 83, 2, CREAM, centre_in=(x0, x0 + FW))
lines = ["SOW IN SPRING", "OR LATE SUMMER", "DEPTH 1/2 IN", "ROWS 2 FT APART", "WATER WELL", "HARVEST IN", "ABOUT 90 DAYS"]
for i, s in enumerate(lines):
    cv.text(s, x0 + 12, 114 + i * 17, 2 if len(s) <= 13 else 1, INK)
cv.rect(x0 + 12, FH - 40, x0 + FW - 12, FH - 38, INK * 2)
cv.text("SEASON 1989", x0 + 12, FH - 30, 1, INK)
cv.rect(x0, FH - 6, x0 + FW, FH, GREEN)

# the flap: plain cream with a darker glue line
cv.rect(0, 300, FW, 370, CREAM * 0.96)
cv.rect(0, 360, FW, 366, CREAM * 0.85)
cv.rect(R_EDGE[0], R_EDGE[1], R_EDGE[0] + 40, R_EDGE[1] + 40, CREAM * 0.97)
cv.height = noise(AH, AW, 2) * 0.1
cv.save("SP_Print", strength=0.6)


# ================================================================ geometry
def make(level):
    p = Part("seed_L%d" % level)
    edge = ("SP_Print", R_EDGE)
    p.box((-HW, -HH, -HT), (HW, HH, HT), "SP_Print",
          regions={"+z": ("SP_Print", R_FRONT), "-z": ("SP_Print", R_BACK), "+x": edge, "-x": edge, "+y": edge, "-y": edge})
    if level == 0:
        # the glued flap folded over the top of the back, 3.5 cm deep
        fz = -HT - 0.0006
        p.quad("SP_Print", (HW, HH - 0.035, fz), (-HW, HH - 0.035, fz), (-HW, HH, fz), (HW, HH, fz), (0, 0, -1), R_FLAP)
    return p.finish()


lods = [make(lv) for lv in range(2)]
K.export_model("SeedPacket", lods, [((-HW, -HH, -HT), (HW, HH, HT))])

K.review_setup(ground=False)
K.show_only([lods[0]])
K.shot("front.png", (0, 0, 0), (0, 0, 1), 0.5, ortho=0.18, res=(600, 700))
K.shot("back.png", (0, 0, 0), (0, 0, -1), 0.5, ortho=0.18, res=(600, 700))
K.shot("three_quarter.png", (0, 0, 0), (0.6, 0.3, 1), 0.4, res=(700, 700))
K.finish("seed-packet")
