// red-proof: 2026-11-16 `tools\cli test --no-build --only LF_Test_GridFrameLabels` before Grid_frame.layer existed failed `- marker GridMarker_c0r0 is placed` (49 times) and `- the map board GridMapBoard is placed`; green once the layer was written.
// tier: WORLD
//! The grid world frame (#1810): every cell of the 7 x 7 lattice has a marker post named
//! GridMarker_cXrY standing inside its cell, a plate GridPlate_cXrY beside it (the claim's slug, or
//! FREE), and the gate spawn has a map board GridMapBoard. The lattice is the one in
//! tools/grid/lattice.mjs: 12 m cells from x 22, z 22.
class LF_Test_GridFrameLabels : LF_Test
{
	protected static const float CELL_ORIGIN = 22;
	protected static const float CELL_SIZE = 12;
	protected static const int CELLS = 7;

	override string GetName()
	{
		return "grid/frame-labels";
	}

	override void Run(LF_TestContext ctx)
	{
		BaseWorld world = GetGame().GetWorld();
		int placed;
		for (int row = 0; row < CELLS; row++)
		{
			for (int col = 0; col < CELLS; col++)
			{
				string cell = "c" + col + "r" + row;
				float x0 = CELL_ORIGIN + col * CELL_SIZE;
				float z0 = CELL_ORIGIN + row * CELL_SIZE;

				IEntity marker = null;
				if (!RedProof())
					marker = world.FindEntityByName("GridMarker_" + cell);
				ctx.NotNull(marker, "marker GridMarker_" + cell + " is placed");
				if (marker)
				{
					placed++;
					vector at = marker.GetOrigin();
					ctx.True(at[0] >= x0 && at[0] <= x0 + CELL_SIZE && at[2] >= z0 && at[2] <= z0 + CELL_SIZE, "GridMarker_" + cell + " stands inside its cell (x " + x0 + ".." + (x0 + CELL_SIZE) + ", z " + z0 + ".." + (z0 + CELL_SIZE) + "), at " + at[0] + "," + at[2]);
				}

				IEntity plate = null;
				if (!RedProof())
					plate = world.FindEntityByName("GridPlate_" + cell);
				ctx.NotNull(plate, "plate GridPlate_" + cell + " is placed");
			}
		}
		ctx.Equal(CELLS * CELLS, placed, "one marker per lattice cell");

		IEntity board = null;
		if (!RedProof())
			board = world.FindEntityByName("GridMapBoard");
		ctx.NotNull(board, "the map board GridMapBoard is placed");
	}
}
