# One-off map edit for the handcuffs (docs/asset-pipeline/review-clean.md "Textures": edit the map).
# The Meshy maps read as polished chrome in the game's daylight (look 2026-11-13: pale, glassy cuffs).
# Darker base colour, rougher, a little less metallic: worn blued steel. Originals kept as *_orig.png.
#   blender --background --factory-startup --python tools/assets/handcuffs/tone-maps.py -- <absolute tmp/assets/handcuffs/raw>
# then: tools\cli asset run handcuffs --from clean
import os, shutil, sys
import bpy
import numpy as np

raw = sys.argv[sys.argv.index("--") + 1]


def edit(name, fn):
    path = os.path.join(raw, name)
    orig = path.replace(".png", "_orig.png")
    if not os.path.exists(orig):
        shutil.copyfile(path, orig)
    img = bpy.data.images.load(orig)
    px = np.array(img.pixels[:], dtype=np.float32).reshape(-1, 4)
    before = px[:, :3].mean()
    px = fn(px)
    out = bpy.data.images.new(name + "_t", img.size[0], img.size[1], alpha=True)
    out.pixels = px.ravel().tolist()
    out.filepath_raw = path
    out.file_format = "PNG"
    out.save()
    print("TONE", name, round(float(before), 3), "->", round(float(px[:, :3].mean()), 3))


def base(px):
    px[:, :3] = px[:, :3] * 0.55
    return px


def rough(px):
    px[:, :3] = np.clip(0.25 + px[:, :3] * 0.9, 0, 1)
    return px


def metal(px):
    px[:, :3] = px[:, :3] * 0.85
    return px


edit("tex_base_color.png", base)
edit("tex_roughness.png", rough)
edit("tex_metallic.png", metal)
