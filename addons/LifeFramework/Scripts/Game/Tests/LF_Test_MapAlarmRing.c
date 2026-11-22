// red-proof: 2026-11-22, cli test --no-build --red LF_Test_MapAlarmRing,LF_Test_MapAlarmPlace: RED-OK (RedProof() blanks the in-progress places: "a robbery in progress wins over robbed recently (expected 1, got 2)").
//------------------------------------------------------------------------------------------------
//! map/alarm-ring: a badge over several places shows the strongest alarm (in progress over robbed
//! recently, the newest time), and the ring grows and fades through each beat.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_MapAlarmRing : LF_Test
{
	override string GetName()
	{
		return "map/alarm-ring";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<int> states = {LF_EMapAlarmState.NONE, LF_EMapAlarmState.RECENT, LF_EMapAlarmState.IN_PROGRESS, LF_EMapAlarmState.IN_PROGRESS};
		array<int> times = {0, 900, 100, 200};
		if (RedProof())
			states = {LF_EMapAlarmState.NONE, LF_EMapAlarmState.RECENT, LF_EMapAlarmState.NONE, LF_EMapAlarmState.NONE};

		int when;
		ctx.Equal(LF_EMapAlarmState.IN_PROGRESS, LF_MapAlarmRing.Strongest(states, times, when), "a robbery in progress wins over robbed recently");
		ctx.Equal(200, when, "the newest alarm time of that state");

		array<int> quiet = {LF_EMapAlarmState.NONE};
		array<int> zero = {0};
		ctx.Equal(LF_EMapAlarmState.NONE, LF_MapAlarmRing.Strongest(quiet, zero, when), "a quiet badge shows nothing");

		float rest = LF_MapAlarmRing.Radius(28, LF_MapAlarmRing.Phase(0));
		float mid = LF_MapAlarmRing.Radius(28, LF_MapAlarmRing.Phase(LF_MapAlarmRing.PULSE_SECONDS * 0.5));
		ctx.True(mid > rest, "the ring grows through the beat");
		ctx.EqualFloat(rest, LF_MapAlarmRing.Radius(28, LF_MapAlarmRing.Phase(LF_MapAlarmRing.PULSE_SECONDS * 3)), 0.01, "and starts again every beat");
		ctx.True(((LF_MapAlarmRing.RingColor(0) >> 24) & 0xFF) > ((LF_MapAlarmRing.RingColor(0.9) >> 24) & 0xFF), "it fades as it grows");
	}
}
