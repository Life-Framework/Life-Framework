// red-proof: PENDING. The base game data project was missing (Steam restoring it), so the
// dedicated server could not boot and `tools\cli test` was not run. No failing line observed yet.
// To prove red: remove the `s_TickOwner && s_TickOwner != this` early return in
// LF_JailCellComponent.OnPostInit and run `tools\cli test --no-build --tier all`;
// "a second cell does not start its own jail tick" must fail.

// tier: WORLD
class EL_Test_RobustnessWorld : EL_Test
{
	protected static const ResourceName JAIL_CELL = "{AA32D528D592E54C}Prefabs/Police/EL_JailCell.et";

	override string GetName()
	{
		return "robustness/single-jail-tick";
	}

	override void Run(EL_TestContext ctx)
	{
		IEntity placed = GetGame().GetWorld().FindEntityByName(LF_Jail.CELL_ID);
		ctx.True(placed != null, "Roadmap_Jail_Cell is placed");
		if (ctx.FailureCount() > 0)
			return;

		LF_JailCellComponent placedCell = EL_Component<LF_JailCellComponent>.Find(placed);
		ctx.True(placedCell != null, "the placed cell carries LF_JailCellComponent");
		if (ctx.FailureCount() > 0)
			return;

		ctx.True(placedCell.IsTickOwner(), "the placed cell owns the jail tick");

		vector offset = "0 0 40";
		IEntity second = EL_Utils.SpawnEntityPrefab(JAIL_CELL, placed.GetOrigin() + offset);
		ctx.True(second != null, "a second jail cell spawns");
		if (ctx.FailureCount() > 0)
			return;

		LF_JailCellComponent secondCell = EL_Component<LF_JailCellComponent>.Find(second);
		ctx.True(secondCell != null, "the second cell carries LF_JailCellComponent");
		if (secondCell)
			ctx.False(secondCell.IsTickOwner(), "a second cell does not start its own jail tick");

		SCR_EntityHelper.DeleteEntityAndChildren(second);
		ctx.True(placedCell.IsTickOwner(), "deleting the second cell leaves the placed cell owning the tick");
	}
}
