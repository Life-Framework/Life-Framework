# The material step for every spec variant (docs/asset-pipeline/README.md "Variants"): one set of
# maps per variant, remapped by that variant's material classes and packed the Enfusion way.
#   blender --background --factory-startup --python variants.py -- <job.json>
# job.json = { name, raw, out, px, variants: [{ name, plan }] }. Reads <raw>/<Variant>/tex_*.png and
# writes <out>/<Name>_<Variant>_BCR.tif and _NMO.tif (BCR: base colour + roughness in alpha; NMO:
# normal X, normal Y flipped to DirectX, metallic, AO in alpha) and <out>/variants.json.
# Model scripts import pack() for their review renders, so the pictures show the remapped maps.
import json, os, sys
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import materials
from tiff import write_tiff


def load(path, px, fallback):
    import bpy
    if not os.path.exists(path):
        return np.full((px, px, 4), fallback, np.float32)
    img = bpy.data.images.load(path, check_existing=False)
    if img.size[0] != px or img.size[1] != px:
        img.scale(px, px)
    arr = np.empty(px * px * 4, np.float32)
    img.pixels.foreach_get(arr)
    bpy.data.images.remove(img)
    return arr.reshape(px, px, 4)[::-1]  # Blender rows run bottom-up; TIFF runs top-down


def pack(folder, plan, px):
    """(bcr, nmo, stats): float arrays 0..1, rows top-down, from the four tex_*.png in folder."""
    base = load(os.path.join(folder, "tex_base_color.png"), px, (0.5, 0.5, 0.5, 1))
    rough = load(os.path.join(folder, "tex_roughness.png"), px, (0.6, 0.6, 0.6, 1))
    metal = load(os.path.join(folder, "tex_metallic.png"), px, (0, 0, 0, 1))
    normal = load(os.path.join(folder, "tex_normal.png"), px, (0.5, 0.5, 1, 1))
    masks = materials.region_masks(plan, metal[:, :, 0], lambda h: np.zeros((px, px), bool))
    b3, r1, m1 = materials.apply(plan, base[:, :, :3], rough[:, :, 0], metal[:, :, 0], masks)
    bcr = np.dstack([b3, r1])
    nmo = np.dstack([normal[:, :, 0], 1.0 - normal[:, :, 1], m1, np.ones((px, px), np.float32)])
    stats = {"mean_base": [float(v) for v in b3.mean(axis=(0, 1))], "mean_roughness": float(r1.mean()), "mean_metallic": float(m1.mean()),
             "regions": [{"class": g["class"], "share": float(m.mean())} for g, m in zip(plan["regions"], masks)]}
    return np.clip(bcr, 0, 1), np.clip(nmo, 0, 1), stats


def to_u8(a):
    return (a * 255 + 0.5).astype(np.uint8)


if __name__ == "__main__":
    job = json.load(open(sys.argv[sys.argv.index("--") + 1]))
    report = {}
    for v in job["variants"]:
        bcr, nmo, stats = pack(os.path.join(job["raw"], v["name"]), v["plan"], job["px"])
        stem = os.path.join(job["out"], "%s_%s" % (job["name"], v["name"]))
        write_tiff(stem + "_BCR.tif", to_u8(bcr))
        write_tiff(stem + "_NMO.tif", to_u8(nmo))
        report[v["name"]] = stats
    with open(os.path.join(job["out"], "variants.json"), "w") as f:
        json.dump(report, f, indent=1)
    print("LF_ASSET_VARIANTS ok", len(report))
