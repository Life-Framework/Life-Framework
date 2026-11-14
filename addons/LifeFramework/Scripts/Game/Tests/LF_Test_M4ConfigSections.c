// red-proof: Observed 2026-11-15 before the classes existed: `tools\cli test --no-build --only LF_Test_M4ConfigSections` failed to compile ("Unknown type 'LF_RestartConfig'"), so the test could not pass. Then green on the classes.
// tier: LOGIC
//! The three M4 config sections (#1343), declared as shapes: `restarts`, `staff` and `dayCycle`.
//! A fixture owner file with all three reads back every value through LF_ConfigService; a config
//! without them gives no restarts, an empty webhook URL and the vanilla day cycle; a bad value keeps
//! the shipped one.
class LF_Test_M4ConfigSections : LF_Test
{
	static const string FIXTURE = "{\"schemaVersion\":1,\"restarts\":{\"times\":[\"04:00\",\"16:30\"],\"warningMinutes\":[30,10,2]},\"staff\":{\"webhookUrl\":\"https://example.invalid/hook\"},\"dayCycle\":{\"mode\":\"accelerated\",\"dayLengthMinutes\":120,\"nightSpeed\":2.5,\"startHour\":6.5,\"clockOffsetMinutes\":-30}}";
	static const string BAD = "{\"schemaVersion\":1,\"restarts\":{\"times\":[\"25:00\"],\"warningMinutes\":[0]},\"dayCycle\":{\"mode\":\"sideways\",\"dayLengthMinutes\":0,\"startHour\":30}}";

	override string GetName()
	{
		return "config/m4-sections";
	}

	protected void Reload()
	{
		LF_RestartConfig.Reload();
		LF_StaffWebhookConfig.Reload();
		LF_DayCycleConfig.Reload();
	}

	override void Run(LF_TestContext ctx)
	{
		// A config without the sections: no restarts, no webhook, vanilla.
		LF_ConfigService.Reset();
		LF_ConfigService.LoadOwnerString("{\"schemaVersion\":1}", "test:m4-none");
		Reload();
		LF_RestartConfig restarts = LF_RestartConfig.Get();
		ctx.Equal(0, restarts.m_aTimes.Count(), "no restarts without the section");
		ctx.Equal(3, restarts.m_aWarningMinutes.Count(), "three default warning leads");
		ctx.Equal(15, restarts.m_aWarningMinutes[0], "first default lead is 15");
		ctx.Equal(5, restarts.m_aWarningMinutes[1], "second default lead is 5");
		ctx.Equal(1, restarts.m_aWarningMinutes[2], "third default lead is 1");
		ctx.EqualStr("", LF_StaffWebhookConfig.Get().m_sWebhookUrl, "the webhook URL is empty by default");
		LF_DayCycleConfig day = LF_DayCycleConfig.Get();
		ctx.EqualStr(LF_DayCycleConfig.MODE_VANILLA, day.m_sMode, "the day cycle is vanilla by default");

		// All three sections in the fixture.
		LF_ConfigService.Reset();
		LF_ConfigService.LoadOwnerString(FIXTURE, "test:m4-full");
		Reload();
		restarts = LF_RestartConfig.Get();
		ctx.Equal(2, restarts.m_aTimes.Count(), "two restart times read");
		ctx.EqualStr("04:00", restarts.m_aTimes[0], "first restart time");
		ctx.EqualStr("16:30", restarts.m_aTimes[1], "second restart time");
		ctx.Equal(3, restarts.m_aWarningMinutes.Count(), "three warning leads read");
		ctx.Equal(30, restarts.m_aWarningMinutes[0], "first warning lead");
		ctx.Equal(10, restarts.m_aWarningMinutes[1], "second warning lead");
		ctx.Equal(2, restarts.m_aWarningMinutes[2], "third warning lead");
		ctx.EqualStr("https://example.invalid/hook", LF_StaffWebhookConfig.Get().m_sWebhookUrl, "the webhook URL is read");
		day = LF_DayCycleConfig.Get();
		ctx.EqualStr(LF_DayCycleConfig.MODE_ACCELERATED, day.m_sMode, "mode read");
		ctx.Equal(120, day.m_iDayLengthMinutes, "day length read");
		ctx.True(Math.AbsFloat(day.m_fNightSpeed - 2.5) < 0.001, "night speed read");
		ctx.True(Math.AbsFloat(day.m_fStartHour - 6.5) < 0.001, "start hour read");
		ctx.Equal(-30, day.m_iClockOffsetMinutes, "clock offset read");
		ctx.True(LF_ConfigService.IsKnownTopLevelKey(LF_RestartConfig.SECTION), "restarts is a known top-level key");
		ctx.True(LF_ConfigService.IsKnownTopLevelKey(LF_StaffWebhookConfig.SECTION), "staff is a known top-level key");
		ctx.True(LF_ConfigService.IsKnownTopLevelKey(LF_DayCycleConfig.SECTION), "dayCycle is a known top-level key");

		// Bad values keep the shipped ones.
		LF_ConfigService.Reset();
		LF_ConfigService.LoadOwnerString(BAD, "test:m4-bad");
		Reload();
		restarts = LF_RestartConfig.Get();
		ctx.Equal(0, restarts.m_aTimes.Count(), "a time that is not HH:MM refuses the list");
		ctx.Equal(15, restarts.m_aWarningMinutes[0], "a lead of 0 refuses the list");
		day = LF_DayCycleConfig.Get();
		ctx.EqualStr(LF_DayCycleConfig.MODE_VANILLA, day.m_sMode, "an unknown mode keeps vanilla");
		ctx.Equal(LF_DayCycleConfig.DEFAULT_DAY_MINUTES, day.m_iDayLengthMinutes, "a day length below 1 keeps the default");
		ctx.True(Math.AbsFloat(day.m_fStartHour - LF_DayCycleConfig.DEFAULT_START_HOUR) < 0.001, "a start hour of 30 keeps the default");

		LF_ConfigService.Reset();
		Reload();
	}
}
