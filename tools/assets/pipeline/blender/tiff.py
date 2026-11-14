# Baseline uncompressed 8-bit RGBA TIFF with unassociated alpha: what Workbench imports.
# clean.py and variants.py both write their _BCR and _NMO maps with it.
import struct


def write_tiff(path, rgba):
    h, w, _ = rgba.shape
    data = rgba.tobytes()
    tags = [(256, 4, 1, w), (257, 4, 1, h), (258, 3, 4, None), (259, 3, 1, 1), (262, 3, 1, 2), (273, 4, 1, None),
            (277, 3, 1, 4), (278, 4, 1, h), (279, 4, 1, len(data)), (284, 3, 1, 1), (338, 3, 1, 2)]
    ifd_at = 8
    bits_at = ifd_at + 2 + len(tags) * 12 + 4
    data_at = bits_at + 8
    with open(path, "wb") as f:
        f.write(struct.pack("<2sHI", b"II", 42, ifd_at))
        f.write(struct.pack("<H", len(tags)))
        for tag, typ, count, value in tags:
            if tag == 258:
                value = bits_at
            if tag == 273:
                value = data_at
            if typ == 3 and count == 1:
                f.write(struct.pack("<HHIHH", tag, typ, count, value, 0))
            else:
                f.write(struct.pack("<HHII", tag, typ, count, value))
        f.write(struct.pack("<I", 0))
        f.write(struct.pack("<4H", 8, 8, 8, 8))
        f.write(data)
