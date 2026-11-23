// red-proof: observed 2026-11-23 with `tools\cli test --no-build --red LF_Test_FirstShift`: RED PROOF OK, the first shift switched off failed
// `- the event carries the granted amount (expected 80, got 40)` and `- the daily cap cuts the doubled amount to 30 (expected 30, got 20)`.
//! First shift of the day (0.3.0 W5, N11): the first award of a game day in a trade pays double for
//! 30 minutes, a later award is normal, a second trade has its own first shift, and the next game day
//! (or the morning whistle) opens it again. LF_JobLevels.AddXp applies it before the daily cap and
//! publishes LF_OnJobXpAwarded with the amount actually granted.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_FirstShift : LF_Test
{
	static const string UID = "test-firstshift-worker";
	static const string OTHER = "test-firstshift-other";

	// Synthetic world times: day 1 starts at 86400 s, day 2 at 172800 s.
	static const float T0 = 100000;
	static const float T_29_MIN = 100000 + 1740;
	static const float T_31_MIN = 100000 + 1860;
	static const float T_NEXT_DAY = 172800 + 60;

	override string GetName()
	{
		return "level/first-shift";
	}

	//------------------------------------------------------------------------------------------------
	static LF_JobsConfig MakeConfig()
	{
		LF_JobsConfig config = new LF_JobsConfig();
		array<int> mining = {100, 300, 600};
		array<int> smelting = {100};
		array<int> capped = {100};
		config.m_aTracks = {};
		config.m_aTracks.Insert(LF_JobTrackDef.Make("MINING", mining));
		config.m_aTracks.Insert(LF_JobTrackDef.Make("SMELTING", smelting));
		config.m_aTracks.Insert(LF_JobTrackDef.Make("CAPPED", capped, 30));
		config.m_aXpAwards = {};
		config.m_aLicenceGates = {};
		config.m_iJobXpFirstLevel = 500;
		config.m_fJobXpLevelGrowth = 2;
		return config;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_JobsConfig.UseForTest(MakeConfig());
		LF_JobLevels.Forget(UID);
		LF_JobLevels.Forget(OTHER);
		// Red proof: with the first shift off nothing doubles and the checks below fail.
		LF_FirstShift.SetEnabled(!RedProof());
		LF_FirstShift.SetRequireLive(false);

		RunWindow(ctx);
		RunAddXp(ctx);

		LF_FirstShift.SetEnabled(true);
		LF_FirstShift.SetRequireLive(true);
		LF_JobLevels.Forget(UID);
		LF_JobLevels.Forget(OTHER);
		LF_JobsConfig.Reload();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunWindow(LF_TestContext ctx)
	{
		ctx.Equal(20, LF_FirstShift.Apply(UID, "MINING", 10, T0), "the first award of the day in MINING is doubled");
		ctx.True(LF_FirstShift.IsOpen(UID, "MINING", T0 + 1), "the window is open right after it");
		ctx.Equal(20, LF_FirstShift.Apply(UID, "MINING", 10, T_29_MIN), "an award 29 minutes in is still doubled");
		ctx.Equal(10, LF_FirstShift.Apply(UID, "MINING", 10, T_31_MIN), "an award 31 minutes in is the normal rate");
		ctx.False(LF_FirstShift.IsOpen(UID, "MINING", T_31_MIN), "the window is closed after 30 minutes");
		ctx.Equal(10, LF_FirstShift.Apply(UID, "MINING", 10, T_31_MIN + 600), "later shifts that day stay normal");

		ctx.Equal(20, LF_FirstShift.Apply(UID, "SMELTING", 10, T_31_MIN), "a second trade gets its own first shift");
		ctx.Equal(20, LF_FirstShift.Apply(OTHER, "MINING", 10, T_31_MIN), "another character has its own first shift");

		ctx.Equal(20, LF_FirstShift.Apply(UID, "MINING", 10, T_NEXT_DAY), "the next game day opens the first shift again");
		ctx.Equal(10, LF_FirstShift.Apply(UID, "MINING", 10, T_NEXT_DAY + 1900), "and it closes 30 minutes later again");

		// The morning whistle reopens every trade the same day.
		LF_FirstShift.ReopenAll("test whistle");
		ctx.Equal(20, LF_FirstShift.Apply(UID, "MINING", 10, T_NEXT_DAY + 2000), "the morning whistle reopens the first shift");

		ctx.Equal(0, LF_FirstShift.Apply(UID, "MINING", 0, T0), "a zero award stays zero and opens nothing");
		ctx.Equal(10, LF_FirstShift.Apply("", "MINING", 10, T0), "an empty player id is not doubled");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunAddXp(LF_TestContext ctx)
	{
		LF_JobLevels.Forget(UID);
		LF_FirstShift.ReopenAll("test reset");

		LF_FirstShift_Listener listener = new LF_FirstShift_Listener();
		LF_EventBus.Subscribe(LF_OnJobXpAwarded, listener);
		ctx.Equal(80, LF_JobLevels.AddXp(UID, "MINING", 40, "test"), "AddXp: the first MINING award is doubled");
		ctx.Equal(80, LF_JobLevels.XpOf(UID, "MINING"), "the doubled XP is what is stored");
		ctx.Equal(80, LF_JobLevels.AddXp(UID, "MINING", 40, "test"), "AddXp: still inside the 30 minutes");
		ctx.Equal(160, LF_JobLevels.XpOf(UID, "MINING"), "two doubled awards add up");
		ctx.Equal(2, listener.m_aSeen.Count(), "each grant published LF_OnJobXpAwarded");
		if (listener.m_aSeen.Count() > 0)
		{
			ctx.Equal(80, listener.m_aSeen[0].m_iAmount, "the event carries the granted amount");
			ctx.EqualStr("MINING", listener.m_aSeen[0].m_sJob, "the event carries the job");
			ctx.EqualStr(UID, listener.m_aSeen[0].m_sPlayerUid, "the event carries the uid");
			ctx.EqualStr("test", listener.m_aSeen[0].m_sReason, "the event carries the reason");
		}

		// The daily cap holds on the doubled amount: cap 30, a 20 XP award doubles to 40 and is cut to 30.
		ctx.Equal(30, LF_JobLevels.AddXp(UID, "CAPPED", 20, "test"), "the daily cap cuts the doubled amount to 30");
		ctx.Equal(0, LF_JobLevels.AddXp(UID, "CAPPED", 5, "test"), "and nothing more is granted today");

		LF_EventBus.Unsubscribe(LF_OnJobXpAwarded, listener);
	}
}

//------------------------------------------------------------------------------------------------
class LF_FirstShift_Listener : LF_EventSubscriber
{
	ref array<ref LF_OnJobXpAwarded> m_aSeen = {};

	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnJobXpAwarded e = LF_OnJobXpAwarded.Cast(busEvent);
		if (e)
			m_aSeen.Insert(e);

		return true;
	}
}
