# The hospital bill: a letterhead invoice in blue ink on white stock, folded in three. Modelled and
# painted in Blender (spec "source": "blender"). Everything printed is fictional.
# Run by the generate stage:  blender --background --factory-startup --python model.py -- <job.json>
import json, os, sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "paper-print"))
import numpy as np
import paperlib as P

job = json.load(open(sys.argv[sys.argv.index("--") + 1]))
raw, px = job["raw"], job["px"]
W, H = 148.0, 210.0
c = P.Canvas(W, H, px, px)
BLUE = (34, 70, 128)
PALE = (96, 120, 158)
TINT = (214, 224, 240)
WHITE = (255, 255, 255)
RED = (176, 44, 40)

# Letterhead: seal, name, address.
c.ring(24, 190, 9.5, 8.6, BLUE)
c.ring(24, 190, 7.4, 7.0, BLUE)
c.text(24, 185.6, "H", 12, BLUE, align="CENTER")
c.text(38, 193.8, "EVERON REGIONAL", 7.0, BLUE, spacing=1.04)
c.text(38, 186.6, "HOSPITAL", 7.0, BLUE, spacing=1.2)
c.text(38, 181.2, "Billing and Accounts Office", 3.3, PALE)
c.text(W - 10, 193.4, "14 Infirmary Road", 2.9, PALE, align="RIGHT")
c.text(W - 10, 189.6, "Port Gorse, Everon", 2.9, PALE, align="RIGHT")
c.text(W - 10, 185.8, "Tel. 41 22 07", 2.9, PALE, align="RIGHT")
c.rect(10, 177, W - 10, 178.2, BLUE)
c.rect(10, 175.6, W - 10, 175.9, BLUE)

# Invoice heading and the particulars.
c.text(10, 163.5, "INVOICE", 11, BLUE, spacing=1.18)
c.text(W - 10, 165.6, "Invoice no.", 2.6, PALE, align="RIGHT")
c.text(W - 10, 160.4, "0 0 0 0 0 0", 5.2, RED, face="mono", align="RIGHT")
for (x0, x1, yy, lab) in ((10, 78, 144, "PATIENT"), (82, W - 10, 144, "DATE OF TREATMENT"), (10, 78, 132, "WARD / DEPARTMENT"), (82, W - 10, 132, "ATTENDING PHYSICIAN")):
    c.text(x0 + 1.2, yy + 7.0, lab, 2.3, PALE, spacing=1.12)
    c.box(x0, yy, x1, yy + 9.5, BLUE, 0.2)

# The table.
top = 124
c.rect(10, top - 7, W - 10, top, BLUE)
c.text(13, top - 4.8, "DESCRIPTION OF SERVICE", 2.8, WHITE, spacing=1.1)
c.text(104, top - 4.8, "QTY", 2.8, WHITE, spacing=1.1, align="CENTER")
c.text(W - 13, top - 4.8, "AMOUNT", 2.8, WHITE, spacing=1.1, align="RIGHT")
rows = 8
for k in range(rows):
    y1 = top - 7 - 7.2 * k
    if k % 2 == 0:
        c.rect(10, y1 - 7.2, W - 10, y1, TINT)
    c.hline(10, W - 10, y1 - 7.2, PALE, 0.15)
bottom = top - 7 - 7.2 * rows
c.vline(95, bottom, top - 7, PALE, 0.15)
c.vline(113, bottom, top - 7, PALE, 0.15)
c.box(10, bottom, W - 10, top, BLUE, 0.25)

# Total.
c.rect(86, bottom - 12.5, W - 10, bottom - 2.5, BLUE)
c.text(90, bottom - 8.6, "TOTAL DUE", 3.6, WHITE, spacing=1.12)
c.rect(112, bottom - 11.2, W - 11.2, bottom - 3.8, WHITE)
c.text(10, bottom - 6.0, "Payable at the cashier's office,", 2.8, PALE)
c.text(10, bottom - 9.6, "ground floor, within thirty days.", 2.8, PALE)

# Footer: signature and terms.
c.hline(10, 62, 32, BLUE, 0.2)
c.text(10, 28.6, "Accounts clerk", 2.5, PALE)
c.hline(76, W - 10, 32, BLUE, 0.2)
c.text(76, 28.6, "Date", 2.5, PALE)
c.text(W / 2, 17.6, "Cheques are not accepted. Please quote the invoice number when paying.", 2.5, PALE, align="CENTER")
c.text(W / 2, 13.6, "EVERON REGIONAL HOSPITAL  -  FORM B-3  -  1989", 2.3, PALE, align="CENTER", spacing=1.1)

c.layer("stamp")
c.tilt(115, 64, 8)
c.box(98, 56, 132, 72, BLUE, 0.5)
c.box(99.4, 57.4, 130.6, 70.6, BLUE, 0.2)
c.text(115, 64.2, "ACCOUNTS", 4.2, BLUE, align="CENTER")
c.text(115, 59.0, "OFFICE", 3.0, BLUE, align="CENTER", spacing=1.3)

print_img = P.worn(c.render({"print"}, os.path.join(raw, "_print.png")), 0.16, 51)
stamp_img = P.worn(c.render({"stamp"}, os.path.join(raw, "_stamp.png")), 0.5, 52)
ink = print_img * stamp_img
surface = P.Surface((246, 246, 244), fibre=0.03, blotch=0.02, rough=0.82, seed=61)
f1 = P.fold(px, px, "h", 1 / 3, 7, 0.03, 0.8)
f2 = P.fold(px, px, "h", 2 / 3, 7, 0.03, 0.8)
maps = P.compose(surface, ink, crease=(f1[0] + f2[0], f1[1] + f2[1]))
P.write_maps(raw, maps)
for f in ("_print.png", "_stamp.png"):
    os.remove(os.path.join(raw, f))
me = P.sheet(0.148, 0.21, 0.0008)
ob = P.export_glb(me, "HospitalBillSheet", raw, os.path.join(raw, "tex_base_color.png"))
P.review(os.path.join(raw, "review.png"), ob, dist=0.42)
print("LF_ASSET_MODEL ok", len(me.polygons), "faces")
