// red-proof: RED-OK residents/gossip-bench (observed 2026-11-23 with `tools\cli test --no-build --red LF_Test_GossipBench`: RedProof() skips every listening step, so each "overhears" assertion fails, e.g. "two rumors told across two hours (expected 2, got 0)")
//! residents/gossip-bench (#4462, slice of #4170, plan W5.3): a player who sits on the gossip bench with a
//! gossip within earshot for the listen time overhears one rumor through `LF_RumorPool.Tell` (its place is
//! heard of); no gossip near, standing up early, or an unknown place gives nothing; the bench tells one
//! rumor per game hour. The bench prefab carries the base Sit actions and the listening component.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_GossipBench : LF_Test
{
	protected static const ResourceName BENCH = "{9A0D37E96A665D27}Prefabs/Props/Residents/LF_GossipBench.et";
	protected static const vector SPOT = "100 0 80"; // inside the test annex (x/z 18..114), never on the town
	protected static const string CHARACTER = "gb-char";
	protected static const string OTHER = "gb-other";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "residents/gossip-bench";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_RumorDef Rumor(string id, string place, string giver)
	{
		LF_RumorDef def = LF_RumorDef.Make(id, place, "#LF_Test_Rumor_" + id, false);
		def.m_aGivers.Insert(giver);
		return def;
	}

	//------------------------------------------------------------------------------------------------
	//! Listens for `seconds` whole seconds from `stamp`; returns the rumor told, or null.
	protected LF_RumorDef Listen(LF_GossipBenchComponent bench, string who, bool sitting, bool gossip, int seconds, int stamp)
	{
		LF_RumorDef told;
		for (int i = 0; i < seconds; i++)
		{
			LF_RumorDef def;
			if (!RedProof())
				def = bench.Step(who, sitting, gossip, 1, stamp);

			if (def)
				told = def;
		}

		return told;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_MapKnown.ResetStatics();
		LF_RumorsConfig.ResetStatics();

		LF_RumorsConfig config = new LF_RumorsConfig();
		config.m_aCrimeGivers = {};
		config.m_aRumors = {};
		config.m_aRumors.Insert(Rumor("barn", "gb-barn", LF_GossipBenchRules.GIVER));
		config.m_aRumors.Insert(Rumor("shop_only", "gb-shop", "shop"));
		LF_RumorsConfig.UseForTest(config);

		ctx.True(LF_GossipBenchRules.IsGossipId("gossip_odile"), "a resident id starting gossip_ is a gossip");
		ctx.True(!LF_GossipBenchRules.IsGossipId("teller"), "another resident is not a gossip");
		ctx.True(!LF_GossipBenchRules.IsGossipId(""), "no id is not a gossip");

		vector at = SPOT;
		at[1] = GetGame().GetWorld().GetSurfaceY(at[0], at[2]);
		IEntity benchEntity = LF_Utils.SpawnEntityPrefab(BENCH, at);
		ctx.NotNull(benchEntity, "the gossip bench spawns");
		if (!benchEntity)
		{
			LF_MapKnown.ResetStatics();
			LF_RumorsConfig.ResetStatics();
			return;
		}

		LF_GossipBenchComponent bench = LF_GossipBenchComponent.Cast(benchEntity.FindComponent(LF_GossipBenchComponent));
		ctx.NotNull(bench, "the bench carries LF_GossipBenchComponent");
		ctx.True(benchEntity.FindComponent(ActionsManagerComponent) != null, "the bench keeps the base Sit actions (an action on the object, no menu)");
		if (!bench)
		{
			LF_TestCharacterFixture.Delete(benchEntity);
			LF_MapKnown.ResetStatics();
			LF_RumorsConfig.ResetStatics();
			return;
		}

		int listen = LF_GossipBenchRules.LISTEN_SECONDS;
		int stamp = 600;

		// Standing up early: sat 19 s, stood, sat again for 19 s: still nothing (the listen starts over).
		ctx.True(Listen(bench, CHARACTER, true, true, listen - 1, stamp) == null, "19 seconds of listening is not enough");
		ctx.True(Listen(bench, CHARACTER, false, true, 1, stamp) == null, "standing up gives nothing");
		ctx.True(Listen(bench, CHARACTER, true, true, listen - 1, stamp) == null, "the listen started over after standing up");
		ctx.True(LF_MapKnown.KnownOf(CHARACTER, "gb-barn") == LF_EMapPoiKnown.NONE, "standing up early marks no place");

		// No gossip near: a full listen gives nothing.
		ctx.True(Listen(bench, OTHER, true, false, listen + 5, stamp) == null, "no gossip near gives nothing however long they sit");
		ctx.True(LF_MapKnown.KnownOf(OTHER, "gb-barn") == LF_EMapPoiKnown.NONE, "no gossip near marks no place");

		// Sitting with a gossip near for the full time: one rumor, its place heard of; shop rumors are not a gossip's.
		LF_RumorDef told = Listen(bench, CHARACTER, true, true, 1, stamp);
		ctx.NotNull(told, "sitting the full listen with a gossip near overhears a rumor");
		if (told)
			ctx.True(told.m_sId == "barn", "the rumor is one a gossip may tell, not the shop's");

		ctx.True(LF_MapKnown.KnownOf(CHARACTER, "gb-barn") == LF_EMapPoiKnown.HEARD, "the rumor's place is heard of");
		ctx.True(LF_MapKnown.KnownOf(CHARACTER, "gb-shop") == LF_EMapPoiKnown.NONE, "a shop rumor was not overheard");
		ctx.Equal(1, bench.ToldCount(), "the bench told one rumor");

		// One per bench per game hour: someone else listening within the hour hears nothing.
		LF_MapKnown.ResetStatics();
		ctx.True(Listen(bench, OTHER, true, true, listen + 5, stamp + 30) == null, "the same hour tells no second rumor");
		ctx.Equal(1, bench.ToldCount(), "still one rumor told in the hour");

		// An hour later it tells again.
		LF_RumorDef again = Listen(bench, OTHER, true, true, listen + 5, stamp + 61);
		ctx.NotNull(again, "an hour later the bench tells another rumor");
		ctx.Equal(2, bench.ToldCount(), "two rumors told across two hours");

		// Everything told: a full listen gives nothing and does not spend the hour.
		ctx.True(Listen(bench, OTHER, true, true, listen + 5, stamp + 200) == null, "nothing new to hear gives nothing");

		LF_TestCharacterFixture.Delete(benchEntity);
		LF_MapKnown.ResetStatics();
		LF_RumorsConfig.ResetStatics();
	}
}
