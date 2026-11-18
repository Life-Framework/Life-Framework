# A glass milk bottle, modelled and textured in Blender (spec "source": "blender"), no generation credits.
# Run by the generate stage:  blender --background --factory-startup --python model.py -- <job.json>
# A half-litre bottle: milk-white body, one blue band, a foil cap. No text, no dairy name.
# Stands on Z, base at the bottom. Material ids: 0.0 milk white, 0.3 blue band, 0.6 foil cap.
import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "fishing-lib"))
import geo
from geo import piece, lathe_z, finish

MILK, BLUE, FOIL = 0.0, 0.3, 0.6

body = piece(MILK)
lathe_z(body, 0, 0, [(0.0, 0.0), (0.0, 0.026), (0.003, 0.032), (0.1, 0.032), (0.12, 0.026), (0.14, 0.0125), (0.158, 0.0125), (0.16, 0.0138), (0.166, 0.0138), (0.166, 0.0)], seg=20)
band = piece(BLUE)
lathe_z(band, 0, 0, [(0.038, 0.0), (0.038, 0.0327), (0.082, 0.0327), (0.082, 0.0)], seg=20)
cap = piece(FOIL)
lathe_z(cap, 0, 0, [(0.150, 0.0), (0.150, 0.0147), (0.1695, 0.0153), (0.1695, 0.0)], seg=20)

finish("MilkBottle", geo.job(), [
    (MILK, (0.88, 0.88, 0.84), 0.15, 0.0),
    (BLUE, (0.08, 0.22, 0.48), 0.4, 0.0),
    (FOIL, (0.7, 0.7, 0.72), 0.3, 0.9),
], 0.4, center=True, tile=0.15)
