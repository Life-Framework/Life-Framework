# A tin of instant coffee, modelled and textured in Blender (spec "source": "blender"), no generation credits.
# Run by the generate stage:  blender --background --factory-startup --python model.py -- <job.json>
# A round tin, plain dark-red wrap with one cream band, steel rims and a slip-on yellow lid. No text, no brand.
# Stands on Z, base at the bottom. Material ids: 0.0 red wrap, 0.3 cream, 0.6 steel, 0.9 lid.
import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "fishing-lib"))
import geo
from geo import piece, lathe_z, finish

WRAP, CREAM, STEEL, LID = 0.0, 0.3, 0.6, 0.9
R = 0.045

rim = piece(STEEL)
lathe_z(rim, 0, 0, [(0.0, 0.0), (0.0, R - 0.002), (0.002, R + 0.0006), (0.007, R + 0.0006), (0.007, 0.0)], seg=20)
wrap = piece(WRAP)
lathe_z(wrap, 0, 0, [(0.007, 0.0), (0.007, R), (0.096, R), (0.096, 0.0)], seg=20)
band = piece(CREAM)
lathe_z(band, 0, 0, [(0.040, 0.0), (0.040, R + 0.0006), (0.064, R + 0.0006), (0.064, 0.0)], seg=20)
lip = piece(STEEL)
lathe_z(lip, 0, 0, [(0.096, 0.0), (0.096, R + 0.0006), (0.100, R + 0.0006), (0.100, 0.0)], seg=20)
lid = piece(LID)
lathe_z(lid, 0, 0, [(0.100, 0.0), (0.100, R + 0.0014), (0.1125, R + 0.0014), (0.1135, R - 0.003), (0.1135, 0.0)], seg=20)

finish("CoffeeTin", geo.job(), [
    (WRAP, (0.30, 0.07, 0.05), 0.5, 0.0),
    (CREAM, (0.86, 0.80, 0.62), 0.6, 0.0),
    (STEEL, (0.62, 0.62, 0.60), 0.35, 0.9),
    (LID, (0.82, 0.64, 0.12), 0.45, 0.0),
], 0.35, center=True, tile=0.15)
