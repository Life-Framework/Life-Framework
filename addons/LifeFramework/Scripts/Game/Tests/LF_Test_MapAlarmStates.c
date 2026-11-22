// red-proof: 2026-11-22, cli test --no-build --red LF_Test_MapAlarmStates,LF_Test_MapAlarmJip: RED-OK (RedProof() skips Advance: "robbed recently after the live window (expected 2, got 1)").
//------------------------------------------------------------------------------------------------
//! map/alarm-states: a robbery report rings a place IN_PROGRESS, LIVE_SECONDS later it is RECENT,
//! RECENT_SECONDS after that it is gone, and a new report restarts it. Only robbery offences ring.
//! Server side through LF_MapAlarms.Raise/Advance and the replicated text the game mode carries.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_MapAlarmStates : LF_Test
{
	override string GetName()
	{
		return "map/alarm-states";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_EMapAlarmState State(string kind, string id)
	{
		int raised;
		return LF_MapAlarms.StateOf(kind, id, raised);
	}

	//------------------------------------------------------------------------------------------------
	protected void Step(int now)
	{
		LF_MapAlarms.SetNowForTest(now);
		if (!RedProof())
			LF_MapAlarms.Advance(now);

		LF_GameModeRoleplay gameMode = LF_GameModeRoleplay.Cast(GetGame().GetGameMode());
		if (gameMode)
			gameMode.LF_SetMapAlarms(LF_MapAlarms.Snapshot());
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_MapAlarms.ResetStatics();
		int t0 = 1800000000;
		LF_MapAlarms.SetNowForTest(t0);

		ctx.True(LF_MapAlarmRules.IsRobbery(LF_WantedReports.OFFENCE_ROBBERY), "a till robbery rings");
		ctx.True(LF_MapAlarmRules.IsRobbery(LF_WantedReports.OFFENCE_BREAK_IN), "a vault break-in rings");
		ctx.False(LF_MapAlarmRules.IsRobbery(LF_WantedReports.OFFENCE_CARJACKING), "a carjacking rings no shop");
		ctx.False(LF_MapAlarmRules.IsRobbery(LF_WantedReports.OFFENCE_SHOPLIFTING), "shoplifting rings no robbery");

		LF_OnServiceCall carjack = LF_OnServiceCall.Create("u1", "police", "", LF_WantedReports.OFFENCE_CARJACKING, "radio", "100 0 100", null);
		ctx.EqualStr("", LF_MapAlarms.OnCall(carjack), "a carjacking call rings nothing");
		LF_OnServiceCall nowhere = LF_OnServiceCall.Create("u1", "police", "", LF_WantedReports.OFFENCE_ROBBERY, "radio", vector.Zero, null);
		ctx.EqualStr("", LF_MapAlarms.OnCall(nowhere), "a robbery with no site rings nothing");

		ctx.True(LF_GameModeRoleplay.Cast(GetGame().GetGameMode()) != null, "the roleplay game mode carries the alarms");
		LF_MapAlarms.Raise(LF_MapAlarmRules.Key("FuelTill", "test.fuel"), t0);
		ctx.Equal(LF_EMapAlarmState.IN_PROGRESS, State("FuelTill", "test.fuel"), "the report rings the fuel till in progress");
		ctx.Equal(LF_EMapAlarmState.NONE, State("StoreCounter", "test.shop"), "another place stays quiet");

		Step(t0 + LF_MapAlarmRules.LIVE_SECONDS - 1);
		ctx.Equal(LF_EMapAlarmState.IN_PROGRESS, State("FuelTill", "test.fuel"), "still in progress just before the window ends");

		Step(t0 + LF_MapAlarmRules.LIVE_SECONDS);
		ctx.Equal(LF_EMapAlarmState.RECENT, State("FuelTill", "test.fuel"), "robbed recently after the live window");

		int t1 = t0 + LF_MapAlarmRules.LIVE_SECONDS + 10;
		LF_MapAlarms.Raise(LF_MapAlarmRules.Key("FuelTill", "test.fuel"), t1);
		ctx.Equal(LF_EMapAlarmState.IN_PROGRESS, State("FuelTill", "test.fuel"), "a new report restarts the alarm");
		ctx.Equal(1, LF_MapAlarms.ClientCount(), "one row per place, never two");

		Step(t1 + LF_MapAlarmRules.LIVE_SECONDS + LF_MapAlarmRules.RECENT_SECONDS);
		ctx.Equal(LF_EMapAlarmState.NONE, State("FuelTill", "test.fuel"), "the alarm is gone after the cooldown");
		ctx.Equal(0, LF_MapAlarms.ClientCount(), "no rows left on the carrier");

		LF_MapAlarms.ClearAll();
		LF_MapAlarms.ResetStatics();
	}
}
