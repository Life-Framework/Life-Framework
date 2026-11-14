// red-proof (#932): restore the two backpack tiers in MoneyStack.et; "a large cash stack stays the money mesh" fails.
// red-proof: revert the Combine source-quantity clamp (min with source
// quantity), the Split bounds guard, SortByQuantity's int keys to string keys,
// the init-quantity application in EOnInit, or the top-tier model swap in
// UpdateStackModel, then run `tools\cli test --tier all`; each breakage below fails.
// tier: WORLD
class LF_Test_QuantityOperations : LF_Test
{
	protected static const ResourceName MONEY_STACK_PREFAB = "{5439738849229352}Prefabs/Items/Currencies/MoneyStack.et";
	protected static const ResourceName MONEY_STACK_MODEL = "{773BE5E227A5A9CB}Assets/Items/Currencies/MoneyStack.xob";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "quantity/operations";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		Resource res = Resource.Load(MONEY_STACK_PREFAB);
		ctx.True(res != null && res.IsValid(), "money stack prefab loads: " + MONEY_STACK_PREFAB);
		if (ctx.FailureCount() > 0)
			return;

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.LOCAL;

		IEntity source = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		IEntity dest = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		ctx.True(source != null && dest != null, "two money stacks spawn");
		if (ctx.FailureCount() > 0)
			return;

		LF_QuantityComponent sourceQty = LF_Component<LF_QuantityComponent>.Find(source);
		LF_QuantityComponent destQty = LF_Component<LF_QuantityComponent>.Find(dest);
		ctx.True(sourceQty != null && destQty != null, "both stacks carry LF_QuantityComponent");
		if (ctx.FailureCount() > 0)
			return;

		ctx.True(LF_Component<RplComponent>.Find(source) != null && LF_Component<RplComponent>.Find(dest) != null, "spawned stacks carry RplComponent");
		if (ctx.FailureCount() > 0)
			return;

		TestCombineClamp(ctx, sourceQty, destQty);
		TestSplitBounds(ctx, res, params);
		TestNumericSort(ctx, res, params);
		TestQuantityVisuals(ctx, res, params);
	}

	//------------------------------------------------------------------------------------------------
	protected void TestCombineClamp(LF_TestContext ctx, LF_QuantityComponent sourceQty, LF_QuantityComponent destQty)
	{
		sourceQty.SetQuantity(5);
		destQty.SetQuantity(3);
		ctx.Equal(5, sourceQty.GetQuantity(), "source stack set to 5");
		ctx.Equal(3, destQty.GetQuantity(), "destination stack set to 3");

		EntityID sourceId = sourceQty.GetOwner().GetID();

		int transferred;
		destQty.Combine(sourceQty, 100, transferred);

		ctx.Equal(5, transferred, "Combine transfers only the source's 5, not the requested 100");
		ctx.Equal(8, destQty.GetQuantity(), "destination gained exactly 5");
		ctx.True(GetGame().GetWorld().FindEntityByID(sourceId) == null, "fully drained source entity is deleted");
	}

	//------------------------------------------------------------------------------------------------
	protected void TestSplitBounds(LF_TestContext ctx, Resource res, EntitySpawnParams params)
	{
		IEntity stack = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		ctx.True(stack != null, "split fixture stack spawns");
		if (ctx.FailureCount() > 0)
			return;

		LF_QuantityComponent qty = LF_Component<LF_QuantityComponent>.Find(stack);
		ctx.True(qty != null, "split fixture carries LF_QuantityComponent");
		if (ctx.FailureCount() > 0)
			return;

		qty.SetQuantity(10);
		ctx.Equal(10, qty.GetQuantity(), "split fixture set to 10");

		qty.Split(12);
		ctx.Equal(10, qty.GetQuantity(), "split larger than the stack is rejected, stack untouched");

		qty.Split(0);
		ctx.Equal(10, qty.GetQuantity(), "split of zero is rejected, stack untouched");

		qty.Split(10);
		ctx.Equal(10, qty.GetQuantity(), "split equal to the stack is rejected, stack untouched");

		qty.Split(4);
		ctx.Equal(6, qty.GetQuantity(), "split of 4 leaves 6 on the source");
	}

	//------------------------------------------------------------------------------------------------
	protected void TestNumericSort(LF_TestContext ctx, Resource res, EntitySpawnParams params)
	{
		IEntity a = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		IEntity b = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		IEntity c = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		ctx.True(a != null && b != null && c != null, "sort fixture stacks spawn");
		if (ctx.FailureCount() > 0)
			return;

		LF_QuantityComponent qtyA = LF_Component<LF_QuantityComponent>.Find(a);
		LF_QuantityComponent qtyB = LF_Component<LF_QuantityComponent>.Find(b);
		LF_QuantityComponent qtyC = LF_Component<LF_QuantityComponent>.Find(c);
		if (ctx.FailureCount() > 0)
			return;

		qtyA.SetQuantity(10);
		qtyB.SetQuantity(2);
		qtyC.SetQuantity(7);

		array<LF_QuantityComponent> components();
		components.Insert(qtyA);
		components.Insert(qtyB);
		components.Insert(qtyC);

		array<LF_QuantityComponent> descending = LF_QuantityComponent.SortByQuantity(components);
		ctx.Equal(10, descending[0].GetQuantity(), "numeric sort descending puts 10 first");
		ctx.Equal(7, descending[1].GetQuantity(), "numeric sort descending puts 7 second");
		ctx.Equal(2, descending[2].GetQuantity(), "numeric sort descending puts 2 last");

		array<LF_QuantityComponent> ascending = LF_QuantityComponent.SortByQuantity(components, false);
		ctx.Equal(2, ascending[0].GetQuantity(), "numeric sort ascending puts 2 first");
		ctx.Equal(7, ascending[1].GetQuantity(), "numeric sort ascending puts 7 second");
		ctx.Equal(10, ascending[2].GetQuantity(), "numeric sort ascending puts 10 last");
	}

	//------------------------------------------------------------------------------------------------
	protected void TestQuantityVisuals(LF_TestContext ctx, Resource res, EntitySpawnParams params)
	{
		IEntity stack = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		ctx.True(stack != null, "visual fixture stack spawns");
		if (ctx.FailureCount() > 0)
			return;

		LF_QuantityComponent qty = LF_Component<LF_QuantityComponent>.Find(stack);
		ctx.True(qty != null, "visual fixture carries LF_QuantityComponent");
		if (ctx.FailureCount() > 0)
			return;

		ctx.Equal(100, qty.GetQuantity(), "money stack spawns at its configured init quantity");
		ctx.True(qty.GetActiveStackModel() == MONEY_STACK_MODEL, "init quantity shows the small tier model");
		if (ctx.FailureCount() > 0)
			return;

		qty.SetQuantity(10000);
		ctx.Equal(10000, qty.GetQuantity(), "stack set to the top tier quantity");
		ctx.True(qty.GetActiveStackModel() == MONEY_STACK_MODEL, "a large cash stack stays the money mesh, never a backpack (#932)");

		qty.SetQuantity(50);
		ctx.True(qty.GetActiveStackModel().IsEmpty(), "quantity below every tier restores the base model");
	}
};