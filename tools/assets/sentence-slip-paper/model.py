# The sentence slip: a court form in black ink on buff stock, with a seal and a red SERVED stamp.
# Modelled and painted in Blender (spec "source": "blender"). Everything printed is fictional.
# Run by the generate stage:  blender --background --factory-startup --python model.py -- <job.json>
import json, math, os, sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "paper-print"))
import numpy as np
import paperlib as P

job = json.load(open(sys.argv[sys.argv.index("--") + 1]))
raw, px = job["raw"], job["px"]
W, H = 148.0, 210.0
c = P.Canvas(W, H, px, px)
INK = (30, 28, 34)
PALE = (96, 92, 98)
RED = (184, 38, 36)
SEAL = (60, 44, 96)

# Double border with square corner blocks.
c.box(5, 5, W - 5, H - 5, INK, 0.7)
c.box(7, 7, W - 7, H - 7, INK, 0.2)
for (x, y) in ((5, 5), (W - 9, 5), (5, H - 9), (W - 9, H - 9)):
    c.rect(x, y, x + 4, y + 4, INK)

# Heading.
c.text(W / 2, 189, "DISTRICT COURT OF EVERON", 7.2, INK, align="CENTER", spacing=1.1)
c.text(W / 2, 182.4, "CRIMINAL DIVISION", 3.8, INK, align="CENTER", spacing=1.5)
c.hline(16, W - 16, 178.4, INK, 0.5)
c.hline(16, W - 16, 177.2, INK, 0.15)
c.text(W / 2, 166.5, "SENTENCE SLIP", 11, INK, align="CENTER", spacing=1.12)
c.text(W / 2, 160.8, "Form C-27  -  to be handed to the clerk of the prison", 3.0, PALE, align="CENTER")
c.text(W - 14, 150.4, "No.", 3.4, INK, align="RIGHT")


def row(y, label, x0=14, x1=W - 14, h=9.0):
    c.text(x0, y + h - 2.8, label, 2.4, PALE, spacing=1.12)
    c.hline(x0, x1, y, INK, 0.25)


row(139, "CASE NUMBER", 14, 70)
row(139, "DATE OF HEARING", 78, W - 14)
row(127, "DEFENDANT")
row(115, "CHARGE")
c.hline(14, W - 14, 108.4, INK, 0.12)

# What was ordered: tick boxes and amounts.
c.text(14, 100.4, "THE COURT ORDERS", 3.0, INK, spacing=1.2)
for k, lab in enumerate(("CUSTODY", "FINE", "COMMUNITY SERVICE")):
    y = 92.4 - 8.0 * k
    c.box(15, y - 0.8, 20.4, y + 4.6, INK, 0.35)
    c.text(24, y + 0.2, lab, 3.4, INK, spacing=1.1)
    c.text(W - 14, y + 0.2, ("MONTHS", "UNITS", "HOURS")[k], 2.6, PALE, align="RIGHT", spacing=1.2)
    c.hline(96, W - 36, y, INK, 0.2)
c.hline(14, W - 14, 64.5, INK, 0.12)

# Signatures.
c.hline(14, 66, 50, INK, 0.25)
c.text(14, 46.4, "Presiding judge", 2.5, PALE)
c.hline(14, 66, 30, INK, 0.25)
c.text(14, 26.4, "Clerk of the court", 2.5, PALE)
c.text(W - 14, 12, "C-27 / 1989 / EVC PRINTING OFFICE", 2.2, PALE, face="mono", align="RIGHT")

# Court seal and the SERVED stamp, in the stamp layer (pressed ink).
c.layer("stamp")
sx, sy = 105, 38
c.ring(sx, sy, 14, 13.1, SEAL)
c.ring(sx, sy, 11.2, 10.8, SEAL)
c.hline(sx - 8, sx + 8, sy + 4.5, SEAL, 0.5)
c.vline(sx, sy - 5.5, sy + 4.5, SEAL, 0.5)
for dx in (-6.5, 6.5):
    c.poly([(sx + dx - 2.8, sy - 0.2), (sx + dx + 2.8, sy - 0.2), (sx + dx, sy - 2.4)], SEAL)
c.rect(sx - 2.6, sy - 6.6, sx + 2.6, sy - 5.4, SEAL)
c.text(sx, sy - 9.8, "EVERON", 2.3, SEAL, align="CENTER", spacing=1.4)
c.text(sx, sy + 7.6, "DISTRICT COURT", 2.0, SEAL, align="CENTER", spacing=1.2)
c.tilt(86, 120, -14)
c.box(60, 112, 114, 128, RED, 0.8)
c.box(61.8, 113.8, 112.2, 126.2, RED, 0.25)
c.text(87, 116.8, "SERVED", 9.0, RED, align="CENTER", spacing=1.1)

print_img = P.worn(c.render({"print"}, os.path.join(raw, "_print.png")), 0.2, 71)
stamp_img = P.worn(c.render({"stamp"}, os.path.join(raw, "_stamp.png")), 0.5, 72)
ink = print_img * stamp_img
surface = P.Surface((232, 220, 186), fibre=0.05, blotch=0.04, rough=0.9, seed=81)
maps = P.compose(surface, ink, crease=P.fold(px, px, "h", 0.5, 7, 0.04, 0.9))
P.write_maps(raw, maps)
for f in ("_print.png", "_stamp.png"):
    os.remove(os.path.join(raw, f))
me = P.sheet(0.148, 0.21, 0.0008)
ob = P.export_glb(me, "SentenceSlipSheet", raw, os.path.join(raw, "tex_base_color.png"))
P.review(os.path.join(raw, "review.png"), ob, dist=0.42)
print("LF_ASSET_MODEL ok", len(me.polygons), "faces")
