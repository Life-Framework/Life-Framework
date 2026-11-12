// red-proof: 2026-11-12 with LF_JailCellComponent.ReapplyPlaced returning before ApplyPlacement,
// `tools\cli test --no-build --only LF_Test_JailPlacedCell` failed "SyncPlacedCell puts the cell origin
// back on the placed cell". Restored; green.

//! LF_Jail.SyncPlacedCell finds the cell through its component, not by an entity name, so a world
//! that places the cell under any name (or a pack) keeps the sentence tests working (#323).
// tier: WORLD
class LF_Test_JailPlacedCell : LF_Test
{
	override string GetName()
	{
		return "jail/placed-cell";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_JailCellComponent cell = LF_JailCellComponent.GetPlaced();
		ctx.NotNull(cell, "the world places a jail cell (LF_JailCellComponent)");
		if (!cell || !cell.GetOwner())
			return;

		vector placed = cell.GetOwner().GetOrigin();
		LF_Jail.SetCellOrigin("1 2 3");
		ctx.True(vector.Distance(LF_Jail.GetCellOrigin(), placed) > 1, "a moved cell origin is not the placed one");

		LF_Jail.SyncPlacedCell();
		ctx.True(LF_Jail.HasCell(), "the sentence has a cell after the sync");
		ctx.True(vector.Distance(LF_Jail.GetCellOrigin(), placed) < 0.01, "SyncPlacedCell puts the cell origin back on the placed cell");
	}
}
