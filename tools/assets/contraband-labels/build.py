# Contraband labels: the cigarette pack and the vodka bottle with US English print (#4383).
#
# Both items keep their vanilla Bohemia meshes. The vanilla colour maps print Cyrillic, so this script
# takes the vanilla BCR atlas (unpacked by build.mjs), repaints the pack's and the label's own patches
# with an invented 1989 brand in English, and writes the result as a .tif. Everything else in the atlas
# stays the vanilla pixels. No real brand, logo or person: GANNET cigarettes and CORVIN vodka are invented.
#
# Run: node tools/assets/contraband-labels/build.mjs   (Blender: build.py <repo> <vanilla dir> <out dir>)
import bpy, os, sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "paper-print"))
import paperlib as pl  # noqa: E402
import OpenImageIO as oiio  # noqa: E402

args = sys.argv[sys.argv.index("--") + 1:]
VANILLA, OUT = args[0], args[1]
S = 4  # supersampling: each atlas pixel is S x S canvas pixels, averaged down

NAVY = (22, 38, 84)
GOLD = (214, 174, 70)
CREAM = (244, 236, 214)
RED = (170, 30, 42)
KRAFT = (206, 178, 128)
INK = (28, 34, 62)


def read_dds(path):
    inp = oiio.ImageInput.open(path)
    spec = inp.spec()
    px = inp.read_image("uint8")
    inp.close()
    return np.asarray(px).reshape(spec.height, spec.width, spec.nchannels)


def paint(w, h, draw):
    """Run draw(c, w, h) on a fresh canvas (y is measured from the TOP of the patch, in atlas pixels) and
    return the patch as a top-down float (h, w, 3) array."""
    c = pl.Canvas(w, h, w * S, h * S)
    draw(c, w, h)
    arr = c.render({"print"}, os.path.join(OUT, "_patch.png"))[::-1]  # Blender rows run bottom-up
    return arr.reshape(h, S, w, S, 3).mean(axis=(1, 3))


def T(c, h, x, ytop, s, size, rgb, face="sans", align="CENTER", fit=None, heavy=False, spacing=1.0):
    """Text with its baseline ytop pixels from the top of the patch. heavy draws it twice, 0.4 px apart."""
    y = h - ytop
    c.text(x, y, s, size, rgb, face, align=align, fit=fit, spacing=spacing)
    if heavy:
        c.text(x + 0.45, y, s, size, rgb, face, align=align, fit=fit, spacing=spacing)


def R(c, h, x0, ytop0, x1, ytop1, rgb):
    c.rect(x0, h - ytop1, x1, h - ytop0, rgb)


def put(atlas, x0, y0, patch, rot=0):
    """Paste a patch (top-down float) into the uint8 atlas at atlas pixel (x0, y0), turned rot x 90 degrees CCW."""
    p = np.rot90(patch, rot)
    h, w = p.shape[:2]
    atlas[y0:y0 + h, x0:x0 + w, :3] = np.clip(p * 255.0 + 0.5, 0, 255).astype(np.uint8)
    # The alpha channel is roughness. The old print's detail is in it (a photo shows up as a ghost), so the
    # patch gets one flat value: the median of what the vanilla patch had.
    a = atlas[y0:y0 + h, x0:x0 + w, 3]
    atlas[y0:y0 + h, x0:x0 + w, 3] = int(np.median(a))


def barcode(c, h, x0, ytop0, x1, ytop1, seed):
    rng = np.random.default_rng(seed)
    x = x0
    while x < x1 - 1.5:
        wbar = float(rng.choice([0.7, 1.0, 1.4, 2.0]))
        if rng.random() < 0.6:
            R(c, h, x, ytop0, min(x + wbar, x1), ytop1, (10, 10, 10))
        x += wbar + float(rng.choice([0.7, 1.0, 1.4]))


