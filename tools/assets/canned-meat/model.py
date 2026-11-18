# A tin of corned beef, modelled and textured in Blender (spec "source": "blender"), no generation credits.
# Run by the generate stage:  blender --background --factory-startup --python model.py -- <job.json>
# A closed rectangular tin: steel top and base, a plain dark-red wrap with one cream band, a key tab. No text, no brand.
# Stands on Z, base at the bottom. Material ids: 0.0 red wrap, 0.3 cream band, 0.6 steel.
import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "fishing-lib"))
import geo
from geo import piece, block, finish

WRAP, CREAM, STEEL = 0.0, 0.3, 0.6

base = piece(STEEL)
block(base, -0.0500, 0.0500, -0.0320, 0.0320, 0.0, 0.006, bevel=0.0025, seg=2)
wrap = piece(WRAP)
block(wrap, -0.0496, 0.0496, -0.0316, 0.0316, 0.006, 0.0440, bevel=0.0015, seg=2)
band = piece(CREAM)
block(band, -0.0502, 0.0502, -0.0322, 0.0322, 0.0195, 0.0300, bevel=0.0006)
top = piece(STEEL)
block(top, -0.0500, 0.0500, -0.0320, 0.0320, 0.0440, 0.0500, bevel=0.0025, seg=2)
key = piece(STEEL)
block(key, 0.0270, 0.0400, -0.0070, 0.0070, 0.0498, 0.0520, bevel=0.0008)

finish("CannedMeat", geo.job(), [
    (WRAP, (0.40, 0.08, 0.06), 0.5, 0.0),
    (CREAM, (0.86, 0.80, 0.62), 0.6, 0.0),
    (STEEL, (0.62, 0.62, 0.60), 0.35, 0.9),
], 0.3, center=True, tile=0.15)
