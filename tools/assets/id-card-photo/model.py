# The photo ID card (form R-89, #907): a laminated card, 85.6 x 54 mm, with a printed layout, a photo box
# and a stamp, matching the card sheet in game (LF_UISkin.IdCard: sage paper, navy rules). The texture is
# an atlas: the front in the top half, the back (mirrored by the mesh) in the bottom half.
# Modelled and painted in Blender (spec "source": "blender"). Everything printed is fictional.
# Run by the generate stage:  blender --background --factory-startup --python model.py -- <job.json>
import json, math, os, sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "paper-print"))
import numpy as np
import paperlib as P

job = json.load(open(sys.argv[sys.argv.index("--") + 1]))
raw, px = job["raw"], job["px"]
W, H = 85.6, 54.0
half = px // 2
NAVY = (40, 66, 118)
PALE = (96, 112, 140)
WHITE = (255, 255, 255)
RED = (176, 44, 40)
WAVE = (232, 240, 228)
SAGE = (228, 233, 218)


def waves(c, rgb):
    """Fine wavy security lines across the card, the way a registry prints a background."""
    for k in range(14):
        pts_a, pts_b = [], []
        for i in range(0, 171):
            x = W * i / 170
            y = 6 + k * 3.2 + 1.3 * math.sin(i / 170 * 9.0 + k * 0.6) + 0.5 * math.sin(i / 170 * 23 + k)
            pts_a.append((x, y + 0.11))
            pts_b.append((x, y - 0.11))
        c.poly(pts_a + pts_b[::-1], rgb)


# ---------------------------------------------------------------- front
c = P.Canvas(W, H, px, half)
waves(c, WAVE)
c.rect(0, 45, W, H, NAVY)
c.star(7.2, 49.5, 2.5, WHITE)
c.text(12.5, 50.2, "REPUBLIC OF EVERON", 3.3, WHITE, spacing=1.1)
c.text(12.5, 46.6, "IDENTITY CARD", 2.0, WHITE, spacing=1.35)
c.text(W - 4, 46.6, "FORM R-89", 1.9, WHITE, face="mono", align="RIGHT")
# Photo box.
c.rect(4, 8.6, 27.4, 40, (170, 184, 200))
for i in range(6):  # a studio backdrop, lighter at the top
    c.rect(4, 8.6 + i * 5.2, 27.4, 13.8 + i * 5.2, (186 - i * 4, 198 - i * 4, 212 - i * 3))
c.disc(15.7, 28.6, 5.4, (150, 120, 104))
c.poly([(5.6, 8.6), (7.6, 17.5), (12, 21.4), (19.4, 21.4), (23.8, 17.5), (25.8, 8.6)], (72, 84, 102))
c.poly([(13.4, 21.4), (18, 21.4), (15.7, 15.6)], (230, 232, 228))
c.box(4, 8.6, 27.4, 40, NAVY, 0.3)
# Printed lines: label above a rule.
fx = 31.5


def field(y, label, x0=fx, x1=W - 4):
    c.text(x0, y + 1.6, label, 1.7, PALE, spacing=1.18)
    c.hline(x0, x1, y, NAVY, 0.18)


