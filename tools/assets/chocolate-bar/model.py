# A wrapped bar of milk chocolate, modelled and textured in Blender (spec "source": "blender"), no generation credits.
# Run by the generate stage:  blender --background --factory-startup --python model.py -- <job.json>
# A flat slab in a dark-red wrapper with one gold band and silver crimped ends. No text, no brand.
# Lies on Z (thickness), base at the bottom. Material ids: 0.0 wrapper, 0.3 gold band, 0.6 silver ends.
import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "fishing-lib"))
import geo
from geo import piece, block, finish

WRAP, GOLD, SILVER = 0.0, 0.3, 0.6

slab = piece(WRAP)
block(slab, -0.070, 0.070, -0.035, 0.035, 0.0, 0.012, bevel=0.0018, seg=2)
band = piece(GOLD)
block(band, -0.022, 0.022, -0.0356, 0.0356, -0.0004, 0.0124, bevel=0.0005)
for sx in (-1, 1):
    end = piece(SILVER)
    block(end, 0.070 * sx if sx > 0 else -0.0775, 0.0775 if sx > 0 else -0.070, -0.033, 0.033, 0.0030, 0.0090, bevel=0.0008)

finish("ChocolateBar", geo.job(), [
    (WRAP, (0.42, 0.06, 0.07), 0.45, 0.0),
    (GOLD, (0.80, 0.62, 0.16), 0.35, 0.6),
    (SILVER, (0.70, 0.70, 0.72), 0.3, 0.9),
], 0.3, center=True, tile=0.15)
