# Run by tools/validation/asset-materials-selftest.mjs under Blender (it ships numpy): the numbers
# of materials.py. Prints one "MATFAIL <what>" line per broken expectation and "MATDONE" at the end.
#   blender --background --factory-startup --python materials_selftest.py -- <plan.json>
import json, os, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import materials

plan = json.load(open(sys.argv[sys.argv.index("--") + 1]))


def check(cond, what):
    if not cond:
        print("MATFAIL", what)


# 2 x 2 atlas: top row plain plastic-looking pixels, bottom row Meshy-metal pixels. Meshy reads like chrome:
# rough 0.1, metal 1 on the metal row; the plastic row has rough 0.2, metal 0.
rough = np.array([[0.2, 0.2], [0.1, 0.1]], np.float32)
metal = np.array([[0.0, 0.0], [1.0, 1.0]], np.float32)
base = np.full((2, 2, 3), 0.8, np.float32)
base[0, 0] = (0.9, 0.8, 0.1)  # a yellow pixel keeps its hue

p = plan["default"]
masks = materials.region_masks(p, metal, lambda h: np.zeros((2, 2), bool))
nb, nr, nm = materials.apply(p, base, rough, metal, masks)
check(np.all((nr[0] >= 0.75) & (nr[0] <= 0.85)), "plastic roughness stays 0.75..0.85")
check(np.all(nm[0] <= 0.03), "plastic metallic is near zero")
check(np.all(nm[1] >= 0.75), "a pixel Meshy marked metal stays metal")
check(np.all(nr[1] >= 0.3), "metal is no longer a mirror (roughness >= 0.3)")
check(nb[0, 0, 0] > nb[0, 0, 2] * 3, "plastic keeps the yellow hue")
check(base[0, 0, 0] == np.float32(0.9), "the input base array is not edited in place")

# A height region: the top-left pixel is in a masked face, and the class pins rough and metal.
h = plan["height"]
tall = np.array([[True, False], [False, False]])
masks = materials.region_masks(h, metal, lambda m: tall)
nb, nr, nm = materials.apply(h, base, rough, metal, masks)
check(abs(nr[0, 0] - 0.42) < 1e-6 and abs(nm[0, 0] - 0.85) < 1e-6, "the height region takes its class's pinned roughness and metal")
check(abs(nr[0, 1] - 0.9) < 1e-6 and nm[0, 1] == 0, "outside the region the default class pins rubber")
check(abs(nb[0, 0, 0] - nb[0, 0, 2]) < 1e-6 and nb[0, 1, 0] < 0.11, "base_lum makes grey, rubber near black")
print("MATDONE")
