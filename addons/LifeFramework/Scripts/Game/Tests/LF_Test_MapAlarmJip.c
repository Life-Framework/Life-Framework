// red-proof: 2026-11-22, cli test --no-build --red LF_Test_MapAlarmStates,LF_Test_MapAlarmJip: RED-OK (RedProof() skips decoding the snapshot, so the late joiner sees no rows).
//------------------------------------------------------------------------------------------------
//! map/alarm-jip: a late joiner decodes the carrier's text alone (no event history) into the same
//! states and alarm times; malformed rows are skipped; the hover line follows the state and age.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_MapAlarmJip : LF_Test
{
	override string GetName()
	{
		return "map/alarm-jip";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		int t0 = 1800000000;
		array<ref LF_MapAlarmRow> server = {};
		server.Insert(LF_MapAlarmRow.Make(LF_MapAlarmRules.Key("FuelTill", "sp.fuel"), LF_EMapAlarmState.IN_PROGRESS, t0));
		server.Insert(LF_MapAlarmRow.Make(LF_MapAlarmRules.Key("Bank", "sp.bank"), LF_EMapAlarmState.RECENT, t0 - 400));
		server.Insert(LF_MapAlarmRow.Make(LF_MapAlarmRules.Key("Bank", "gone"), LF_EMapAlarmState.NONE, t0));
		string text = LF_MapAlarmRules.Encode(server);

		// The late joiner: fresh statics, only the snapshot text.
		LF_MapAlarms.ResetStatics();
		if (!RedProof())
			LF_MapAlarms.OnReplicated(text + ";broken|row;x|9|1");

		ctx.Equal(2, LF_MapAlarms.ClientCount(), "the two live rows arrive, NONE and malformed rows do not");
		int raised;
		ctx.Equal(LF_EMapAlarmState.IN_PROGRESS, LF_MapAlarms.StateOf("FuelTill", "sp.fuel", raised), "the late joiner sees the fuel till robbed now");
		ctx.Equal(t0, raised, "with the time the alarm went off");
		ctx.Equal(LF_EMapAlarmState.RECENT, LF_MapAlarms.StateOf("Bank", "sp.bank", raised), "and the bank robbed recently");
		ctx.Equal(LF_EMapAlarmState.NONE, LF_MapAlarms.StateOf("Bank", "gone", raised), "a cleared alarm is not on the wire");

		string arg;
		ctx.EqualStr("#LF-MapAlarm_InProgressNow", LF_MapAlarmRules.CardKey(LF_EMapAlarmState.IN_PROGRESS, LF_MapAlarmRules.MinutesAgo(t0, t0 + 30), arg), "under a minute reads just raised");
		ctx.EqualStr("#LF-MapAlarm_InProgress", LF_MapAlarmRules.CardKey(LF_EMapAlarmState.IN_PROGRESS, LF_MapAlarmRules.MinutesAgo(t0, t0 + 75), arg), "past a minute reads minutes ago");
		ctx.EqualStr("1", arg, "one minute ago");
		ctx.Equal(0, LF_MapAlarmRules.MinutesAgo(t0, t0 - 20), "a client clock behind the server never reads negative");
		ctx.EqualStr("#LF-MapAlarm_Recent", LF_MapAlarmRules.CardKey(LF_EMapAlarmState.RECENT, 9, arg), "cooldown reads robbed recently");

		LF_MapAlarms.ResetStatics();
	}
}
