# One-off map edit for the spike strip (docs/asset-pipeline/review-clean.md "Textures": edit the map).
# The Meshy maps read as pale shiny metal in the game's daylight (look 2026-11-13). The strip should be
# matte black rubber with bare steel spikes: the spike faces (centre above the plate's top, z > 0.0505 m
# in the FBX) are painted steel, everything else rubber. The UV islands are found by rasterising each
# face's UV triangle, so the maps keep their scuffs and the normal map is left alone.
#   blender --background --factory-startup --python tools/assets/spike-strip/tone-maps.py -- <addon Data dir> <SpikeStrip.fbx> <backup dir>
# The maps are edited in place; the first run keeps the originals as <backup dir>/*_orig.tif and every
# run reads from those, so a rerun does not darken twice. Then rebuild the .edds (a Workbench refresh).
import os, shutil, sys
import bpy
import numpy as np

data_dir, fbx, backup = sys.argv[sys.argv.index("--") + 1:][:3]
STEEL_Z = 0.0505
HEADER = 154  # write_tiff in clean.py: 8 + 2 + 11 tags * 12 + 4 + 8, then 1024 x 1024 RGBA bytes
PX = 1024


def load(path):
    raw = open(path, "rb").read()
    px = np.frombuffer(raw[HEADER:], dtype=np.uint8).reshape(PX, PX, 4).astype(np.float32) / 255.0
    return raw[:HEADER], px


def save(path, head, px):
    with open(path, "wb") as f:
        f.write(head)
        f.write((np.clip(px, 0, 1) * 255 + 0.5).astype(np.uint8).tobytes())


os.makedirs(backup, exist_ok=True)
paths = {}
for tag in ("BCR", "NMO"):
    live = os.path.join(data_dir, f"SpikeStrip_{tag}.tif")
    orig = os.path.join(backup, f"SpikeStrip_{tag}_orig.tif")
    if not os.path.exists(orig):
        shutil.copyfile(live, orig)
    paths[tag] = (live, orig)

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=fbx)
me = bpy.data.objects["SpikeStrip_LOD0"].data
me.calc_loop_triangles()
uv = np.array([l.uv[:] for l in me.uv_layers[0].data], dtype=np.float64)
mask = np.zeros((PX, PX), dtype=bool)
for t in me.loop_triangles:
    if me.polygons[t.polygon_index].center.z <= STEEL_Z:
        continue
    p = uv[list(t.loops)] * PX
    x0, x1 = int(max(0, np.floor(p[:, 0].min()))), int(min(PX - 1, np.ceil(p[:, 0].max())))
    y0, y1 = int(max(0, np.floor(p[:, 1].min()))), int(min(PX - 1, np.ceil(p[:, 1].max())))
    xs, ys = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
    a, b, c = p
    d = (b[1] - c[1]) * (a[0] - c[0]) + (c[0] - b[0]) * (a[1] - c[1])
    if abs(d) < 1e-12:
        continue
    w1 = ((b[1] - c[1]) * (xs - c[0]) + (c[0] - b[0]) * (ys - c[1])) / d
    w2 = ((c[1] - a[1]) * (xs - c[0]) + (a[0] - c[0]) * (ys - c[1])) / d
    inside = (w1 >= -0.02) & (w2 >= -0.02) & (1 - w1 - w2 >= -0.02)
    mask[y0:y1 + 1, x0:x1 + 1] |= inside
mask = mask[::-1]  # UV v runs up, TIFF rows run down
steel = mask[:, :, None]
print("TONE steel share of the atlas", round(float(mask.mean()), 3))

head, bcr = load(paths["BCR"][1])
_, nmo = load(paths["NMO"][1])
lum = bcr[:, :, :3].mean(axis=2, keepdims=True)
rubber_c = 0.035 + 0.07 * lum                    # near black, scuffs kept
steel_c = 0.12 + 0.45 * lum                      # mid grey bare steel
bcr[:, :, :3] = np.where(steel, steel_c, rubber_c)
bcr[:, :, 3:4] = np.where(steel, 0.42, 0.9)      # roughness: rubber matt, steel a little satin
nmo[:, :, 2:3] = np.where(steel, 0.85, 0.0)      # metallic: rubber none, spikes metal
save(paths["BCR"][0], head, bcr)
save(paths["NMO"][0], head, nmo)
print("TONE done: base mean", round(float(bcr[:, :, :3].mean()), 3), "rough", round(float(bcr[:, :, 3].mean()), 3), "metal", round(float(nmo[:, :, 2].mean()), 3))
