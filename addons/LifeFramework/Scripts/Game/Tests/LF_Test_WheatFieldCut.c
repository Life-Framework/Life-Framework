// red-proof: Observed 2026-11-19 with the batched red run `--no-build --red LF_Test_WheatFieldCut`: `RED PROOF OK (all 1 listed test(s) fail with the hook on)`, the cut skipped (stubble 0, bitset 0).
//! harvest/wheat-field-cut (#3287): a placed LF_WheatField draws one local patch per cell (a 60 x 40 m box is
//! 260); cutting 20 cells leaves 240 patches, 20 stubbles and 20 bits in the bitset, a repeat cut changes
//! nothing, and Regrow stands all 260 again. The test deletes every mesh it made.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_WheatFieldCut : LF_Test
{
	protected static const ResourceName FIELD = "{5FF806585DB556B3}Prefabs/Harvest/LF_WheatField.et";
	protected static const vector SPOT = "40 0 40";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "harvest/wheat-field-cut";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		vector at = SPOT;
		at[1] = GetGame().GetWorld().GetSurfaceY(at[0], at[2]);
		IEntity entity = LF_Utils.SpawnEntityPrefab(FIELD, at);
		LF_WheatField field = LF_WheatField.Cast(entity);
		ctx.NotNull(field, "the wheat field prefab spawns as an LF_WheatField");
		if (field)
			Check(ctx, field);

		if (field)
			field.ClearLocal();

		if (entity)
			SCR_EntityHelper.DeleteEntityAndChildren(entity);
	}

	//------------------------------------------------------------------------------------------------
	protected void Check(LF_TestContext ctx, LF_WheatField field)
	{
		ctx.Equal(260, field.CellCount(), "a 60 x 40 m box is 260 cells of 3 m");

		// A dedicated test server draws nothing by itself; force the local draw so the meshes can be counted.
		field.ShowLocal(true);
		ctx.Equal(260, field.PatchCount(), "an untouched field has 260 local patches");
		ctx.Equal(0, field.StubbleCount(), "an untouched field has no stubble");
		ctx.Equal(0, field.CutCount(), "an untouched field has no cut cell");

		array<int> cells = {};
		for (int i = 0; i < 20; i++)
		{
			cells.Insert(i * 13);
		}

		int cut = 0;
		if (!RedProof())
			cut = field.CutCells(cells);

		ctx.Equal(20, cut, "cutting 20 standing cells reports 20");
		ctx.Equal(240, field.PatchCount(), "240 patches stand after the cut");
		ctx.Equal(20, field.StubbleCount(), "the 20 cut cells show stubble");
		ctx.Equal(20, field.CutCount(), "the bitset counts 20 cut cells");

		ctx.Equal(0, field.CutCells(cells), "cutting the same cells again cuts nothing");
		ctx.Equal(240, field.PatchCount(), "a repeat cut leaves 240 patches");
		ctx.Equal(20, field.StubbleCount(), "a repeat cut leaves 20 stubbles");

		array<int> outside = {-1, 260, 9999};
		ctx.Equal(0, field.CutCells(outside), "cells outside the grid are ignored");

		field.Regrow();
		ctx.Equal(260, field.PatchCount(), "regrowing stands 260 patches again");
		ctx.Equal(0, field.StubbleCount(), "regrowing removes the stubble");
		ctx.Equal(0, field.CutCount(), "regrowing clears the bitset");
	}
}
