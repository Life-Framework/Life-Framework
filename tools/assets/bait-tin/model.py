# The bait tin, modelled and textured in Blender (spec "source": "blender"), no generation credits.
# Run by the generate stage:  blender --background --factory-startup --python model.py -- <job.json>
# A round green-enamelled tobacco tin with a pressed steel lid and a wire clasp, the kind an Everon angler
# keeps worms in on the belt. Stands on Z, base at the bottom. Material ids: 0.0 paint, 0.6 steel.
import math, os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "fishing-lib"))
import geo
from geo import piece, block, lathe_z, sweep, finish
from mathutils import Vector

PAINT, STEEL = 0.0, 0.6
R, H = 0.0475, 0.042

body = piece(PAINT)
lathe_z(body, 0, 0, [(0.0, 0.0), (0.0, R - 0.003), (0.002, R), (0.026, R), (0.026, R - 0.0035), (0.012, R - 0.004), (0.0, R - 0.0035)], seg=20)
# the rolled lip and the lid in steel
lid = piece(STEEL)
lathe_z(lid, 0, 0, [(0.024, R + 0.0008), (0.034, R + 0.0014), (H - 0.003, R + 0.0014), (H - 0.0015, R - 0.002), (H, R - 0.006), (H, R - 0.017), (H - 0.0016, R - 0.019), (H - 0.0016, 0.001)], seg=20)
hinge = piece(STEEL)
block(hinge, -0.009, 0.009, -R - 0.0036, -R + 0.002, 0.022, 0.038, bevel=0.0008)                      # hinge knuckle at the back
clasp = piece(STEEL)
block(clasp, -0.006, 0.006, R - 0.001, R + 0.0034, 0.02, 0.04, bevel=0.0008)                          # clasp tab at the front

finish("BaitTin", geo.job(), [
    (PAINT, (0.07, 0.13, 0.06), 0.45, 0.0),
    (STEEL, (0.34, 0.34, 0.34), 0.4, 0.9),
], 0.25, center=True, tile=0.15)
