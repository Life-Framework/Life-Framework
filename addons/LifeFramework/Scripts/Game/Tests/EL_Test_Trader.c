// red-proof: flip an expected value in Run (e.g. assert item.m_ValuePerItem == 999
// or output.m_iOutputAmount == 0). The test runner prints FAIL trader/item-config
// or FAIL trader/processing-config and exits nonzero.

// tier: LOGIC
class EL_Test_TraderItemConfig : EL_Test
{
	override string GetName()
	{
		return "trader/item-config";
	}

	override void Run(EL_TestContext ctx)
	{
		ref EL_TraderItem item = new EL_TraderItem();
		item.m_ItemPrefab = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";
		item.m_ValuePerItem = 5;
		item.m_bIllegal = false;

		ctx.EqualStr("{C9D661E5B0714711}Prefabs/Items/Food/Apple.et", item.m_ItemPrefab, "item prefab matches");
		ctx.Equal(5, item.m_ValuePerItem, "item value is 5");
		ctx.False(item.m_bIllegal, "apple is legal item");

		ref EL_TraderItem illegalItem = new EL_TraderItem();
		illegalItem.m_ValuePerItem = 500;
		illegalItem.m_bIllegal = true;
		ctx.True(illegalItem.m_bIllegal, "contraband is marked illegal");
		ctx.Equal(500, illegalItem.m_ValuePerItem, "contraband has higher value");
	}
};

//------------------------------------------------------------------------------------------------
// tier: LOGIC
class EL_Test_ProcessingRecipeConfig : EL_Test
{
	override string GetName()
	{
		return "trader/processing-config";
	}

	override void Run(EL_TestContext ctx)
	{
		ref EL_ProcessingInput input = new EL_ProcessingInput();
		input.m_InputPrefab = "{6853E2E2AE878611}Prefabs/Resources/Mining/IronNugget.et";
		input.m_iInputAmount = 2;

		ref EL_ProcessingOutput output = new EL_ProcessingOutput();
		output.m_OutputPrefab = "{BCD1E8CD7F975074}Prefabs/Resources/Mining/IronBar.et";
		output.m_iOutputAmount = 1;

		ctx.Equal(2, input.m_iInputAmount, "2 nuggets required per smelt");
		ctx.Equal(1, output.m_iOutputAmount, "1 bar produced per smelt");
		ctx.True(!input.m_InputPrefab.IsEmpty(), "input prefab is set");
		ctx.True(!output.m_OutputPrefab.IsEmpty(), "output prefab is set");
	}
};
