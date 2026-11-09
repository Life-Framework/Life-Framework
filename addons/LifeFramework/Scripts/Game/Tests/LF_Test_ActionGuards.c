// red-proof: remove the empty-prefab/positive-amount checks from
// LF_GatherAction.IsGatherRequestConfigured or LF_ProcessAction.IsRecipeConfigured
// and run `tools\cli test --tier fast`; the malformed configuration assertions
// go red.

// tier: LOGIC
class LF_Test_ActionGuards : LF_Test
{
	override string GetName()
	{
		return "actions/configuration-guards";
	}

	override void Run(LF_TestContext ctx)
	{
		ctx.True(LF_GatherAction.IsGatherRequestConfigured("{C9D661E5B0714711}Prefabs/Items/Food/Apple.et", 1), "configured gather request is allowed");
		ctx.False(LF_GatherAction.IsGatherRequestConfigured(ResourceName.Empty, 1), "gather without an item prefab is rejected");
		ctx.False(LF_GatherAction.IsGatherRequestConfigured("{C9D661E5B0714711}Prefabs/Items/Food/Apple.et", 0), "gather with zero amount is rejected");

		ref array<ref LF_ProcessingInput> inputs = new array<ref LF_ProcessingInput>();
		ref array<ref LF_ProcessingOutput> outputs = new array<ref LF_ProcessingOutput>();
		ctx.False(LF_ProcessAction.IsRecipeConfigured(null, outputs), "recipe without inputs is rejected");
		ctx.False(LF_ProcessAction.IsRecipeConfigured(inputs, null), "recipe without outputs is rejected");

		LF_ProcessingInput input = new LF_ProcessingInput();
		input.m_InputPrefab = "{6853E2E2AE878611}Prefabs/Resources/Mining/IronNugget.et";
		input.m_iInputAmount = 2;
		inputs.Insert(input);

		LF_ProcessingOutput output = new LF_ProcessingOutput();
		output.m_OutputPrefab = "{BCD1E8CD7F975074}Prefabs/Resources/Mining/IronBar.et";
		output.m_iOutputAmount = 1;
		outputs.Insert(output);

		ctx.True(LF_ProcessAction.IsRecipeConfigured(inputs, outputs), "complete processing recipe is allowed");
		output.m_iOutputAmount = 0;
		ctx.False(LF_ProcessAction.IsRecipeConfigured(inputs, outputs), "zero-output recipe is rejected");
	}
};
