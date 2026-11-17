// red-proof: observed 2026-11-17 with `tools\cli test --no-build --red LF_Test_ReputationRules`: RED-OK reputation/rules (fails with the hook on, 32 assertions), first `the daily cap cuts a gain to what is left (expected 40, got 60)`. Green with `--only`.
// tier: LOGIC
//------------------------------------------------------------------------------------------------
//! Records LF_OnReputationChanged events for the test.
class LF_TestReputationRecorder : LF_EventSubscriber
{
	string m_sLog;

	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnReputationChanged changed = LF_OnReputationChanged.Cast(busEvent);
		if (changed)
			m_sLog += string.Format("%1:%2:%3>%4;", changed.m_sPlayerUid, changed.m_sFaction, changed.m_iTierBefore, changed.m_iTier);

		return true;
	}
}

//! Standing rules (#2696): points move by the config row, a cooldown and a daily cap hold back farming,
//! points clamp to the first and last threshold, the tier comes from the thresholds, an unknown
//! faction or action changes nothing, and LF_OnReputationChanged fires only when the tier changes.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_ReputationRules : LF_Test
{
	static const string UID = "test-reputation-rules";
	static const string OTHER = "test-reputation-other";

	override string GetName()
	{
		return "reputation/rules";
	}

	//------------------------------------------------------------------------------------------------
	//! TOWN: the shipped thresholds, a 60 a day cap. POLICE: the same with no cap. "run" gives TOWN 20
	//! every 30 s, "tip" gives POLICE 150 with no cooldown, "ghost" names a faction that does not exist.
	static LF_ReputationConfig MakeConfig()
	{
		LF_ReputationConfig config = new LF_ReputationConfig();
		array<int> thresholds = {-300, -100, 0, 100, 400, 1000};
		config.m_aFactions = {};
		config.m_aFactions.Insert(LF_ReputationFactionDef.Make("TOWN", thresholds, 60));
		config.m_aFactions.Insert(LF_ReputationFactionDef.Make("POLICE", thresholds, 0));
		config.m_aAwards = {};
		config.m_aAwards.Insert(LF_ReputationAwardDef.Make("run", "TOWN", 20, 30));
		config.m_aAwards.Insert(LF_ReputationAwardDef.Make("tip", "POLICE", 150));
		config.m_aAwards.Insert(LF_ReputationAwardDef.Make("ghost", "NO_SUCH_FACTION", 10));
		return config;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ReputationConfig.UseForTest(MakeConfig());
		LF_Reputation.Forget(UID);
		LF_Reputation.Forget(OTHER);

		RunTiers(ctx);
		// RedProof: skip the awards, so every assertion that needs a moved point fails (`--red`).
		if (!RedProof())
			RunAwards(ctx);
		RunCapAndClamp(ctx);
		RunEvents(ctx);

		LF_Reputation.Forget(UID);
		LF_Reputation.Forget(OTHER);
		LF_ReputationConfig.Reload();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunTiers(LF_TestContext ctx)
	{
		LF_ReputationFactionDef town = LF_Reputation.FactionDef("TOWN");
		ctx.NotNull(town, "the test config has the TOWN faction");
		if (!town)
			return;

		ctx.Equal(-2, town.TierFor(-300), "the first threshold is tier -2");
		ctx.Equal(-2, town.TierFor(-1000), "below the first threshold is still tier -2");
		ctx.Equal(-1, town.TierFor(-100), "-100 is tier -1");
		ctx.Equal(-1, town.TierFor(-99), "-99 is still tier -1");
		ctx.Equal(0, town.TierFor(0), "0 is tier 0");
		ctx.Equal(1, town.TierFor(100), "100 is tier 1");
		ctx.Equal(2, town.TierFor(400), "400 is tier 2");
		ctx.Equal(3, town.TierFor(1000), "1000 is tier 3");
		ctx.Equal(3, town.TierFor(100000), "past the table is the top tier");
		ctx.Equal(-300, town.MinPoints(), "the lowest points are the first threshold");
		ctx.Equal(1000, town.MaxPoints(), "the highest points are the last threshold");

		ctx.Equal(0, LF_Reputation.Tier(UID, "TOWN"), "a new character is tier 0");
		ctx.Equal(0, LF_Reputation.Tier(UID, "NO_SUCH_FACTION"), "an unknown faction reads as tier 0");
		ctx.Equal(0, LF_Reputation.Points(UID, "TOWN"), "a new character has no points");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunAwards(LF_TestContext ctx)
	{
		ctx.Equal(20, LF_Reputation.Award(UID, "run"), "an award moves points by the config row");
		ctx.Equal(20, LF_Reputation.Points(UID, "TOWN"), "the character holds the 20 points");
		ctx.Equal(0, LF_Reputation.Award(UID, "run"), "a second award inside the cooldown does nothing");
		ctx.Equal(20, LF_Reputation.Points(UID, "TOWN"), "the cooldown left the points alone");
		ctx.Equal(20, LF_Reputation.Award(OTHER, "run"), "the cooldown is per character");

		ctx.Equal(0, LF_Reputation.Award(UID, "no_such_action"), "an unknown action awards nothing");
		ctx.Equal(0, LF_Reputation.Award(UID, "ghost"), "an action naming an unknown faction awards nothing");
		ctx.Equal(0, LF_Reputation.Award("", "tip"), "an empty character id awards nothing");
		ctx.Equal(0, LF_Reputation.Points(UID, "NO_SUCH_FACTION"), "the unknown faction holds nothing");
		ctx.Equal(0, LF_Reputation.Add(UID, "NO_SUCH_FACTION", 50, "test"), "adding to an unknown faction changes nothing");
		ctx.Equal(20, LF_Reputation.Points(UID, "TOWN"), "the rejected calls left TOWN alone");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunCapAndClamp(LF_TestContext ctx)
	{
		// TOWN cap 60 a day: 20 already earned, 100 more is cut to 40, then nothing.
		ctx.Equal(40, LF_Reputation.Add(UID, "TOWN", 100, "test"), "the daily cap cuts a gain to what is left");
		ctx.Equal(60, LF_Reputation.Points(UID, "TOWN"), "the character holds the capped 60");
		ctx.Equal(0, LF_Reputation.Add(UID, "TOWN", 10, "test"), "a gain past the cap earns nothing");
		ctx.Equal(-70, LF_Reputation.Add(UID, "TOWN", -70, "test"), "a loss is not stopped by the cap");
		ctx.Equal(-10, LF_Reputation.Points(UID, "TOWN"), "the loss took the points below 0");

		// POLICE has no cap: +150 twice clamps at the last threshold only at 1000.
		ctx.Equal(150, LF_Reputation.Award(UID, "tip"), "an uncapped faction takes the whole award");
		int guard = 0;
		while (LF_Reputation.Points(UID, "POLICE") < 1000 && guard < 20)
		{
			LF_Reputation.Award(UID, "tip");
			guard++;
		}

		ctx.Equal(1000, LF_Reputation.Points(UID, "POLICE"), "points clamp to the last threshold");
		ctx.Equal(3, LF_Reputation.Tier(UID, "POLICE"), "the clamped points are the top tier");
		ctx.Equal(0, LF_Reputation.Award(UID, "tip"), "a gain at the top changes nothing");

		ctx.Equal(-1300, LF_Reputation.Add(UID, "POLICE", -5000, "test"), "a big loss reports what was actually taken");
		ctx.Equal(-300, LF_Reputation.Points(UID, "POLICE"), "points clamp to the first threshold");
		ctx.Equal(-2, LF_Reputation.Tier(UID, "POLICE"), "the clamped points are the bottom tier");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunEvents(LF_TestContext ctx)
	{
		LF_Reputation.Forget(OTHER);
		LF_TestReputationRecorder recorder = new LF_TestReputationRecorder();
		LF_EventBus.Subscribe(LF_OnReputationChanged, recorder);

		// POLICE: 0 -> 90 stays tier 0; 90 -> 150 is tier 1; a loss back to 50 is tier 0 again.
		LF_Reputation.Add(OTHER, "POLICE", 90, "test");
		ctx.EqualStr("", recorder.m_sLog, "points inside a tier publish no change");
		LF_Reputation.Add(OTHER, "POLICE", 60, "test");
		ctx.EqualStr(OTHER + ":POLICE:0>1;", recorder.m_sLog, "crossing a threshold publishes the tier change");
		LF_Reputation.Add(OTHER, "POLICE", -100, "test");
		ctx.EqualStr(OTHER + ":POLICE:0>1;" + OTHER + ":POLICE:1>0;", recorder.m_sLog, "a loss across a threshold publishes it too");
		LF_Reputation.Add(OTHER, "NO_SUCH_FACTION", 500, "test");
		ctx.EqualStr(OTHER + ":POLICE:0>1;" + OTHER + ":POLICE:1>0;", recorder.m_sLog, "an unknown faction publishes nothing");

		LF_EventBus.Unsubscribe(LF_OnReputationChanged, recorder);
	}
}
