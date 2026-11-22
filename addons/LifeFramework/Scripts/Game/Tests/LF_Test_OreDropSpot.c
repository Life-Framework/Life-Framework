// red-proof: 2026-11-22, `tools\cli test --no-build --red LF_Test_OreDropSpot` (the hook puts every spot on the box edge with no margin): RED PROOF OK, "a piece lies outside the rock's footprint" failed for every slot.

//! Where ore lies after it breaks off a rock (LF_OreDrop.Spot): outside the rock's footprint box, toward the
//! miner first, then fanned to either side, no two slots closer than LF_OreDrop.SPACING, the same answer every time.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_OreDropSpot : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "mining/ore-drop-spot";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// red-proof hook: with no margin a piece is put on the box edge, inside the clearance the rule demands.
		float margin = LF_OreDrop.MARGIN;
		if (RedProof())
			margin = 0;

		array<vector> halves = {Vector(1.1, 0, 0.8), Vector(0.3, 0, 0.3), Vector(2.0, 0, 0.5)};
		array<vector> aways = {Vector(1, 0, 0), Vector(0, 0, -1), Vector(-1, 0, 1), Vector(0.3, 0.9, 0.2)};
		foreach (vector half : halves)
		{
			foreach (vector away : aways)
			{
				array<vector> spots = {};
				for (int slot = 0; slot < LF_OreDrop.SLOTS; slot++)
				{
					vector spot = LF_OreDrop.Spot(half[0], half[2], away, slot, margin);
					spots.Insert(spot);
					string what = string.Format("box %1x%2, away %3, slot %4", half[0], half[2], away, slot);
					ctx.True(LF_OreDrop.OutsideBox(spot[0], spot[2], half[0], half[2], LF_OreDrop.MARGIN), "a piece lies outside the rock's footprint: " + what);
					ctx.EqualFloat(0, spot[1], 0.0001, "the spot is a ground-plane offset, the probe sets the height: " + what);
				}

				float closest = 1e9;
				for (int i = 0; i < spots.Count(); i++)
				{
					for (int j = i + 1; j < spots.Count(); j++)
					{
						float apart = vector.Distance(spots[i], spots[j]);
						if (apart < closest)
							closest = apart;
					}
				}

				ctx.True(closest >= LF_OreDrop.SPACING - 0.001, string.Format("no two pieces share a spot (closest %1 m): box %2x%3, away %4", closest, half[0], half[2], away));

				vector flat = away;
				flat[1] = 0;
				flat.Normalize();
				ctx.True(vector.Dot(spots[0], flat) > 0, string.Format("the first piece lies on the miner's side: box %1x%2, away %3", half[0], half[2], away));
			}
		}

		vector again = LF_OreDrop.Spot(1.1, 0.8, vector.Right, 4);
		vector same = LF_OreDrop.Spot(1.1, 0.8, vector.Right, 4);
		ctx.EqualFloat(0, vector.Distance(again, same), 0.0001, "the same rock and slot give the same spot");
		vector wrapped = LF_OreDrop.Spot(1.1, 0.8, vector.Right, 4 + LF_OreDrop.SLOTS);
		ctx.EqualFloat(0, vector.Distance(again, wrapped), 0.0001, "the fan wraps after its last slot");
		vector degenerate = LF_OreDrop.Spot(1, 1, vector.Zero, 0);
		ctx.True(degenerate.Length() > 1, "no miner direction still gives a spot outside the rock");
		ctx.EqualFloat(1, LF_OreDrop.ExitDistance(1, 0.5, 1, 0), 0.0001, "a ray along x leaves a 1 x 0.5 box at 1 m");
		ctx.EqualFloat(0.5, LF_OreDrop.ExitDistance(1, 0.5, 0, -1), 0.0001, "a ray along z leaves it at 0.5 m");
		ctx.False(LF_OreDrop.OutsideBox(0.8, 0.2, 1, 0.5, 0.3), "a point inside the grown box is not outside");
	}
}
