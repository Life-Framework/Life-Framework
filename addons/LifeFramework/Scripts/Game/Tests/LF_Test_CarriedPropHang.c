// red-proof: Observed 2026-11-17 with `tools/cli test --no-build --red LF_Test_CarriedPropHang`: `RED PROOF OK`; with the hook on (identity slot transform) "the handle is on the hand (0.455442 m off)", "the prop hangs upright" and "the long side runs along the carrier's forward axis" fail; green with `--only`.

// tier: LOGIC
//! A carried prop hangs from the hand (#2174 grip): for any hand pose, the slot transform that
//! LF_CarriedPropComponent.SolveHanging returns puts the prop's handle on the hand bone, keeps the
//! prop upright and turns its long side along the carrier's forward axis.
class LF_Test_CarriedPropHang : LF_Test
{
	override string GetName()
	{
		return "character/carried-prop-hang";
	}

	override void Run(LF_TestContext ctx)
	{
		vector handle = "0 0.455 0.02";
		vector forward = "0.6 0 0.8";
		// Hand poses: at rest, mid swing forward, mid swing back with the wrist rolled.
		array<vector> angles = {"0 0 180", "30 -40 160", "-25 35 -150"};
		foreach (vector ypr : angles)
		{
			vector pivot[4];
			Math3D.AnglesToMatrix(ypr, pivot);
			pivot[3] = "100 1.1 200";

			vector local[4];
			if (!RedProof())
				LF_CarriedPropComponent.SolveHanging(pivot, forward, handle, local);
			else
				Math3D.MatrixIdentity4(local);

			// The prop's world transform is the hand's times the slot's.
			vector world[4];
			Math3D.MatrixMultiply4(pivot, local, world);
			vector handleWorld = world[3] + world[0] * handle[0] + world[1] * handle[1] + world[2] * handle[2];
			ctx.True(vector.Distance(handleWorld, pivot[3]) < 0.001, string.Format("hand %1: the handle is on the hand (%2 m off)", ypr, vector.Distance(handleWorld, pivot[3])));
			ctx.True(world[1][1] > 0.999, string.Format("hand %1: the prop hangs upright (up %2)", ypr, world[1]));
			ctx.True(vector.Dot(world[0], forward) > 0.999, string.Format("hand %1: the long side runs along the carrier's forward axis", ypr));
		}
	}
}
