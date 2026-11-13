# The pixel half of the material-class step (tools/assets/pipeline/materials.mjs decides the plan).
# numpy only, so a selftest can import it without bpy. Arrays are (px, px) floats 0..1, base is (px, px, 3).
import numpy as np


def luminance(base):
    return base[:, :, :3].mean(axis=2)


def remap(v, rng):
    lo, hi = rng
    return lo + (hi - lo) * np.clip(v, 0.0, 1.0)


def region_masks(plan, metal, height_mask):
    """One boolean mask per region, in order. height_mask(h) -> mask of faces above h metres."""
    out = []
    for g in plan["regions"]:
        if "metallic_above" in g:
            out.append(metal > g["metallic_above"])
        else:
            out.append(height_mask(g["above_height_m"]))
    return out


def apply(plan, base, rough, metal, masks):
    """Return (base, rough, metal) remapped by class. The input arrays are not changed."""
    cls = np.zeros(rough.shape, dtype=np.int32)  # 0 is the default class
    names = [plan["default"]]
    for g, m in zip(plan["regions"], masks):
        if g["class"] not in names:
            names.append(g["class"])
        cls[m] = names.index(g["class"])
    nb, nr, nm = base.copy(), rough.copy(), metal.copy()
    lum = luminance(base)
    for i, name in enumerate(names):
        c = plan["classes"][name]
        sel = cls == i
        nr[sel] = remap(rough, c["rough"])[sel]
        nm[sel] = remap(metal, c["metal"])[sel]
        if "base_lum" in c:
            grey = remap(lum, c["base_lum"])
            for ch in range(3):
                nb[:, :, ch][sel] = grey[sel]
        else:
            nb[sel] = np.clip(base[sel] * c.get("base_scale", 1.0), 0.0, 1.0)
    return nb, nr, nm
