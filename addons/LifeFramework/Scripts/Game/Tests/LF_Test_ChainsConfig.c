// red-proof: observed 2026-11-10 with `tools\cli test --no-build --only LF_Test_ChainsConfig`: passing hasLicense=true to the licensed step failed `a gate refuses a worker without the licence (expected 1, got 0)` and `SUMMARY tier=only passed=0 failed=1`; restored, green.

// tier: LOGIC
//! Illicit chain config (#416): a gate refuses a worker without the licence, the level or the
//! equipment; a malformed chain disables itself and every other chain still loads; the heat
//! arithmetic caps and decays.
class LF_Test_ChainsConfig : LF_Test
{
	protected static const ResourceName APPLE = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";
	protected static const ResourceName JUICE = "{1DBD99E6347DE1AD}Prefabs/Items/Drinks/AppleJuice.et";
	protected static const ResourceName NUGGET = "{6853E2E2AE878611}Prefabs/Resources/Mining/IronNugget.et";

	override string GetName()
	{
		return "chains/config";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_ChainStepDef Step(string id, string station)
	{
		LF_ChainStepDef step = new LF_ChainStepDef();
		step.m_sStepId = id;
		step.m_sStationType = station;
		step.m_iWork = 5;
		step.m_aInputs = new array<ref LF_ProcessingInput>();
		step.m_aOutputs = new array<ref LF_ProcessingOutput>();

		LF_ProcessingInput input = new LF_ProcessingInput();
		input.m_InputPrefab = APPLE;
		input.m_iInputAmount = 2;
		step.m_aInputs.Insert(input);

		LF_ProcessingOutput output = new LF_ProcessingOutput();
		output.m_OutputPrefab = JUICE;
		output.m_iOutputAmount = 1;
		step.m_aOutputs.Insert(output);
		return step;
	}

	//------------------------------------------------------------------------------------------------
	protected LF_ChainDef Chain(string id, int tier)
	{
		LF_ChainDef chain = new LF_ChainDef();
		chain.m_sChainId = id;
		chain.m_iTier = tier;
		chain.m_aSteps = new array<ref LF_ChainStepDef>();
		chain.m_aSteps.Insert(Step("mix", "barrel"));
		chain.m_aEquipment = new array<ref LF_ChainEquipmentDef>();
		return chain;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		RunGates(ctx);
		RunDisable(ctx);
		RunHeat(ctx);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunGates(LF_TestContext ctx)
	{
		LF_ChainStepDef open = Step("open", "barrel");
		ctx.Equal(LF_EChainGate.OK, LF_ChainGate.Evaluate(open, false, 0, false), "a step with no gates lets anyone work");

		LF_ChainStepDef licensed = Step("licensed", "still");
		licensed.m_eLicense = LF_ELicenseType.CRIMINAL_BLACKMARKET;
		ctx.Equal(LF_EChainGate.LICENSE, LF_ChainGate.Evaluate(licensed, false, 99, true), "a gate refuses a worker without the licence");
		ctx.Equal(LF_EChainGate.OK, LF_ChainGate.Evaluate(licensed, true, 0, false), "the licence alone opens a licence-only step");

		LF_ChainStepDef skilled = Step("skilled", "still");
		skilled.m_iMinLevel = 5;
		ctx.Equal(LF_EChainGate.LEVEL, LF_ChainGate.Evaluate(skilled, true, 4, true), "a gate refuses a worker below the level");
		ctx.Equal(LF_EChainGate.OK, LF_ChainGate.Evaluate(skilled, false, 5, false), "the exact level passes");

		LF_ChainStepDef equipped = Step("equipped", "press");
		equipped.m_sEquipmentId = "coil";
		ctx.Equal(LF_EChainGate.EQUIPMENT, LF_ChainGate.Evaluate(equipped, true, 99, false), "a gate refuses a worker without the equipment");
		ctx.Equal(LF_EChainGate.OK, LF_ChainGate.Evaluate(equipped, false, 0, true), "carrying the equipment unlocks the step");

		LF_ChainStepDef all = Step("all", "press");
		all.m_eLicense = LF_ELicenseType.CRIMINAL_BLACKMARKET;
		all.m_iMinLevel = 3;
		all.m_sEquipmentId = "coil";
		ctx.Equal(LF_EChainGate.LICENSE, LF_ChainGate.Evaluate(all, false, 0, false), "licence is checked first");
		ctx.Equal(LF_EChainGate.LEVEL, LF_ChainGate.Evaluate(all, true, 0, false), "level is checked second");
		ctx.Equal(LF_EChainGate.EQUIPMENT, LF_ChainGate.Evaluate(all, true, 3, false), "equipment is checked last");

		ctx.EqualStr("", LF_ChainGate.ReasonKey(LF_EChainGate.OK), "OK has no refusal text");
		ctx.EqualStr("#LF-Chain_NeedLicense", LF_ChainGate.ReasonKey(LF_EChainGate.LICENSE), "licence refusal key");
		ctx.EqualStr("#LF-Chain_NeedLevel", LF_ChainGate.ReasonKey(LF_EChainGate.LEVEL), "level refusal key");
		ctx.EqualStr("#LF-Chain_NeedEquipment", LF_ChainGate.ReasonKey(LF_EChainGate.EQUIPMENT), "equipment refusal key");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunDisable(LF_TestContext ctx)
	{
		ctx.EqualStr("", LF_ChainCatalog.Validate(Chain("good", 1)), "a well formed chain validates");

		LF_ChainDef noSteps = Chain("no-steps", 1);
		noSteps.m_aSteps.Clear();
		ctx.True(!LF_ChainCatalog.Validate(noSteps).IsEmpty(), "a chain with no steps is invalid");

		LF_ChainDef badTier = Chain("bad-tier", 0);
		ctx.True(!LF_ChainCatalog.Validate(badTier).IsEmpty(), "tier 0 is invalid");
		badTier.m_iTier = LF_ChainCatalog.MAX_TIER + 1;
		ctx.True(!LF_ChainCatalog.Validate(badTier).IsEmpty(), "a tier above the maximum is invalid");

		LF_ChainDef noStation = Chain("no-station", 1);
		noStation.m_aSteps[0].m_sStationType = "";
		ctx.True(!LF_ChainCatalog.Validate(noStation).IsEmpty(), "a step with no station is invalid");

		LF_ChainDef noWork = Chain("no-work", 1);
		noWork.m_aSteps[0].m_iWork = 0;
		ctx.True(!LF_ChainCatalog.Validate(noWork).IsEmpty(), "a step with no work is invalid");

		LF_ChainDef noRecipe = Chain("no-recipe", 1);
		noRecipe.m_aSteps[0].m_aOutputs.Clear();
		ctx.True(!LF_ChainCatalog.Validate(noRecipe).IsEmpty(), "a step with no output is invalid");

		LF_ChainDef ghostEquipment = Chain("ghost-equipment", 2);
		ghostEquipment.m_aSteps[0].m_sEquipmentId = "coil";
		ctx.True(!LF_ChainCatalog.Validate(ghostEquipment).IsEmpty(), "a step naming unlisted equipment is invalid");

		LF_ChainEquipmentDef coil = new LF_ChainEquipmentDef();
		coil.m_sEquipmentId = "coil";
		coil.m_Prefab = NUGGET;
		coil.m_iPrice = 250;
		ghostEquipment.m_aEquipment.Insert(coil);
		ctx.EqualStr("", LF_ChainCatalog.Validate(ghostEquipment), "listing the equipment makes it valid");

		LF_ChainDef coldHeat = Chain("bad-heat", 1);
		coldHeat.m_Heat = new LF_ChainHeatDef();
		coldHeat.m_Heat.m_fPerWork = -1;
		ctx.True(!LF_ChainCatalog.Validate(coldHeat).IsEmpty(), "a negative heat setting is invalid");

		// One bad chain between two good ones: only the bad one is disabled.
		LF_ResourceChainConfig config = new LF_ResourceChainConfig();
		config.m_aChains = new array<ref LF_ChainDef>();
		config.m_aChains.Insert(Chain("alpha", 1));
		config.m_aChains.Insert(noSteps);
		config.m_aChains.Insert(Chain("omega", 3));
		config.m_aChains.Insert(Chain("alpha", 2));
		LF_ChainCatalog.Build(config, false);

		ctx.True(LF_ChainCatalog.IsEnabled("alpha"), "the chain before the bad one still loads");
		ctx.True(LF_ChainCatalog.IsEnabled("omega"), "the chain after the bad one still loads");
		ctx.True(LF_ChainCatalog.IsDisabled("no-steps"), "the bad chain is disabled");
		ctx.True(!LF_ChainCatalog.IsEnabled("no-steps"), "the bad chain does not run");
		ctx.Equal(2, LF_ChainCatalog.EnabledCount(), "two chains run");
		ctx.Equal(1, LF_ChainCatalog.FindChain("alpha").m_iTier, "a duplicate id keeps the first chain");
		ctx.NotNull(LF_ChainCatalog.FindStep("omega", "mix"), "a step of an enabled chain resolves");
		ctx.True(LF_ChainCatalog.FindStep("no-steps", "mix") == null, "a step of a disabled chain does not");

		LF_ChainCatalog.ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunHeat(LF_TestContext ctx)
	{
		LF_ChainHeatDef heat = new LF_ChainHeatDef();
		heat.m_fPerWork = 2;
		heat.m_fDecayPerSecond = 1;
		heat.m_fMax = 10;
		heat.m_fRangeM = 40;

		ctx.EqualFloat(6, LF_ChainHeat.AfterWork(heat, 0, 3), 0.001, "work adds heat per unit");
		ctx.EqualFloat(10, LF_ChainHeat.AfterWork(heat, 6, 10), 0.001, "heat stops at the maximum");
		ctx.EqualFloat(4, LF_ChainHeat.AfterDecay(heat, 10, 6), 0.001, "heat fades with time");
		ctx.EqualFloat(0, LF_ChainHeat.AfterDecay(heat, 3, 60), 0.001, "heat never goes below zero");
		ctx.EqualFloat(40, LF_ChainHeat.RangeAt(heat, 10), 0.001, "full heat smells at full range");
		ctx.EqualFloat(10, LF_ChainHeat.RangeAt(heat, 2.5), 0.001, "a quarter of the heat smells a quarter as far");
		ctx.EqualFloat(0, LF_ChainHeat.RangeAt(heat, 0), 0.001, "a cold site cannot be smelled");
		ctx.EqualFloat(0, LF_ChainHeat.RangeAt(null, 5), 0.001, "a chain without heat settings makes no trace");
	}
}
