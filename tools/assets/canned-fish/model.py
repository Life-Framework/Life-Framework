# A tin of sardines in oil, modelled and textured in Blender (spec "source": "blender"), no generation credits.
# Run by the generate stage:  blender --background --factory-startup --python model.py -- <job.json>
# A flat rounded tin, plain steel sides, a blue lid panel with one cream band and a pull-ring tab. No text, no brand.
# Stands on Z, base at the bottom. Material ids: 0.0 blue, 0.3 cream, 0.6 steel.
import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "fishing-lib"))
import geo
from geo import piece, block, finish

BLUE, CREAM, STEEL = 0.0, 0.3, 0.6

body = piece(STEEL)
block(body, -0.0525, 0.0525, -0.035, 0.035, 0.0, 0.028, bevel=0.0045, seg=3)
lid = piece(BLUE)
block(lid, -0.0475, 0.0475, -0.0300, 0.0300, 0.0272, 0.0292, bevel=0.0006)
band = piece(CREAM)
block(band, -0.0400, 0.0400, -0.0070, 0.0070, 0.0288, 0.0300, bevel=0.0003)
ring = piece(STEEL)
block(ring, 0.0345, 0.0480, -0.0070, 0.0070, 0.0288, 0.0312, bevel=0.0008)

finish("CannedFish", geo.job(), [
    (BLUE, (0.05, 0.14, 0.32), 0.4, 0.2),
    (CREAM, (0.86, 0.80, 0.62), 0.6, 0.0),
    (STEEL, (0.62, 0.62, 0.60), 0.35, 0.9),
], 0.25, center=True, tile=0.15)
