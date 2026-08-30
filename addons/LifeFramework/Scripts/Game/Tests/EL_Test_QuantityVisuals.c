// red-proof: revert GetStackModelForQuantity to pick the FIRST threshold the quantity
// clears (drop the bestThreshold comparison) or drop the empty-model skip, then run
// `tools\cli test --tier fast`; each breakage below fails.

// tier: LOGIC
class EL_Test_QuantityVisuals : EL_Test
{
	protected static const ResourceName MODEL_SMALL = "{AAA0000000000001}Models/Small.xob";
	protected static const ResourceName MODEL_MEDIUM = "{AAA0000000000002}Models/Medium.xob";
	protected static const ResourceName MODEL_LARGE = "{AAA0000000000003}Models/Large.xob";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "quantity/visuals";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(EL_TestContext ctx)
	{
		TestEmptyList(ctx);
		TestBelowLowestTier(ctx);
		TestAtThreshold(ctx);
		TestAboveHighestTier(ctx);
		TestIgnoresMissingModel(ctx);
		TestOrderIndependent(ctx);
	}

	//------------------------------------------------------------------------------------------------
	protected ref EL_QuantityStack MakeStack(int amount, ResourceName model)
	{
		EL_QuantityStack stack = new EL_QuantityStack();
		stack.m_iQuantityAmount = amount;
		stack.m_StackModel = model;
		return stack;
	}

	//------------------------------------------------------------------------------------------------
	protected ref array<ref EL_QuantityStack> MakeTiers()
	{
		ref array<ref EL_QuantityStack> stacks = new array<ref EL_QuantityStack>();
		stacks.Insert(MakeStack(100, MODEL_SMALL));
		stacks.Insert(MakeStack(1000, MODEL_MEDIUM));
		stacks.Insert(MakeStack(10000, MODEL_LARGE));
		return stacks;
	}

	//------------------------------------------------------------------------------------------------
	protected void TestEmptyList(EL_TestContext ctx)
	{
		ref array<ref EL_QuantityStack> stacks = new array<ref EL_QuantityStack>();
		ResourceName model = EL_QuantityComponent.GetStackModelForQuantity(5000, stacks);
		ctx.True(model.IsEmpty(), "empty tier list yields no model");
	}

	//------------------------------------------------------------------------------------------------
	protected void TestBelowLowestTier(EL_TestContext ctx)
	{
		ResourceName model = EL_QuantityComponent.GetStackModelForQuantity(99, MakeTiers());
		ctx.True(model.IsEmpty(), "quantity below the lowest threshold yields no model");
	}

	//------------------------------------------------------------------------------------------------
	protected void TestAtThreshold(EL_TestContext ctx)
	{
		ResourceName model = EL_QuantityComponent.GetStackModelForQuantity(100, MakeTiers());
		ctx.True(model == MODEL_SMALL, "quantity exactly at a threshold selects that tier");
	}

	//------------------------------------------------------------------------------------------------
	protected void TestAboveHighestTier(EL_TestContext ctx)
	{
		ResourceName model = EL_QuantityComponent.GetStackModelForQuantity(100000, MakeTiers());
		ctx.True(model == MODEL_LARGE, "quantity above the highest threshold selects the top tier");
	}

	//------------------------------------------------------------------------------------------------
	protected void TestIgnoresMissingModel(EL_TestContext ctx)
	{
		ref array<ref EL_QuantityStack> stacks = new array<ref EL_QuantityStack>();
		stacks.Insert(MakeStack(100, MODEL_SMALL));
		stacks.Insert(MakeStack(1000, ResourceName.Empty));
		stacks.Insert(MakeStack(10000, MODEL_LARGE));

		ResourceName model = EL_QuantityComponent.GetStackModelForQuantity(15000, stacks);
		ctx.True(model == MODEL_LARGE, "a tier with no model is skipped, not selected");
	}

	//------------------------------------------------------------------------------------------------
	protected void TestOrderIndependent(EL_TestContext ctx)
	{
		ref array<ref EL_QuantityStack> stacks = new array<ref EL_QuantityStack>();
		stacks.Insert(MakeStack(10000, MODEL_LARGE));
		stacks.Insert(MakeStack(100, MODEL_SMALL));
		stacks.Insert(MakeStack(1000, MODEL_MEDIUM));

		ResourceName model = EL_QuantityComponent.GetStackModelForQuantity(1500, stacks);
		ctx.True(model == MODEL_MEDIUM, "highest cleared threshold wins regardless of list order");
	}
};