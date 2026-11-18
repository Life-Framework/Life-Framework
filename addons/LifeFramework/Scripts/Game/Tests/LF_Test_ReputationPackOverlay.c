// red-proof: observed 2026-11-18 `tools\cli test --no-build --red LF_Test_ReputationPackOverlay` printed `RED PROOF OK`; with the hook skipping the pack source it failed `patch: tier 2 pays the pack's 11% (expected 11, got 10)`, `patch: the pack's contract award replaces the core 10 (expected 25, got 10)` and `add: the pack's contract exists`. `--only` with the gate, contract and pay-bonus tests: passed=4 failed=0.
//! #3044: the standing tables read pack overlays. A pack row with an id the core has patches it (a
//! faction's words, an award's points, a gate's tier, a pay row, a contract's award), a new id adds a
//! row, and `remove` takes one out; fields the pack leaves out stay core. Without a pack the core files
//! answer alone. The test overlays (Configs/Tests/LF_Test_Reputation*Overlay.conf) are reached through
//! a mission header built in code.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_ReputationPackOverlay : LF_Test
{
	static const ResourceName OVERLAY = "{F107146A610857C1}Configs/Tests/LF_Test_ReputationOverlay.conf";
	static const ResourceName OVERLAY_CONTRACTS = "{6B83D3E82929413B}Configs/Tests/LF_Test_ReputationContractsOverlay.conf";

	//! Records a missing row as a failure; true when `row` exists.
	protected bool Has(LF_TestContext ctx, Managed row, string message)
	{
		ctx.NotNull(row, message);
		return row != null;
	}

	override string GetName()
	{
		return "reputation/pack-overlay";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_ConfigService.Reset();
		LF_ReputationConfig.Reload();
		LF_ReputationContractsConfig.Reload();

		// No pack: the core files alone.
		LF_ReputationConfig core = LF_ReputationConfig.Get();
		if (!Has(ctx, core, "the core reputation config loads"))
			return;

		ctx.True(core.FindAward("job_completed") && core.FindAward("job_completed").m_iPoints == 5, "without a pack, job_completed awards the core 5");
		ctx.True(core.FindAward("test_pack_action") == null, "without a pack, the pack's award is not there");
		ctx.True(core.FindAward("flagged_logout") != null, "without a pack, flagged_logout is still an award");
		ctx.True(LF_ReputationContracts.FindDef("test_pack_contract") == null, "without a pack, the pack's contract is not there");

		LF_MissionHeader header = new LF_MissionHeader();
		header.m_aLF_ConfigOverlays = {};
		header.m_aLF_ConfigOverlays.Insert(OVERLAY);
		header.m_aLF_ConfigOverlays.Insert(OVERLAY_CONTRACTS);
		LF_MissionConfigPackSource source = new LF_MissionConfigPackSource();
		source.SetHeader(header);
		if (!RedProof())
			LF_ConfigService.SetPackSource(source);

		LF_ReputationConfig.Reload();
		LF_ReputationContractsConfig.Reload();
		LF_ReputationConfig config = LF_ReputationConfig.Get();
		if (!Has(ctx, config, "the layered reputation config loads"))
			return;

		// Factions: patch the words, keep the ladder, add one.
		LF_ReputationFactionDef town = config.FindFaction("TOWN");
		if (Has(ctx, town, "TOWN is still a faction"))
		{
			ctx.EqualStr("#Test-Pack-Town", town.m_sLabel, "patch: the pack's faction label replaces the core one");
			ctx.EqualStr("#Test-Pack-T3", town.m_aTierLabels[3], "patch: the pack's tier words replace the core words");
			ctx.Equal(6, town.m_aTierThresholds.Count(), "patch: a field the pack leaves out (the ladder) stays core");
			ctx.Equal(60, town.m_iDailyCap, "patch: the daily cap stays core");
		}

		LF_ReputationFactionDef added = config.FindFaction("TEST_PACK");
		if (Has(ctx, added, "add: the pack's faction exists"))
			ctx.Equal(3, added.m_aTierThresholds.Count(), "add: the new faction has its ladder");

		// Awards: patch, add, remove.
		LF_ReputationAwardDef award = config.FindAward("job_completed");
		if (Has(ctx, award, "job_completed is still an award"))
		{
			ctx.Equal(9, award.m_iPoints, "patch: the pack's points replace the core 5");
			ctx.EqualStr("TOWN", award.m_sFaction, "patch: the faction stays core");
			ctx.EqualFloat(60, award.m_fCooldownSeconds, 0.01, "patch: the cooldown stays core");
		}

		LF_ReputationAwardDef packAward = config.FindAward("test_pack_action");
		if (Has(ctx, packAward, "add: the pack's award exists"))
		{
			ctx.Equal(4, packAward.m_iPoints, "add: the new award has its points");
			ctx.EqualStr("TEST_PACK", packAward.m_sFaction, "add: the new award feeds the pack's faction");
		}

		ctx.True(config.FindAward("flagged_logout") == null, "remove: the pack takes flagged_logout out");

		// Gates: patch the tier, add a contract gate.
		LF_ReputationGateDef taxi = LF_Reputation.FindGate("job:TAXI");
		if (Has(ctx, taxi, "job:TAXI is still a gate"))
		{
			ctx.Equal(1, taxi.m_iMinTier, "patch: the pack raises the gate to tier 1");
			ctx.EqualStr("TOWN", taxi.m_sFaction, "patch: the gate's faction stays core");
		}

		LF_ReputationGateDef contractGate = LF_Reputation.FindGate("contract:test_pack");
		if (Has(ctx, contractGate, "add: the pack's contract gate exists"))
			ctx.Equal(2, contractGate.m_iMinTier, "add: the contract gate has its tier");

		// Pay: patch the per-tier percents.
		LF_ReputationPayDef pay = LF_ReputationPay.FindDef("TOWN");
		if (Has(ctx, pay, "the town pay row is still there"))
		{
			ctx.Equal(6, pay.PercentFor(1), "patch: tier 1 pays the pack's 6%");
			ctx.Equal(11, pay.PercentFor(2), "patch: tier 2 pays the pack's 11%");
		}

		// Contracts: patch the award, add a contract.
		LF_ReputationContractDef parcel = LF_ReputationContracts.FindDef("test_parcel");
		if (Has(ctx, parcel, "the core contract is still there"))
		{
			ctx.Equal(25, parcel.m_iAwardPoints, "patch: the pack's contract award replaces the core 10");
			ctx.Equal(15, parcel.m_iStakePoints, "patch: the stake stays core");
		}

		LF_ReputationContractDef packContract = LF_ReputationContracts.FindDef("test_pack_contract");
		if (Has(ctx, packContract, "add: the pack's contract exists"))
		{
			ctx.Equal(LF_EJobType.TAXI, packContract.Job(), "add: the contract wraps the pack's job");
			ctx.EqualStr("contract:test_pack", packContract.m_sStandingGate, "add: the contract names its gate");
			ctx.Equal(12, packContract.m_iAwardPoints, "add: the contract has its award");
		}

		LF_ConfigService.Reset();
		LF_ReputationConfig.Reload();
		LF_ReputationContractsConfig.Reload();
	}
}
