// red-proof: revert the Combine source-quantity clamp (min with source
// quantity), the Split bounds guard, SortByQuantity's int keys to string keys,
// the init-quantity application in EOnInit, or the top-tier model swap in
// UpdateStackModel, then run `tools\cli test --tier all`; each breakage below fails.
// tier: WORLD
class EL_Test_QuantityOperations : EL_Test
{
	protected static const ResourceName MONEY_STACK_PREFAB = "{5439738849229352}Prefabs/Items/Currencies/MoneyStack.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "quantity/operations";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(EL_TestContext ctx)
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

		EL_QuantityComponent sourceQty = EL_Component<EL_QuantityComponent>.Find(source);
		EL_QuantityComponent destQty = EL_Component<EL_QuantityComponent>.Find(dest);
		ctx.True(sourceQty != null && destQty != null, "both stacks carry EL_QuantityComponent");
		if (ctx.FailureCount() > 0)
			return;

		ctx.True(EL_Component<RplComponent>.Find(source) != null && EL_Component<RplComponent>.Find(dest) != null, "spawned stacks carry RplComponent");
		if (ctx.FailureCount() > 0)
			return;

		TestCombineClamp(ctx, sourceQty, destQty);
		TestSplitBounds(ctx, res, params);
		TestNumericSort(ctx, res, params);
		TestQuantityVisuals(ctx, res, params);
	}

	//------------------------------------------------------------------------------------------------
	protected void TestCombineClamp(EL_TestContext ctx, EL_QuantityComponent sourceQty, EL_QuantityComponent destQty)
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
	protected void TestSplitBounds(EL_TestContext ctx, Resource res, EntitySpawnParams params)
	{
		IEntity stack = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		ctx.True(stack != null, "split fixture stack spawns");
		if (ctx.FailureCount() > 0)
			return;

		EL_QuantityComponent qty = EL_Component<EL_QuantityComponent>.Find(stack);
		ctx.True(qty != null, "split fixture carries EL_QuantityComponent");
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
	protected void TestNumericSort(EL_TestContext ctx, Resource res, EntitySpawnParams params)
	{
		IEntity a = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		IEntity b = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		IEntity c = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		ctx.True(a != null && b != null && c != null, "sort fixture stacks spawn");
		if (ctx.FailureCount() > 0)
			return;

		EL_QuantityComponent qtyA = EL_Component<EL_QuantityComponent>.Find(a);
		EL_QuantityComponent qtyB = EL_Component<EL_QuantityComponent>.Find(b);
		EL_QuantityComponent qtyC = EL_Component<EL_QuantityComponent>.Find(c);
		if (ctx.FailureCount() > 0)
			return;

		qtyA.SetQuantity(10);
		qtyB.SetQuantity(2);
		qtyC.SetQuantity(7);

		array<EL_QuantityComponent> components();
		components.Insert(qtyA);
		components.Insert(qtyB);
		components.Insert(qtyC);

		array<EL_QuantityComponent> descending = EL_QuantityComponent.SortByQuantity(components);
		ctx.Equal(10, descending[0].GetQuantity(), "numeric sort descending puts 10 first");
		ctx.Equal(7, descending[1].GetQuantity(), "numeric sort descending puts 7 second");
		ctx.Equal(2, descending[2].GetQuantity(), "numeric sort descending puts 2 last");

array<EL_QuantityComponent> ascending = EL_QuantityComponent.SortByQuantity(components, false);
		ctx.Equal(2, ascending[0].GetQuantity(), "numeric sort ascending puts 2 first");
		ctx.Equal(7, ascending[1].GetQuantity(), "numeric sort ascending puts 7 second");
		ctx.Equal(10, ascending[2].GetQuantity(), "numeric sort ascending puts 10 last");
	}

	//------------------------------------------------------------------------------------------------
	protected void TestQuantityVisuals(EL_TestContext ctx, Resource res, EntitySpawnParams params)
	{
		IEntity stack = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		ctx.True(stack != null, "visual fixture stack spawns");
		if (ctx.FailureCount() > 0)
			return;

		EL_QuantityComponent qty = EL_Component<EL_QuantityComponent>.Find(stack);
		ctx.True(qty != null, "visual fixture carries EL_QuantityComponent");
		if (ctx.FailureCount() > 0)
			return;

		ctx.Equal(100, qty.GetQuantity(), "money stack spawns at its configured init quantity");

		// Headless servers load mesh geometry for physics; when the base mesh is absent the
		// model-swap assertions cannot hold and are skipped rather than flaked.
		VObject baseModel = stack.GetVObject();
		if (!baseModel)
			return;

		qty.SetQuantity(10000);
		ctx.Equal(10000, qty.GetQuantity(), "stack set to the top tier quantity");

		VObject topModel = stack.GetVObject();
		ctx.True(topModel != null, "top tier stack still has a mesh");
		if (ctx.FailureCount() > 0)
			return;

		ctx.True(topModel != baseModel, "top tier quantity swaps the mesh to a different model");

		qty.SetQuantity(50);
		VObject restoredModel = stack.GetVObject();
		ctx.True(restoredModel == baseModel, "quantity below every tier restores the base mesh");
	}
};