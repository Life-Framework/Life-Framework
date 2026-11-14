# The fine ticket: a police penalty notice, printed in one green ink on cream stock, with a tear-off
# payment stub and a round office stamp. Modelled and painted in Blender (spec "source": "blender").
# Run by the generate stage:  blender --background --factory-startup --python model.py -- <job.json>
# Everything printed is fictional: the Republic of Everon, its Constabulary and form P-14 do not exist.
import json, os, sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "paper-print"))
import numpy as np
import paperlib as P

job = json.load(open(sys.argv[sys.argv.index("--") + 1]))
raw, px = job["raw"], job["px"]
W, H = 148.0, 210.0
c = P.Canvas(W, H, px, px)

GREEN = (38, 92, 70)
PALE = (96, 132, 116)
RED = (176, 44, 40)
WHITE = (255, 255, 255)
BLUE = (36, 62, 140)

# Border: two thin rules inside the margin.
c.box(5, 5, W - 5, H - 5, GREEN, 0.45)
c.box(6.6, 6.6, W - 6.6, H - 6.6, GREEN, 0.18)

# Header band with the crest.
c.rect(8.5, 183, W - 8.5, 202.5, GREEN)
cx, cy = 20, 192.8
c.poly([(cx - 5, cy + 7), (cx + 5, cy + 7), (cx + 5, cy - 1), (cx, cy - 7), (cx - 5, cy - 1)], WHITE)
c.poly([(cx - 4, cy + 6), (cx + 4, cy + 6), (cx + 4, cy - 0.7), (cx, cy - 5.8), (cx - 4, cy - 0.7)], GREEN)
c.star(cx, cy + 0.8, 2.6, WHITE)
c.text(29, 193.6, "EVERON CONSTABULARY", 6.2, WHITE, bold=0.012, spacing=1.05)
c.text(29, 187.4, "Traffic and Public Order Division", 3.3, WHITE)
c.text(W - 12, 187.4, "FORM P-14", 3.3, WHITE, face="mono", align="RIGHT")

# Title and serial.
c.text(W / 2, 170.5, "NOTICE OF PENALTY", 9.5, GREEN, align="CENTER", bold=0.01, spacing=1.08)
c.text(W / 2, 164.2, "Issued under the Road and Public Order Act  -  retain this notice", 3.0, PALE, align="CENTER")
c.text(12, 156.5, "No.", 3.6, GREEN, bold=0.01)
c.box(19, 153.6, 52, 159.8, RED, 0.3)
c.text(35.5, 155.2, "0 0 0 0 0 0", 4.6, RED, face="mono", align="CENTER")


def field(x0, x1, y, label, h=9.5):
    c.text(x0 + 1.2, y + h - 3.1, label, 2.35, PALE, spacing=1.12)
    c.box(x0, y, x1, y + h, GREEN, 0.22)


# Fields.
y = 138
field(12, 72, y, "DATE OF OFFENCE")
field(76, 136, y, "TIME")
y = 126
field(12, 136, y, "PLACE OF OFFENCE")
y = 114
field(12, 72, y, "VEHICLE REGISTRATION")
field(76, 136, y, "DRIVING PERMIT NO.")
y = 90
field(12, 136, y, "NATURE OF OFFENCE", h=21.5)
for k in range(1, 4):
    c.hline(14, 134, y + 21.5 - 3.1 - 5.2 * k - 0.6, PALE, 0.12)
y = 78
field(12, 72, y, "OFFICER NUMBER")
field(76, 136, y, "STATION")

# The penalty box.
c.rect(12, 56, 136, 72.5, GREEN)
c.text(16, 66.2, "PENALTY", 4.2, WHITE, bold=0.01, spacing=1.15)
c.text(16, 60.2, "Pay within fourteen days at any post office", 2.9, WHITE)
c.rect(88, 58, 134, 70.5, WHITE)
c.box(88, 58, 134, 70.5, GREEN, 0.3)

# Tear-off perforation and the stub.
c.dots(8, W - 8, 48, GREEN, 1.6, 0.2)
c.text(12, 49.6, "TEAR HERE", 2.2, PALE, spacing=1.3)
c.text(12, 40.5, "PAYMENT STUB", 5.0, GREEN, bold=0.01, spacing=1.08)
c.text(12, 35.6, "Return this stub with the amount shown above", 2.8, PALE)
c.text(W - 12, 40.5, "No.", 3.6, GREEN, align="RIGHT", bold=0.01)
field(12, 72, 16, "RECEIVED BY")
field(76, 136, 16, "DATE PAID")
c.text(12, 10.5, "P-14  /  1989  /  EVC PRINTING OFFICE", 2.2, PALE, face="mono", spacing=1.1)

# Stamp layer: a round office stamp, a little off true, and a red "ISSUED" box.
c.layer("stamp")
sx, sy, r = 108, 121, 15
c.tilt(sx, sy, 12)
c.ring(sx, sy, r, r - 0.8, BLUE)
c.ring(sx, sy, r - 1.6, r - 2.1, BLUE)
c.text(sx, sy + 3.6, "EVERON", 3.6, BLUE, align="CENTER", bold=0.02, rot=0)
c.text(sx, sy - 0.9, "CONSTABULARY", 2.6, BLUE, align="CENTER", bold=0.02, spacing=1.05)
c.hline(sx - 8.5, sx + 8.5, sy - 2.1, BLUE, 0.2)
c.text(sx, sy - 5.6, "TRAFFIC DIVISION", 1.9, BLUE, align="CENTER", bold=0.015)
c.star(sx - 9.2, sy + 0.4, 1.1, BLUE)
c.star(sx + 9.2, sy + 0.4, 1.1, BLUE)
print_img = c.render({"print"}, os.path.join(raw, "_print.png"))
stamp_img = P.worn(c.render({"stamp"}, os.path.join(raw, "_stamp.png")), 0.55, 11)
print_img = P.worn(print_img, 0.18, 12)
ink = print_img * stamp_img

surface = P.Surface((236, 231, 214), fibre=0.04, blotch=0.03, rough=0.88, seed=21)
crease = P.fold(px, px, "h", 0.5, 7, 0.035, 0.9)
maps = P.compose(surface, ink, crease=crease)
P.write_maps(raw, maps)
for f in ("_print.png", "_stamp.png"):
    os.remove(os.path.join(raw, f))

me = P.sheet(0.148, 0.21, 0.0008)
ob = P.export_glb(me, "FineTicketSheet", raw, os.path.join(raw, "tex_base_color.png"))
P.review(os.path.join(raw, "review.png"), ob, dist=0.42)
print("LF_ASSET_MODEL ok", len(me.polygons), "faces")
