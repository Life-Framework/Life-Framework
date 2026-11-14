# The receipt: a narrow till slip of thin grey-white paper, printed in dot-matrix capitals. Modelled and
# painted in Blender (spec "source": "blender"). Everything printed is fictional.
# Run by the generate stage:  blender --background --factory-startup --python model.py -- <job.json>
import json, os, sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "paper-print"))
import numpy as np
import paperlib as P

job = json.load(open(sys.argv[sys.argv.index("--") + 1]))
raw, px = job["raw"], job["px"]
W, H = 58.0, 180.0
c = P.Canvas(W, H, px, px)
INK = (38, 38, 46)
GREY = (96, 96, 104)
PURPLE = (88, 52, 128)
L, R = 4.5, W - 4.5
y = H - 9.0
S = 2.75


def line(s, size=S, align="LEFT", step=4.3, rgb=INK):
    global y
    x = L
    if align == "CENTER":
        x = W / 2
    if align == "RIGHT":
        x = R
    c.text(x, y, s, size, rgb, face="mono", align=align)
    y -= step


def leader(label, step=4.3, size=S):
    global y
    c.text(L, y, label, size, INK, face="mono")
    c.dots(L + c.width_of(label + " ", size, "mono"), R - 11, y + 0.3, GREY, 1.5, 0.17)
    y -= step


line("EVERON GENERAL STORES", 3.3, "CENTER", 5.0)
line("MARKET SQUARE No. 4", S, "CENTER", 3.8)
line("PORT GORSE", S, "CENTER", 5.4)
c.dots(L, R, y + 2, INK, 1.4, 0.16)
y -= 2.0
line("TILL 02        CLERK 07")
line("DATE   --/--/1989   --:--", step=5.2)
c.dots(L, R, y + 2.4, INK, 1.4, 0.16)
y -= 2.2
line("DESCRIPTION             SUM", S, step=5.0, rgb=GREY)
for k in range(8):
    leader("ITEM %d" % (k + 1))
c.dots(L, R, y + 2.6, INK, 1.4, 0.16)
y -= 3.0
leader("SUBTOTAL")
leader("TAX", step=5.6)
c.rect(L - 0.5, y - 1.7, R + 0.5, y + 4.0, INK)
c.text(L + 1, y, "TOTAL", 3.4, (255, 255, 255), face="mono")
y -= 8.6
leader("CASH")
leader("CHANGE", step=7.0)
c.dots(L, R, y + 3.0, INK, 1.4, 0.16)
y -= 2.4
line("THANK YOU", 3.0, "CENTER", 4.2)
line("PLEASE KEEP THIS SLIP", S, "CENTER", 3.8)
line("NO REFUND WITHOUT RECEIPT", 2.4, "CENTER", 6.0, GREY)
line("No 000000", S, "CENTER", 4.0)
c.layer("stamp")
c.tilt(40, 28, -9)
c.box(30, 21, 52, 29, PURPLE, 0.35)
c.text(41, 23.1, "PAID", 4.6, PURPLE, face="mono", align="CENTER")

print_img = P.worn(c.render({"print"}, os.path.join(raw, "_print.png")), 0.35, 31)
stamp_img = P.worn(c.render({"stamp"}, os.path.join(raw, "_stamp.png")), 0.5, 32)
ink = print_img * stamp_img
surface = P.Surface((238, 238, 232), fibre=0.025, blotch=0.02, rough=0.8, seed=41)
maps = P.compose(surface, ink, wear=0.25, crease=P.fold(px, px, "h", 0.66, 6, 0.025, 0.6))
P.write_maps(raw, maps)
for f in ("_print.png", "_stamp.png"):
    os.remove(os.path.join(raw, f))
me = P.sheet(0.058, 0.18, 0.0005, bow_x=0.0008, bow_y=0.006, nx=6, ny=18)
ob = P.export_glb(me, "ReceiptSheet", raw, os.path.join(raw, "tex_base_color.png"))
P.review(os.path.join(raw, "review.png"), ob, dist=0.42)
print("LF_ASSET_MODEL ok", len(me.polygons), "faces")