field(37.2, "SURNAME")
field(29.6, "GIVEN NAMES")
field(22.0, "AGE", fx, fx + 13)
field(22.0, "DATE OF ISSUE", fx + 17, W - 4)
c.text(fx, 15.0, "CARD NO.", 1.7, PALE, spacing=1.18)
c.text(fx + 14, 14.6, "EV 0 0 0 0 0 0", 3.0, RED, face="mono")
c.hline(fx, W - 4, 13.0, NAVY, 0.18)
c.text(33, 3.4, "VALID WITH PHOTOGRAPH ONLY", 1.7, PALE, spacing=1.12)
c.text(W - 4, 3.4, "R-89 / 1989", 1.7, PALE, face="mono", align="RIGHT")
c.layer("stamp")
c.tilt(23, 11, 14)
c.ring(23, 11, 7.4, 7.0, NAVY)
c.ring(23, 11, 5.8, 5.5, NAVY)
c.text(23, 11.4, "EVERON", 1.7, NAVY, align="CENTER")
c.text(23, 8.8, "REGISTRY", 1.4, NAVY, align="CENTER", spacing=1.1)
c.star(23, 14.4, 0.8, NAVY)
f_print = P.worn(c.render({"print"}, os.path.join(raw, "_fp.png")), 0.06, 91)
f_stamp = P.worn(c.render({"stamp"}, os.path.join(raw, "_fs.png")), 0.45, 92)
front_ink = f_print * f_stamp

# ---------------------------------------------------------------- back
c = P.Canvas(W, H, px, half)
waves(c, WAVE)
c.rect(0, 41, W, H, NAVY)
c.text(W / 2, 45.6, "REPUBLIC OF EVERON  -  CIVIL REGISTRY", 2.8, WHITE, align="CENTER", spacing=1.1)
c.text(4, 35.0, "This card is the property of the Republic of Everon.", 2.2, NAVY)
c.text(4, 31.6, "If found, hand it to any police station or post office.", 2.2, NAVY)
c.text(4, 28.2, "A replacement is issued by the registry clerk on payment of the fee.", 2.2, NAVY)
c.rect(4, 14.5, W - 4, 23.5, (252, 252, 250))
c.box(4, 14.5, W - 4, 23.5, NAVY, 0.2)
c.text(5.4, 20.9, "HOLDER'S SIGNATURE", 1.7, PALE, spacing=1.15)
c.hline(8, W - 8, 16.8, PALE, 0.12)
c.text(4, 9.2, "<<EV000000<<<<<<<<<<<<<<<<<<<<<<<<<<<", 2.6, NAVY, face="mono")
c.text(4, 5.2, "R89<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<0", 2.6, NAVY, face="mono")
b_print = P.worn(c.render({"print"}, os.path.join(raw, "_bp.png")), 0.06, 93)

# ---------------------------------------------------------------- foil seal (metallic) on the front
c = P.Canvas(W, H, px, half)
c.disc(73.5, 10.5, 4.6, (0, 0, 0))
foil = 1.0 - c.render({"print"}, os.path.join(raw, "_foil.png"))[:, :, 0]
for f in ("_fp.png", "_fs.png", "_bp.png", "_foil.png"):
    os.remove(os.path.join(raw, f))

# ---------------------------------------------------------------- compose the atlas
lam = P.Surface(SAGE, fibre=0.012, blotch=0.012, rough=0.3, rough_var=0.08, grain=0.15, seed=101, ink_rough=0.1)
front = P.compose(lam, front_ink, wear=0.05)
back = P.compose(P.Surface(SAGE, fibre=0.012, blotch=0.012, rough=0.3, rough_var=0.08, grain=0.15, seed=111, ink_rough=0.1), b_print, wear=0.05)
for k in front:
    pass
fm = foil[:, :, None]
front["base_color"] = front["base_color"] * (1 - fm) + np.array([0.72, 0.78, 0.82], np.float32)[None, None, :] * fm
front["metallic"] = np.repeat(fm * 0.85, 3, axis=2)
front["roughness"] = front["roughness"] * (1 - fm) + 0.25 * fm
maps = {k: np.concatenate([back[k], front[k]], axis=0) for k in front}  # rows run bottom-up: the back is the lower half
P.write_maps(raw, maps)

me = P.card(0.0856, 0.054, 0.0008, 0.0032)
ob = P.export_glb(me, "IdCardPhoto", raw, os.path.join(raw, "tex_base_color.png"))
P.review(os.path.join(raw, "review.png"), ob, dist=0.2)
print("LF_ASSET_MODEL ok", len(me.polygons), "faces")
