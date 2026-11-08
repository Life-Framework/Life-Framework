// red-proof: the cocaine output was pointed at the coca-leaves prefab and
// `tools\cli test --no-build --tier fast` printed
// [ELTEST] FAIL processing/recipe-chains
// [ELTEST]   - cocaine input and output prefabs are distinct
// The distinct pair was then restored.

// tier: LOGIC
class EL_Test_RecipeChains : EL_Test
{
	override string GetName()
	{
		return "processing/recipe-chains";
	}

	protected void AssertChain(EL_TestContext ctx, ResourceName inputPrefab, ResourceName outputPrefab, string chain)
	{
		ref EL_ProcessingInput input = new EL_ProcessingInput();
		input.m_InputPrefab = inputPrefab;
		input.m_iInputAmount = 1;

		ref EL_ProcessingOutput output = new EL_ProcessingOutput();
		output.m_OutputPrefab = outputPrefab;
		output.m_iOutputAmount = 1;

		ref array<ref EL_ProcessingInput> inputs = new array<ref EL_ProcessingInput>();
		ref array<ref EL_ProcessingOutput> outputs = new array<ref EL_ProcessingOutput>();
		inputs.Insert(input);
		outputs.Insert(output);

		ctx.True(!input.m_InputPrefab.IsEmpty(), chain + " input prefab is set");
		ctx.True(!output.m_OutputPrefab.IsEmpty(), chain + " output prefab is set");
		ctx.True(input.m_InputPrefab != output.m_OutputPrefab, chain + " input and output prefabs are distinct");
		ctx.True(EL_ProcessAction.IsRecipeConfigured(inputs, outputs), chain + " recipe is configured");
	}

	override void Run(EL_TestContext ctx)
	{
		AssertChain(ctx, "{DD593D31D4F3DFBF}Prefabs/Items/Drugs/Coca_leaves.et", "{B854F08E28254CB8}Prefabs/Items/Drugs/Cocaine.et", "cocaine");
		AssertChain(ctx, "{73CF2BF72525F5D5}Prefabs/Items/Food/RowanBerries.et", "{CD44DE4529F22D17}Prefabs/Items/Food/BerryJam.et", "jam");
		AssertChain(ctx, "{2B0213949BE91F38}Prefabs/Items/Resources/EmptyBeerBottle.et", "{C3B9A61B460BE287}Prefabs/Items/Drinks/BeerBottle.et", "beer");
	}
};
