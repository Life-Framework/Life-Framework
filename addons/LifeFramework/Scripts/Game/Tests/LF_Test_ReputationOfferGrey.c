// red-proof: observed 2026-11-18 with `tools\cli test --no-build --red LF_Test_ReputationOfferGrey`: RED PROOF OK, first `the tier word is the gate's minimum tier word (expected '#LF-Reputation_Tier_1', got '')`. Green with `--only`: passed=4 failed=0 (with the gate, sync and client-offer tests).
//! The greyed job offer on a dedicated client (#2915): the client reads its own tiers from
//! LF_ReputationSync.ClientTier. Below a gate's tier the offer is refused with the faction's refusal key
//! and the tier word; at the tier it is open; with no tiers received yet it is open (the server still
//! refuses on use); a gate id with no row is open.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_ReputationOfferGrey : LF_Test
{
	override string GetName()
	{
		return "reputation/offer-grey";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_ReputationConfig MakeConfig()
	{
		array<int> thresholds = {-300, -100, 0, 100, 400, 1000};
		array<string> words = {"#LF-Reputation_Tier_Neg2", "#LF-Reputation_Tier_Neg1", "#LF-Reputation_Tier_0", "#LF-Reputation_Tier_1", "#LF-Reputation_Tier_2", "#LF-Reputation_Tier_3"};
		LF_ReputationConfig config = new LF_ReputationConfig();
		config.m_aFactions = {};
		array<string> ids = {"TOWN", "POLICE", "UNDERWORLD"};
		foreach (string id : ids)
		{
			LF_ReputationFactionDef faction = LF_ReputationFactionDef.Make(id, thresholds, 0);
			faction.m_aTierLabels = {};
			faction.m_aTierLabels.Copy(words);
			faction.m_bShown = (id != "UNDERWORLD");
			config.m_aFactions.Insert(faction);
		}

		config.m_aAwards = {};
		config.m_aGates = {};
		config.m_aGates.Insert(LF_ReputationGateDef.Make("job:TAXI", "TOWN", 1));
		config.m_aGates.Insert(LF_ReputationGateDef.Make("run:TAXI", "POLICE", 1));
		config.m_aGates.Insert(LF_ReputationGateDef.Make("gate:hidden", "UNDERWORLD", 1));
		return config;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ReputationConfig.UseForTest(MakeConfig());
		LF_ReputationSync.ForgetClientTiers();

		string word;

		// No tiers received yet: the offer shows, the server still refuses on use.
		ctx.EqualStr("", LF_ReputationOfferGrey.ClientRefusal("job:TAXI", word), "with no tiers received the offer is open");
		ctx.True(LF_ReputationOfferGrey.ClientPasses("job:TAXI"), "with no tiers received the offer can be performed");

		// Tiers received, below the gate.
		LF_ReputationSync.ReceiveOnClient(LF_ReputationSync.Pack(0, 0, 0));
		// RedProof: the client greying is unwired, so a refused tier still passes (`--red`).
		if (RedProof())
			LF_ReputationSync.ForgetClientTiers();

		ctx.False(LF_ReputationOfferGrey.ClientPasses("job:TAXI"), "town tier 0 is below the tier 1 sign-up gate");
		ctx.EqualStr("#LF-Reputation_Refuse_Town", LF_ReputationOfferGrey.ClientRefusal("job:TAXI", word), "the reason is the town's refusal key");
		ctx.EqualStr("#LF-Reputation_Tier_1", word, "the tier word is the gate's minimum tier word");
		ctx.True(LF_ReputationOfferGrey.ClientText("job:TAXI").Length() > 0, "the greyed reason has words");

		// The run gate reads the police, not the town.
		ctx.EqualStr("#LF-Reputation_Refuse_Police", LF_ReputationOfferGrey.ClientRefusal("run:TAXI", word), "the run start reads its own faction");

		// At the tier, and above it.
		LF_ReputationSync.ReceiveOnClient(LF_ReputationSync.Pack(1, 1, 0));
		ctx.True(LF_ReputationOfferGrey.ClientPasses("job:TAXI"), "exactly the gate's tier is open");
		ctx.EqualStr("", LF_ReputationOfferGrey.ClientRefusal("job:TAXI", word), "no reason at the tier");
		LF_ReputationSync.ReceiveOnClient(LF_ReputationSync.Pack(3, 3, 0));
		ctx.True(LF_ReputationOfferGrey.ClientPasses("run:TAXI"), "above the gate's tier is open");

		// A gate id with no row is open; an unshown faction gives no tier word.
		LF_ReputationSync.ReceiveOnClient(LF_ReputationSync.Pack(-2, -2, 0));
		ctx.True(LF_ReputationOfferGrey.ClientPasses("no:such:gate"), "a gate id with no row is open");
		ctx.True(LF_ReputationOfferGrey.ClientPasses(""), "an empty gate id is open");
		ctx.EqualStr("#LF-Reputation_Refuse_Underworld", LF_ReputationOfferGrey.ClientRefusal("gate:hidden", word), "the underworld refuses in its own words");
		ctx.EqualStr("", word, "an unshown faction gives no tier word");

		LF_ReputationSync.ForgetClientTiers();
		LF_ReputationConfig.Reload();
	}
}
