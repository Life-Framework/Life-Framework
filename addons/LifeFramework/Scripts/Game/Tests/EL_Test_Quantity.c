// red-proof: change an expected value (e.g. clamp quantity 1 -> 2, or the
// rejection return) and run `tools\cli test --tier all`. Red run pending:
// first boot after the EPF dependency removal lands.
// tier: WORLD
class EL_Test_QuantityStack : EL_Test
{
	protected static const ResourceName WOOD_LOG_PREFAB = "{32F5A906D8A573BC}Prefabs/Resources/WoodCutting/WoodLog_01_Small.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "quantity/stack-clamp";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(EL_TestContext ctx)
	{
		Resource res = Resource.Load(WOOD_LOG_PREFAB);
		ctx.True(res.IsValid(), "wood log prefab loads: " + WOOD_LOG_PREFAB);
		if (ctx.FailureCount() > 0)
			return;

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.LOCAL;

		IEntity log = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		ctx.True(log != null, "wood log spawns an entity");
		if (ctx.FailureCount() > 0)
			return;

		EL_QuantityComponent quantity = EL_Component<EL_QuantityComponent>.Find(log);
		ctx.True(quantity != null, "wood log carries EL_QuantityComponent");
		if (ctx.FailureCount() > 0)
			return;

		// SetQuantity dereferences the owner's RplComponent; assert it exists
		// so a missing component fails cleanly instead of VME-crashing the suite.
		ctx.True(EL_Component<RplComponent>.Find(log) != null, "wood log carries RplComponent (SetQuantity guard)");
		if (ctx.FailureCount() > 0)
			return;

		ctx.Equal(1, quantity.GetMaxQuantity(), "wood log max quantity is 1 (HandCarryItem_Base)");
		ctx.Equal(1, quantity.GetQuantity(), "fresh stack starts at quantity 1");

		int change;
		ctx.Equal(1, quantity.AddQuantity(5, true, change), "AddQuantity(5) clamps to max 1");
		ctx.Equal(0, change, "clamped AddQuantity reports 0 actually added");

		change = -1;
		ctx.Equal(0, quantity.AddQuantity(5, false, change), "AddQuantity(5, noPartial) returns 0 on overflow");
		ctx.Equal(-1, change, "rejected AddQuantity leaves out change untouched");
		ctx.Equal(1, quantity.GetQuantity(), "quantity unchanged after rejected add");

		change = 0;
		ctx.Equal(0, quantity.RemoveQuantity(5, false, change), "RemoveQuantity(5, noPartial) returns 0 on overdraw");
		ctx.Equal(0, change, "rejected RemoveQuantity reports 0 change");
		ctx.Equal(1, quantity.GetQuantity(), "quantity unchanged after rejected remove");

		EntityID logId = log.GetID();
		change = 0;
		ctx.Equal(0, quantity.RemoveQuantity(1, true, change), "RemoveQuantity(1) drives quantity to 0");
		ctx.Equal(-1, change, "removal reports -1 change");
		ctx.True(GetGame().GetWorld().FindEntityByID(logId) == null, "stack at quantity 0 deletes its entity");
	}
}
