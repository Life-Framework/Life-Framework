//------------------------------------------------------------------------------------------------
//! Records the medic calls the casualty generator publishes, with their positions.
class LF_CasualtyCallTestSink : LF_EventSubscriber
{
	ref array<ref LF_OnServiceCall> m_aCalls = {};

	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnServiceCall call = LF_OnServiceCall.Cast(busEvent);
		if (call && call.m_sCallerUid.StartsWith(LF_CasualtyCalls.CALLER_PREFIX))
			m_aCalls.Insert(call);
		return false;
	}
}

//------------------------------------------------------------------------------------------------
// red-proof: 2026-11-24, cli test --no-build --red LF_Test_CasualtyCall: RED-OK (RedProof() skips the medic tick, so "the tick
//   raises a casualty", "one casualty is open" and "one medic call is placed" fail). Green without it.
// red-proof: 2026-11-24 (#4852, #4853) the expiry, bleed-out and roll-now checks: RedProof() skips the medic tick, so the casualty
//   is null and every one of them is skipped or fails with it; on the old code (expiry = the 900 s board expiry, bleed-out 180 s,
//   no RollNow) "the casualty lasts 120 s" and "the body bleeds out when the call expires" fail.
//! medic/casualty-call (#4827): with a medic on duty a generator tick spawns one hurt resident near
//! a test place and places one medic call with that position, pinned to the medic board; a second
//! tick with one medic stays at the cap; after the board expiry the body is gone and the call is off
//! the board. With no medic on duty nothing spawns. The casualty's own expiry (m_iExpirySeconds, #4852) sets
//! both the call's end and the body's bleed-out; `RollNow` (#4853) raises one past the cap, filtered by kind,
//! and the harness names `Casualty` / `casualty:<n>` find the body.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_CasualtyCall : LF_Test
{
	protected static const string KIND = "test_casualty_site";
	protected static const string SUMMARY = "test casualty: fell off a ladder";
	protected static const int MEDIC_ID = 9481;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "medic/casualty-call";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_CasualtyCalls.ClearAll();
		LF_CasualtyCalls.ResetStatics();
		LF_CasualtyCallConfig config = new LF_CasualtyCallConfig();
		config.m_bEnabled = true;
		config.m_iIntervalSeconds = 1;
		config.m_iPerMedic = 1;
		config.m_iMaxOpen = 2;
		config.m_fSpotRadius = 3;
		config.m_iExpirySeconds = 120;
		config.m_sPrefab = LF_CasualtyCallConfig.DEFAULT_PREFAB;
		config.m_aRows = {};
		config.m_aRows.Insert(LF_CasualtyCallRow.Make(KIND, SUMMARY, 1));
		LF_CasualtyCallConfig.Override(config);
		LF_ServiceBoard.Install();

		IEntity place = LF_TestCharacterFixture.Spawn(ctx);
		if (place)
		{
			bool registered = LF_Places.Register(KIND, "test-1", place);
			ctx.True(registered, "the test place registers");
			if (registered)
				RunCalls(ctx, place);
			LF_Places.Unregister(KIND, "test-1");
		}

		LF_CasualtyCalls.ClearAll();
		LF_TestCharacterFixture.Delete(place);
		LF_CasualtyCalls.ResetStatics();
		LF_CasualtyCallConfig.ResetStatics();
		// The casualty was a real downing: leave the downed counters as later medic tests expect them.
		LF_Downed.ResetStatics();
		LF_CasualtyCalls.Start();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunCalls(LF_TestContext ctx, IEntity place)
	{
		LF_CasualtyCallTestSink sink = new LF_CasualtyCallTestSink();
		LF_EventBus.Subscribe(LF_OnServiceCall, sink);

		int now = System.GetTickCount();
		array<ref LF_ServiceCandidate> nobody = {};
		nobody.Insert(LF_ServiceCandidate.Make(MEDIC_ID, false, LF_EJobType.UNEMPLOYED));
		array<ref LF_ServiceCandidate> medic = {};
		medic.Insert(LF_ServiceCandidate.Make(MEDIC_ID, false, LF_EJobType.MEDIC));

		ctx.True(LF_CasualtyCalls.Tick(now, nobody) == null, "no medic on duty: no casualty");
		ctx.Equal(0, LF_CasualtyCalls.Open().Count(), "no medic on duty: nothing is open");
		ctx.Equal(0, sink.m_aCalls.Count(), "no medic on duty: no call");

		now += 2000;
		LF_Casualty casualty;
		if (!RedProof())
			casualty = LF_CasualtyCalls.Tick(now, medic);

		ctx.True(casualty != null, "a medic on duty: the tick raises a casualty");
		ctx.Equal(1, LF_CasualtyCalls.Open().Count(), "one casualty is open");
		ctx.Equal(1, sink.m_aCalls.Count(), "one medic call is placed");
		if (!casualty || sink.m_aCalls.IsEmpty())
		{
			LF_EventBus.Unsubscribe(LF_OnServiceCall, sink);
			return;
		}

		ctx.Equal(120000, casualty.m_iExpiresMs - now, "the casualty lasts 120 s, its own expiry and not the board's");
		IEntity body = LF_CasualtyCalls.Body(casualty);
		ctx.True(body != null, "the casualty body exists");
		SCR_ChimeraCharacter hurt = SCR_ChimeraCharacter.Cast(body);
		if (hurt)
			ctx.True(Math.AbsFloat(hurt.LF_GetBleedLeft() - 120) < 1, "the body bleeds out when the call expires");
		if (body)
		{
			ctx.True(vector.DistanceXZ(body.GetOrigin(), place.GetOrigin()) <= 4, "the casualty lies near the test place");
			ctx.True(RplComponent.Cast(body.FindComponent(RplComponent)) != null, "the casualty replicates (RplComponent)");
			ctx.True(LF_Downed.IsDowned(body), "the casualty is downed, so a medic can revive and carry them");
		}

		LF_OnServiceCall call = sink.m_aCalls[0];
		ctx.EqualStr("medic", call.m_sService, "the call goes to the medics");
		ctx.True(HasChannel(call.m_sChannels, "board"), "the call goes to the medic board");
		ctx.True(call.m_aReceiverIds.Find(MEDIC_ID) != -1, "the on-duty medic receives it");
		ctx.True(call.m_vPosition != vector.Zero && vector.DistanceXZ(call.m_vPosition, place.GetOrigin()) <= 4, "the call carries the casualty's position");
		ctx.Equal(1, BoardCount(now), "the call is on the medic board");

		now += 2000;
		ctx.True(LF_CasualtyCalls.Tick(now, medic) == null, "one medic: the cap holds a second casualty back");
		ctx.Equal(1, LF_CasualtyCalls.Open().Count(), "still one casualty open");

		int expired = casualty.m_iExpiresMs + 1;
		LF_CasualtyCalls.Tick(expired, nobody);
		ctx.Equal(0, LF_CasualtyCalls.Open().Count(), "after the board expiry no casualty is open");
		ctx.True(LF_CasualtyCalls.Body(casualty) == null, "after the board expiry the body is gone");
		ctx.Equal(0, BoardCount(expired), "after the board expiry the call is off the medic board");

		RunRollNow(ctx, medic, expired + 1000);

		LF_EventBus.Unsubscribe(LF_OnServiceCall, sink);
	}

	//------------------------------------------------------------------------------------------------
	//! #4853: a dev roll skips the interval and the cap, takes a kind filter, and the harness names find the body.
	protected void RunRollNow(LF_TestContext ctx, array<ref LF_ServiceCandidate> medic, int now)
	{
		LF_Casualty first = LF_CasualtyCalls.Tick(now, medic);
		ctx.True(first != null, "roll now: the normal tick raises a casualty first");
		ctx.True(LF_CasualtyCalls.RollNow(now + 1000, "no_such_kind", medic) == null, "roll now: a kind no row has rolls nothing");
		LF_Casualty second = LF_CasualtyCalls.RollNow(now + 1000, KIND, medic);
		ctx.True(second != null, "roll now: a second casualty past the one-per-medic cap");
		ctx.Equal(2, LF_CasualtyCalls.Open().Count(), "roll now: both are open");
		if (!first || !second)
			return;

		ctx.True(first.m_sCallerUid != second.m_sCallerUid, "roll now: each casualty has its own caller uid");
		ctx.True(LF_CasualtyHarness.Named("Casualty") == LF_CasualtyCalls.Body(second), "stand name Casualty is the newest casualty body");
		ctx.True(LF_CasualtyHarness.Named(first.m_sCallerUid) == LF_CasualtyCalls.Body(first), "stand name casualty:<n> is that casualty body");
		ctx.True(LF_CasualtyHarness.Named("Bakery") == null, "another name is not a casualty");
		ctx.True(LF_CasualtyHarness.Describe(second, now + 1000).Contains(second.m_sCallerUid), "the roll's reply names the casualty");
	}

	//------------------------------------------------------------------------------------------------
	protected bool HasChannel(string channels, string channel)
	{
		array<string> list = {};
		channels.Split(",", list, true);
		return list.Find(channel) != -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Medic board entries with the test summary.
	protected int BoardCount(int nowMs)
	{
		array<string> entries = {};
		LF_ServiceBoard.BuildEntries("medic", nowMs, 0, entries);
		int count;
		foreach (string entry : entries)
		{
			if (entry.Contains(SUMMARY))
				count++;
		}
		return count;
	}
}
