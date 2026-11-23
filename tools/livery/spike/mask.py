# tools/livery/spike/mask.py - build a livery mask from a vanilla one (livery spike #4226).
# Runs in Blender's Python (it ships OpenImageIO and numpy):
#   blender -b --factory-startup --python tools/livery/spike/mask.py -- <base.dds> <out.tif> <spec.json>
# The vanilla mask's G, B and A channels (paint layers 3 and 4, and the alpha) are kept; R (layer 2,
# the second paint colour) is cleared and redrawn from the spec:
#   {"probe": 64}                         a labelled grid of N px cells (finds where a panel sits in UV 2)
#   {"text": [{"x":..,"y":..,"h":..,"text":"POLICE","rot":0,"flipx":false}], "keep_r": false}
# x, y are the top-left corner in pixels of the 512 px mask, h the letter height in pixels.
import sys, json
import numpy as np
import OpenImageIO as oiio

FONT = {  # 5x7, rows top to bottom
    "A": ["01110", "10001", "10001", "11111", "10001", "10001", "10001"],
    "B": ["11110", "10001", "10001", "11110", "10001", "10001", "11110"],
    "C": ["01110", "10001", "10000", "10000", "10000", "10001", "01110"],
    "D": ["11110", "10001", "10001", "10001", "10001", "10001", "11110"],
    "E": ["11111", "10000", "10000", "11110", "10000", "10000", "11111"],
    "F": ["11111", "10000", "10000", "11110", "10000", "10000", "10000"],
    "G": ["01110", "10001", "10000", "10111", "10001", "10001", "01111"],
    "H": ["10001", "10001", "10001", "11111", "10001", "10001", "10001"],
    "I": ["01110", "00100", "00100", "00100", "00100", "00100", "01110"],
    "L": ["10000", "10000", "10000", "10000", "10000", "10000", "11111"],
    "O": ["01110", "10001", "10001", "10001", "10001", "10001", "01110"],
    "P": ["11110", "10001", "10001", "11110", "10000", "10000", "10000"],
    "0": ["01110", "10011", "10101", "10101", "10101", "11001", "01110"],
    "1": ["00100", "01100", "00100", "00100", "00100", "00100", "01110"],
    "2": ["01110", "10001", "00001", "00010", "00100", "01000", "11111"],
    "3": ["11110", "00001", "00001", "01110", "00001", "00001", "11110"],
    "4": ["00010", "00110", "01010", "10010", "11111", "00010", "00010"],
    "5": ["11111", "10000", "11110", "00001", "00001", "10001", "01110"],
    "6": ["00110", "01000", "10000", "11110", "10001", "10001", "01110"],
    "7": ["11111", "00001", "00010", "00100", "01000", "01000", "01000"],
    "8": ["01110", "10001", "10001", "01110", "10001", "10001", "01110"],
    " ": ["00000"] * 7,
}


def text_bitmap(text, h):
    s = max(1, round(h / 7))
    cols = []
    for ch in text:
        g = np.array([[int(c) for c in row] for row in FONT[ch]], dtype=np.uint8)
        cols.append(g)
        cols.append(np.zeros((7, 1), dtype=np.uint8))
    bm = np.concatenate(cols[:-1], axis=1)
    return np.kron(bm, np.ones((s, s), dtype=np.uint8))


def stamp(r, bm, x, y):
    hh, ww = bm.shape
    H, W = r.shape
    y0, x0 = max(0, y), max(0, x)
    y1, x1 = min(H, y + hh), min(W, x + ww)
    if y1 <= y0 or x1 <= x0:
        return
    sub = bm[y0 - y:y1 - y, x0 - x:x1 - x]
    r[y0:y1, x0:x1] = np.maximum(r[y0:y1, x0:x1], sub * 255)


def main():
    args = sys.argv[sys.argv.index("--") + 1:]
    base, out, spec_path = args
    spec = json.load(open(spec_path))
    buf = oiio.ImageBuf(base)
    if buf.has_error:
        raise SystemExit("cannot read " + base + ": " + buf.geterror())
    px = (buf.get_pixels(oiio.FLOAT) * 255 + 0.5).astype(np.uint8)  # H x W x 4
    H, W = px.shape[:2]
    r = px[:, :, 0].copy() if spec.get("keep_r") else np.zeros((H, W), dtype=np.uint8)
    if spec.get("clear_gb"):
        px[:, :, 1] = 0
        px[:, :, 2] = 0
    n = spec.get("probe")
    if n:
        rows = "ABCDEFGH"
        for j in range(H // n):
            r[j * n, :] = 255
            for i in range(W // n):
                r[:, i * n] = 255
                stamp(r, text_bitmap(rows[j % 8] + str(i + 1), n // 3), i * n + 6, j * n + 6)
    for t in spec.get("text", []):
        bm = text_bitmap(t["text"], t["h"])
        for _ in range(t.get("rot", 0) // 90):
            bm = np.rot90(bm)
        if t.get("flipx"):
            bm = bm[:, ::-1]
        if t.get("flipy"):
            bm = bm[::-1, :]
        stamp(r, bm, t["x"], t["y"])
    px[:, :, 0] = r
    spec_out = oiio.ImageSpec(W, H, 4, oiio.UINT8)
    o = oiio.ImageOutput.create(out)
    o.open(out, spec_out)
    o.write_image(px)
    o.close()
    prev = out.rsplit(".", 1)[0] + "_preview.png"
    o = oiio.ImageOutput.create(prev)
    o.open(prev, oiio.ImageSpec(W, H, 3, oiio.UINT8))
    o.write_image(np.ascontiguousarray(px[:, :, :3]))
    o.close()
    print("LF_MASK wrote", out, prev)


main()
