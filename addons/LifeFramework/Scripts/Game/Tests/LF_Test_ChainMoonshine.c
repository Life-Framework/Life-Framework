// red-proof: observed 2026-11-10 with `toolscli test --no-build --only LF_Test_ChainMoonshine`: a non-existent BerryJam prefab in the config failed `the moonshine chain is enabled` and `the moonshine chain resolves` (`chain moonshine disabled: a prefab does not resolve`), `SUMMARY tier=only passed=0 failed=1`; swapped for the real WaterBottle, green.

// tier: WORLD
//! The shipped moonshine chain (#420) loads from ResourceChains.conf with every prefab resolving,
//! is tier 1 with no gate, and its steps hand items on: the mash output is the still input, and
//! the still makes the contraband crate the convoy buyer already takes.
class LF_Test_ChainMoonshine : LF_Test
{
	override string GetName()
	{
		return "chains/moonshine";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ChainCatalog.ResetStatics();
		LF_ChainCatalog.Build(LF_ResourceChainCatalog.GetConfig());

		ctx.True(LF_ChainCatalog.IsEnabled("moonshine"), "the moonshine chain is enabled");
		LF_ChainDef chain = LF_ChainCatalog.FindChain("moonshine");
		ctx.NotNull(chain, "the moonshine chain resolves");
		if (!chain)
		{
			LF_ChainCatalog.ResetStatics();
			return;
		}

		ctx.Equal(1, chain.m_iTier, "moonshine is the easy tier");
		ctx.Equal(2, chain.m_aSteps.Count(), "two steps: mash, then distill");
		ctx.NotNull(chain.m_Heat, "a running site gives off heat");

		LF_ChainStepDef mash = chain.FindStep("mash");
		LF_ChainStepDef distill = chain.FindStep("distill");
		ctx.NotNull(mash, "the mash step exists");
		ctx.NotNull(distill, "the distill step exists");
		if (mash && distill)
		{
			ctx.EqualStr("barrel", mash.m_sStationType, "mash is done at a barrel");
			ctx.EqualStr("still", distill.m_sStationType, "distill is done at a still");
			ctx.Equal(LF_EChainGate.OK, LF_ChainGate.Evaluate(mash, false, 0, false), "the easy tier has no gate");
			ctx.Equal(LF_EChainGate.OK, LF_ChainGate.Evaluate(distill, false, 0, false), "the still has no gate either");
			ctx.EqualStr(mash.m_aOutputs[0].m_OutputPrefab, distill.m_aInputs[0].m_InputPrefab, "the mash output feeds the still");
			ctx.EqualStr(LF_Contraband.ITEM, distill.m_aOutputs[0].m_OutputPrefab, "the still makes the contraband crate");
			ctx.True(distill.m_iWork > mash.m_iWork, "the still is the slower step");
		}

		LF_ChainCatalog.ResetStatics();
	}
}