# ----------------------------------------------------------------------------------------------- vodka
def vodka_label(c, w, h):
    R(c, h, 0, 0, w, h, CREAM)
    # front: navy label, double gold rule
    R(c, h, 2, 0, 246, h, NAVY)
    c.box(6, h - 211, 242, h - 5, GOLD, t=1.6)
    c.box(10, h - 207, 238, h - 9, GOLD, t=0.7)
    cx = 124
    T(c, h, cx, 26, "TRIPLE DISTILLED", 9, GOLD, fit=92, heavy=True)
    T(c, h, cx, 72, "CORVIN", 40, CREAM, fit=118, heavy=True)
    T(c, h, cx, 98, "VODKA", 22, GOLD, fit=70, heavy=True)
    c.hline(cx - 62, cx - 44, h - 91, GOLD, t=1.0)
    c.hline(cx + 44, cx + 62, h - 91, GOLD, t=1.0)
    c.ring(cx, h - 130, 21, 19.2, GOLD)
    c.star(cx, h - 130, 11.5, GOLD, inner=0.42)
    T(c, h, cx, 172, "PRODUCT OF EVERON", 9, CREAM, fit=104)
    T(c, h, cx, 187, "80 PROOF  -  750 ML", 11, CREAM, fit=104, heavy=True)
    T(c, h, cx, 200, "Distilled and bottled in Saint-Philippe", 6.2, GOLD, fit=150)
    # back: cream paper, dark print
    bx = 305
    T(c, h, bx, 28, "CORVIN VODKA", 11, INK, fit=96, heavy=True)
    T(c, h, bx, 40, "TRIPLE DISTILLED", 7, INK, fit=70)
    for i, line in enumerate(["Made from winter wheat and", "spring water, filtered through", "birch charcoal. Serve chilled.", "Please drink responsibly."]):
        T(c, h, bx, 58 + i * 9, line, 6.4, INK, face="mono", fit=98)
    T(c, h, bx, 100, "ALC. 40% BY VOL.", 8, INK, fit=84, heavy=True)
    T(c, h, bx, 112, "80 PROOF", 8, INK, fit=44, heavy=True)
    T(c, h, bx, 124, "750 ML (25.4 FL. OZ.)", 7, INK, fit=88)
    T(c, h, bx, 136, "Corvin Distillery, Saint-Philippe", 5.6, INK, fit=98)
    T(c, h, bx, 144, "Republic of Everon", 5.6, INK, fit=60)
    R(c, h, 256, 154, 354, 204, (250, 250, 246))
    barcode(c, h, 261, 160, 349, 192, 1989)
    T(c, h, bx, 200, "0 71254 01989 4", 6, (10, 10, 10), face="mono", fit=84)


# --------------------------------------------------------------------------------------------- carton
def carton_top(c, w, h):
    # upright: 96 wide, 84 tall; placed turned a quarter so its top runs to the atlas left
    R(c, h, 0, 0, w, h, RED)
    c.box(3, 3, w - 3, h - 3, CREAM, t=0.9)
    cx = w / 2
    T(c, h, cx, 14, "FILTER KINGS", 7.5, CREAM, fit=62, heavy=True)
    T(c, h, cx, 46, "GANNET", 27, CREAM, fit=84, heavy=True)
    c.hline(cx - 34, cx + 34, h - 53, GOLD, t=1.0)
    T(c, h, cx, 66, "20 CIGARETTES", 9, CREAM, fit=62, heavy=True)
    T(c, h, cx, 77, "Tobacco Products of Everon", 5.4, GOLD, fit=70)


def carton_side(label):
    def draw(c, w, h):
        R(c, h, 0, 0, w, h, NAVY)
        T(c, h, w / 2, h - 4.5, label, h - 8, CREAM, fit=w * 0.7, heavy=True)
    return draw


def carton_bottom(c, w, h):
    R(c, h, 0, 0, w, h, KRAFT)
    cx = w / 2
    T(c, h, cx, 18, "GANNET", 15, INK, fit=56, heavy=True)
    T(c, h, cx, 28, "FILTER KINGS", 7, INK, fit=56)
    T(c, h, cx, 40, "20 CIGARETTES", 6.5, INK, fit=56)
    for i, line in enumerate(["WARNING: SMOKING IS", "HAZARDOUS TO YOUR", "HEALTH."]):
        T(c, h, cx, 58 + i * 8, line, 5.2, INK, fit=58)
    T(c, h, cx, 86, "Tobacco Products of Everon", 5, INK, fit=62)
    barcode(c, h, 10, 94, w - 10, 104, 4306)


def build():
    os.makedirs(OUT, exist_ok=True)
    # --- cigarette pack: the litter atlas (shared with newspapers and papers, all of it kept but the pack)
    atlas = read_dds(os.path.join(VANILLA, "LitterInterior_01_V1_BCR.dds")).copy()
    put(atlas, 677, 204, paint(96, 84, carton_top), rot=3)
    put(atlas, 657, 205, paint(95, 19, carton_side("20 CIGARETTES")), rot=3)
    put(atlas, 762, 205, paint(95, 18, carton_side("GANNET")), rot=1)
    put(atlas, 677, 185, paint(84, 17, carton_side("FILTER KINGS")), rot=2)
    put(atlas, 677, 301, paint(84, 16, carton_side("FILTER KINGS")))
    put(atlas, 781, 206, paint(85, 91, carton_bottom))
    save_tif(atlas, os.path.join(OUT, "LF_CigaretteCarton_BCR.tif"))
    # --- vodka bottle: the paper label map
    atlas = read_dds(os.path.join(VANILLA, "S2_Vodka_paper_BCR.dds")).copy()
    put(atlas, 6, 248, paint(366, 216, vodka_label))
    save_tif(atlas, os.path.join(OUT, "LF_VodkaBottle_BCR.tif"))


def save_tif(arr, path):
    h, w = arr.shape[:2]
    img = bpy.data.images.new(os.path.basename(path), w, h, alpha=True, float_buffer=False)
    img.colorspace_settings.name = "Non-Color"
    rgba = (arr.astype(np.float32) / 255.0)[::-1]
    img.pixels.foreach_set(rgba.ravel())
    img.filepath_raw = path
    img.file_format = "TIFF"
    img.save()
    bpy.data.images.remove(img)
    print("wrote", path, w, h)


build()
