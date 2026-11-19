# A dry-cured saucisson, modelled and textured in Blender (spec "source": "blender"), no generation credits.
# Run by the generate stage:  blender --background --factory-startup --python model.py -- <job.json>
# A fat sausage along Y with a pale mould-dusted casing, four loops of butcher's twine and a hanging cord. No text, no brand.
# Material ids: 0.0 dark red casing, 0.3 pale dusting, 0.6 twine.
import math, os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "fishing-lib"))
import geo
from geo import piece, lathe_y, sweep, finish
from mathutils import Vector

CASING, DUST, TWINE = 0.0, 0.3, 0.6

R = 0.0240
body_profile = [(-0.115, 0.0), (-0.112, 0.010), (-0.104, 0.019), (-0.085, R), (-0.040, R * 1.02),
                (0.040, R * 1.02), (0.085, R), (0.104, 0.019), (0.112, 0.010), (0.115, 0.0)]
lathe_y(piece(CASING), 0, 0, body_profile, seg=14)

# Pale dusting: two slightly fatter bands over the belly of the sausage.
for y0, y1 in ((-0.078, -0.036), (0.012, 0.062)):
    lathe_y(piece(DUST), 0, 0, [(y0, 0.0), (y0 + 0.002, R * 0.98), (y1 - 0.002, R * 0.98), (y1, 0.0)], seg=14)

# Twine: thin rings round the casing.
for y in (-0.095, -0.030, 0.035, 0.095):
    lathe_y(piece(TWINE), 0, 0, [(y - 0.003, 0.0), (y - 0.0025, R * 1.05), (y + 0.0025, R * 1.05), (y + 0.003, 0.0)], seg=14)

# The hanging cord: a loop standing off one end.
loop = [Vector((0.0, 0.112, 0.0)), Vector((0.0, 0.135, 0.010)), Vector((0.0, 0.150, 0.026)), Vector((0.0, 0.130, 0.040)),
        Vector((0.0, 0.112, 0.028)), Vector((0.0, 0.112, 0.0))]
sweep(piece(TWINE), loop, 0.0016, seg=6)

finish("Saucisson", geo.job(), [
    (CASING, (0.30, 0.10, 0.07), 0.6, 0.0),
    (DUST, (0.78, 0.74, 0.66), 0.85, 0.0),
    (TWINE, (0.68, 0.55, 0.34), 0.9, 0.0),
], 0.4, center=True, tile=0.2)
