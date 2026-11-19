// red-proof: with the pack layer removed (an empty header as the pack source, the `RedProof()` hook) the chain catalog has no `mill-wheat` machine, so the `mill-wheat machine loads through the overlay` line and the recipe lines fail.
//! #3293: the pack's wheat chain goods and mill (harvester spike, section 6), read through the overlays under the
//! Everon 1989 header (the pack leg). The grain sack and the flour sack are real LF quantity items counted in
//! kilograms, and the `mill-wheat` machine turns 2 kg of grain into 1 kg of flour, up to 50 kg of grain at once.
//! The granary curve and the bakery premium row wait for a price unit finer than a whole cash (see the PR).
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronWheatChain : LF_Test
{
	static const ResourceName OVERLAY = "{7B2E5A1C93D40F30}Configs/Chains/LF_Chain_Wheat_Everon1989.conf";
	static const ResourceName GRAIN = "{7B2E5A1C93D40F11}Prefabs/Items/Harvest/LF_GrainSack.et";
	static const ResourceName FLOUR = "{7B2E5A1C93D40F12}Prefabs/Items/Harvest/LF_FlourSack.et";
	static const string MILL = "mill-wheat";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/wheat-chain";
	}

	//------------------------------------------------------------------------------------------------
	//! A sack prefab loads and carries a quantity component that starts as a full 50 kg sack.
	protected void CheckSack(LF_TestContext ctx, string label, ResourceName sack)
	{
		Resource resource = Resource.Load(sack);
		ctx.True(resource && resource.IsValid(), label + " resolves to a real prefab");
		if (!resource || !resource.IsValid())
			return;

		IEntitySource source = SCR_BaseContainerTools.FindEntitySource(resource);
		ctx.True(source != null, label + " has an entity source");
		if (!source)
			return;

		bool found;
		for (int i = 0; i < source.GetComponentCount(); i++)
		{
			IEntityComponentSource component = source.GetComponent(i);
			if (!component || component.GetClassName() != "LF_QuantityComponent")
				continue;

			found = true;
			int maxQuantity;
			component.Get("m_iMaxQuantity", maxQuantity);
			ctx.Equal(50, maxQuantity, label + " holds 50 kg at most");
		}

		ctx.True(found, label + " is an LF quantity item");
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_MissionHeader header = LF_MissionHeader.GetCurrent();
		ctx.NotNull(header, "the pack leg runs under an LF mission header");
		if (header)
			ctx.True(header.m_aLF_ConfigOverlays && header.m_aLF_ConfigOverlays.Find(OVERLAY) != -1, "the header lists the wheat chain overlay");

		CheckSack(ctx, "grain sack", GRAIN);
		CheckSack(ctx, "flour sack", FLOUR);

		if (RedProof())
		{
			// No pack layer: an empty header as the pack source.
			LF_MissionConfigPackSource none = new LF_MissionConfigPackSource();
			none.SetHeader(new LF_MissionHeader());
			LF_ConfigService.SetPackSource(none);
		}

		LF_ResourceChainCatalog.Reload();
		LF_ResourceMachineDef mill = LF_ResourceChainCatalog.FindMachine(MILL);
		if (RedProof())
		{
			LF_ConfigService.Reset();
			LF_ResourceChainCatalog.Reload();
		}

		ctx.NotNull(mill, MILL + " machine loads through the overlay");
		if (!mill)
			return;

		ctx.Equal(1, mill.m_aInputs.Count(), "the mill takes one input");
		ctx.Equal(1, mill.m_aOutputs.Count(), "the mill makes one output");
		if (mill.m_aInputs.Count() != 1 || mill.m_aOutputs.Count() != 1)
			return;

		LF_ProcessingInput input = mill.m_aInputs[0];
		LF_ProcessingOutput output = mill.m_aOutputs[0];
		ctx.True(LF_Utils.SamePrefab(input.m_InputPrefab, GRAIN), "the mill eats grain");
		ctx.True(LF_Utils.SamePrefab(output.m_OutputPrefab, FLOUR), "the mill makes flour");
		ctx.Equal(2, input.m_iInputAmount, "2 kg of grain");
		ctx.Equal(1, output.m_iOutputAmount, "make 1 kg of flour");
		ctx.Equal(25, mill.m_iMaxBatches, "50 kg of grain is 25 batches");
	}
}
