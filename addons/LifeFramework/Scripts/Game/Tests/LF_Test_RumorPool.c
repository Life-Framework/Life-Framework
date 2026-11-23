// red-proof: RED-OK residents/rumor-pool (observed 2026-11-23: fails with the hook on, 25 assertions; nothing is offered)
//! The rumor pool (#4231, slice of #4170, plan W5.1): a rumor is offered only when its place is
//! unknown to the character and its live conditions hold; a crime rumor only to a player the giver
//! counts as `friendly` or better (N32) and only from bar staff or the informant.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_RumorPool : LF_Test
{
	protected static const string CHARACTER = "rp-char";

	protected LF_RumorDef Rumor(string id, string place, bool crime, array<string> givers, array<string> conditions)
	{
		LF_RumorDef def = LF_RumorDef.Make(id, place, "#LF_Test_Rumor_" + id, crime);
		foreach (string giver : givers)
			def.m_aGivers.Insert(giver);

		foreach (string condition : conditions)
			def.m_aConditions.Insert(condition);

		return def;
	}

	protected bool Offered(array<ref LF_RumorDef> offers, string id)
	{
		foreach (LF_RumorDef def : offers)
		{
			if (def.m_sId == id)
				return true;
		}

		return false;
	}

	override string GetName()
	{
		return "residents/rumor-pool";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_MapKnown.ResetStatics();
		LF_RumorsConfig.ResetStatics();

		LF_RumorsConfig config = new LF_RumorsConfig();
		config.m_aCrimeGivers = {"bar", "informant"};
		config.m_aRumors = {};
		config.m_aRumors.Insert(Rumor("barn", "rp-barn", false, {}, {}));
		config.m_aRumors.Insert(Rumor("night_stall", "rp-stall", false, {"shop"}, {"night"}));
		config.m_aRumors.Insert(Rumor("quiet_day", "rp-quiet", false, {}, {"!storm"}));
		config.m_aRumors.Insert(Rumor("still", "rp-still", true, {}, {}));
		config.m_aRumors.Insert(Rumor("safe_job", "rp-safe", true, {"shop"}, {}));
		LF_RumorsConfig.UseForTest(config);

		LF_RumorState state = new LF_RumorState();

		array<ref LF_RumorDef> offers;
		// Red-proof hook: with --red nothing is offered, so every "is offered" assertion fails.
		if (!RedProof())
			offers = LF_RumorPool.Offers(CHARACTER, "shop", LF_ResidentMemory.TIER_NEUTRAL, state);
		else
			offers = {};

		ctx.True(Offered(offers, "barn"), "an unknown place with no conditions is offered");
		ctx.True(!Offered(offers, "night_stall"), "a rumor whose condition does not hold is not offered");
		ctx.True(Offered(offers, "quiet_day"), "a negated condition holds while the state lacks it");
		ctx.True(!Offered(offers, "still"), "a crime rumor is not given to a neutral player");

		state.m_aTrue.Insert("night");
		offers = LF_RumorPool.Offers(CHARACTER, "shop", LF_ResidentMemory.TIER_NEUTRAL, state);
		ctx.True(Offered(offers, "night_stall"), "the rumor appears once its condition holds");
		offers = LF_RumorPool.Offers(CHARACTER, "bar", LF_ResidentMemory.TIER_NEUTRAL, state);
		ctx.True(!Offered(offers, "night_stall"), "a rumor listing givers is not told by another giver");

		state.m_aTrue.Insert("storm");
		offers = LF_RumorPool.Offers(CHARACTER, "shop", LF_ResidentMemory.TIER_NEUTRAL, state);
		ctx.True(!Offered(offers, "quiet_day"), "a negated condition stops the rumor when the state has it");

		// Crime (N32): friendly or better, and only bar staff or the informant.
		offers = LF_RumorPool.Offers(CHARACTER, "bar", LF_ResidentMemory.TIER_FRIENDLY, state);
		ctx.True(Offered(offers, "still"), "bar staff tell a friendly player the crime rumor");
		offers = LF_RumorPool.Offers(CHARACTER, "informant", LF_ResidentMemory.TIER_REGULAR, state);
		ctx.True(Offered(offers, "still"), "the informant tells a regular the crime rumor");
		offers = LF_RumorPool.Offers(CHARACTER, "bar", LF_ResidentMemory.TIER_WARY, state);
		ctx.True(!Offered(offers, "still"), "a wary player gets no crime rumor");
		offers = LF_RumorPool.Offers(CHARACTER, "bar", LF_ResidentMemory.TIER_REFUSES, state);
		ctx.True(!Offered(offers, "still"), "a player the town refuses gets no crime rumor");
		offers = LF_RumorPool.Offers(CHARACTER, "shop", LF_ResidentMemory.TIER_REGULAR, state);
		ctx.True(!Offered(offers, "still"), "a shopkeeper never gives a crime rumor, even to a regular");
		ctx.True(!Offered(offers, "safe_job"), "a crime rumor that lists the shop as giver still needs a crime giver");

		// Place unknown: once heard of or visited, the rumor is gone; another character still gets it.
		offers = LF_RumorPool.Offers(CHARACTER, "bar", LF_ResidentMemory.TIER_FRIENDLY, state);
		ctx.True(Offered(offers, "barn") && Offered(offers, "still"), "both unknown places are offered before telling");
		ctx.True(LF_RumorPool.Tell(CHARACTER, LF_RumorPool.Find("barn")), "telling a rumor records the place as heard of");
		ctx.True(LF_MapKnown.KnownOf(CHARACTER, "rp-barn") == LF_EMapPoiKnown.HEARD, "the character now has heard of the barn");
		offers = LF_RumorPool.Offers(CHARACTER, "bar", LF_ResidentMemory.TIER_FRIENDLY, state);
		ctx.True(!Offered(offers, "barn"), "a place the character has heard of is not offered again");
		ctx.True(Offered(offers, "still"), "other rumors are untouched");
		LF_MapKnown.VisitServer(CHARACTER, "rp-still");
		offers = LF_RumorPool.Offers(CHARACTER, "bar", LF_ResidentMemory.TIER_FRIENDLY, state);
		ctx.True(!Offered(offers, "still"), "a place the character has visited is not offered");
		offers = LF_RumorPool.Offers("rp-other", "bar", LF_ResidentMemory.TIER_FRIENDLY, state);
		ctx.True(Offered(offers, "barn") && Offered(offers, "still"), "another character still gets both");

		// Pick is deterministic for a seed and stays inside the offers.
		ctx.True(LF_RumorPool.Pick(offers, 0) != null, "a pick from a non-empty offer list returns a rumor");
		ctx.True(LF_RumorPool.Pick(offers, 7) == offers.Get(7 % offers.Count()), "the seed picks by modulo");
		array<ref LF_RumorDef> none = {};
		ctx.True(LF_RumorPool.Pick(none, 3) == null, "nothing to offer picks nothing");

		LF_MapKnown.ResetStatics();
		LF_RumorsConfig.ResetStatics();
	}
}
