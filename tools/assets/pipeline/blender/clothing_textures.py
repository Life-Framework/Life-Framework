# The texture step of a route (a) garment (docs/asset-pipeline/clothing.md "Route a").
#   blender --background --factory-startup --python clothing_textures.py -- <job.json>
# job.json = { items: [{ dds, tif, template, tint, paint, swatch }] }:
#   dds       a vanilla map unpacked from its .edds (clothing-vanilla.mjs eddsToDds)
#   template  where to write it as a PNG at full size: the paint template, on the vanilla UV layout
#   tif       where to write the result: 8-bit RGBA, unassociated alpha (Workbench's import format)
#   tint      null, or { rgb: [r,g,b] 0..1, strength: 0..1, mask: { lum: [lo,hi], sat: [lo,hi] } }:
#             recolour the pixels inside the mask so their mean becomes rgb, keeping the weave, the
#             folds and the alpha channel (roughness in a _BCR map)
#   paint     null, or a PNG on the same layout, laid over the result by its own alpha
# Writes <dir of the first tif>/textures.json with per-item numbers.
import json, os, sys
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from tiff import write_tiff


def load(path):
    import bpy
    img = bpy.data.images.load(path, check_existing=False)
    w, h = img.size
    arr = np.empty(w * h * 4, np.float32)
    img.pixels.foreach_get(arr)
    bpy.data.images.remove(img)
    return arr.reshape(h, w, 4)[::-1]  # Blender rows run bottom-up; TIFF and PNG files top-down


def save_png(path, rgba):
    import bpy
    h, w, _ = rgba.shape
    img = bpy.data.images.new(os.path.basename(path), w, h, alpha=True)
    img.pixels.foreach_set(np.ascontiguousarray(rgba[::-1]).reshape(-1).astype(np.float32))
    img.filepath_raw = path
    img.file_format = "PNG"
    img.save()
    bpy.data.images.remove(img)


def smoothstep_in(v, lo, hi, soft=0.04):
    a = np.clip((v - (lo - soft)) / (2 * soft), 0, 1)
    b = np.clip(((hi + soft) - v) / (2 * soft), 0, 1)
    return a * b


def tint(rgba, t):
    rgb = rgba[:, :, :3]
    lum = rgb @ np.array([0.2126, 0.7152, 0.0722], np.float32)
    mx, mn = rgb.max(axis=2), rgb.min(axis=2)
    sat = np.where(mx > 1e-4, (mx - mn) / np.maximum(mx, 1e-4), 0)
    m = t.get("mask") or {}
    mask = smoothstep_in(lum, *m.get("lum", [0, 1])) * smoothstep_in(sat, *m.get("sat", [0, 1]))
    weight = mask * float(t.get("strength", 1))
    target = np.array(t["rgb"], np.float32)
    ref = float((lum * mask).sum() / max(mask.sum(), 1))  # the masked area's mean luminance becomes the target's
    out = rgb.copy()
    recoloured = np.clip(target[None, None, :] * (lum / max(ref, 1e-3))[:, :, None], 0, 1)
    out = rgb * (1 - weight[:, :, None]) + recoloured * weight[:, :, None]
    res = rgba.copy()
    res[:, :, :3] = out
    return res, {"mask_share": float(mask.mean()), "ref_luminance": ref}


def main():
    job = json.load(open(sys.argv[sys.argv.index("--") + 1]))
    report = []
    for it in job["items"]:
        src = load(it["dds"])
        if it.get("template"):
            save_png(it["template"], src)
        out = src
        info = {"tif": it["tif"], "size": [int(src.shape[1]), int(src.shape[0])]}
        if it.get("tint"):
            out, stats = tint(out, it["tint"])
            info.update(stats)
        if it.get("paint"):
            p = load(it["paint"])
            if p.shape != out.shape:
                raise SystemExit("paint %s is %dx%d, the vanilla map is %dx%d" % (it["paint"], p.shape[1], p.shape[0], out.shape[1], out.shape[0]))
            a = p[:, :, 3:4]
            out = out.copy()
            out[:, :, :3] = out[:, :, :3] * (1 - a) + p[:, :, :3] * a
            info["paint_share"] = float((a > 0.01).mean())
        out = np.clip(out, 0, 1)
        info["changed"] = float((np.abs(out - src).max(axis=2) > 2 / 255).mean())
        info["mean_rgb"] = [float(v) for v in out[:, :, :3].mean(axis=(0, 1))]
        write_tiff(it["tif"], (out * 255 + 0.5).astype(np.uint8))
        if it.get("swatch"):
            small = out[::8, ::8]
            save_png(it["swatch"], small)
        report.append(info)
        print("LF_CLOTHING_TEX", os.path.basename(it["tif"]), json.dumps(info))
    if job["items"]:
        with open(os.path.join(os.path.dirname(job["items"][0]["tif"]), "textures.json"), "w") as f:
            json.dump(report, f, indent=1)
    print("LF_CLOTHING_TEX done", len(report))


if __name__ == "__main__":
    main()
