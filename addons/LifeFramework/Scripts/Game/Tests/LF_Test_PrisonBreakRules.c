// red-proof: `tools\cli test --no-build --red LF_Test_PrisonBreakRules` (2026-11-19) skips every TryStart
// through RedProof(). Observed: RED PROOF OK, e.g. "the cooldown that is left (expected 540, got 0)" and
// "closing the door does not skip the cooldown (expected 'cooldown', got '')". The unhooked run passes.

//! Prison break rules (#3420): no member inside refuses; too few officers refuses; the cooldown
//! refuses; a good start opens a window of the configured length. Breaks ship off.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_PrisonBreakRules : LF_Test
{
	protected static const int T0 = 1800000000;

	override string GetName()
	{
		return "police/prison-break-rules";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_PrisonBreakRules.ResetStatics();
		string refusal;

		// The core file ships off, with the documented numbers.
		LF_PrisonBreakConfig shipped = LF_PrisonBreakConfig.Get();
		ctx.True(shipped != null, "the shipped config resolves");
		if (shipped)
		{
			ctx.False(shipped.m_bEnabled, "breaks ship off");
			ctx.Equal(3, shipped.MinOfficers(), "shipped officer floor");
			ctx.Equal(90, shipped.WindowSeconds(), "shipped window");
		}

		LF_PrisonBreakConfig config = LF_PrisonBreakConfig.CreateDefault();
		config.m_bEnabled = true;
		config.m_iMinOfficersOnDuty = 3;
		config.m_iCooldownSeconds = 600;
		config.m_iWindowSeconds = 45;
		LF_PrisonBreakConfig.UseForTest(config);

		bool started = true;
		if (!RedProof())
			started = LF_PrisonBreakRules.TryStart(null, true, true, 1, 5, T0, refusal);
		ctx.False(started, "a missing config refuses");

		LF_PrisonBreakConfig off = LF_PrisonBreakConfig.CreateDefault();
		ctx.EqualStr(LF_PrisonBreakRules.REFUSE_OFF, LF_PrisonBreakRules.Refuse(off, true, true, 1, 5, T0), "a config that is off refuses");

		ctx.EqualStr(LF_PrisonBreakRules.REFUSE_NOT_FREE, LF_PrisonBreakRules.Refuse(config, false, true, 1, 5, T0), "a starter who is not free refuses");
		ctx.EqualStr(LF_PrisonBreakRules.REFUSE_NOT_MEMBER, LF_PrisonBreakRules.Refuse(config, true, false, 1, 5, T0), "a starter with no organisation refuses");
		ctx.EqualStr(LF_PrisonBreakRules.REFUSE_NO_MEMBER_INSIDE, LF_PrisonBreakRules.Refuse(config, true, true, 0, 5, T0), "no member inside refuses");
		ctx.EqualStr(LF_PrisonBreakRules.REFUSE_TOO_FEW_OFFICERS, LF_PrisonBreakRules.Refuse(config, true, true, 1, 2, T0), "too few officers refuses");
		ctx.EqualStr("", LF_PrisonBreakRules.Refuse(config, true, true, 1, 3, T0), "exactly the officer floor is enough");
		ctx.False(LF_PrisonBreakRules.IsWindowOpen(T0), "refusals open no window");

		// A refused start changes nothing.
		started = true;
		if (!RedProof())
			started = LF_PrisonBreakRules.TryStart(config, true, true, 0, 5, T0, refusal);
		ctx.False(started, "no member inside: TryStart refuses");
		ctx.EqualStr(LF_PrisonBreakRules.REFUSE_NO_MEMBER_INSIDE, refusal, "TryStart names the reason");
		ctx.Equal(0, LF_PrisonBreakRules.CooldownSecondsLeft(config, T0), "a refusal spends no cooldown");

		// A good start opens a window of the configured length.
		started = false;
		if (!RedProof())
			started = LF_PrisonBreakRules.TryStart(config, true, true, 2, 4, T0, refusal);
		ctx.True(started, "a good start opens the door");
		ctx.EqualStr("", refusal, "a good start has no refusal");
		ctx.True(LF_PrisonBreakRules.IsWindowOpen(T0 + 1), "the window is open just after the start");
		ctx.Equal(45, LF_PrisonBreakRules.WindowSecondsLeft(T0), "the window is the configured length");
		ctx.True(LF_PrisonBreakRules.IsWindowOpen(T0 + 44), "still open one second before the end");
		ctx.False(LF_PrisonBreakRules.IsWindowOpen(T0 + 45), "shut at the configured length");

		// The island cooldown refuses a second start, then lapses.
		ctx.EqualStr(LF_PrisonBreakRules.REFUSE_WINDOW_OPEN, LF_PrisonBreakRules.Refuse(config, true, true, 2, 4, T0 + 10), "no second break while the door is open");
		ctx.EqualStr(LF_PrisonBreakRules.REFUSE_COOLDOWN, LF_PrisonBreakRules.Refuse(config, true, true, 2, 4, T0 + 60), "the cooldown refuses");
		ctx.Equal(540, LF_PrisonBreakRules.CooldownSecondsLeft(config, T0 + 60), "the cooldown that is left");
		ctx.EqualStr(LF_PrisonBreakRules.REFUSE_COOLDOWN, LF_PrisonBreakRules.Refuse(config, true, true, 2, 4, T0 + 599), "one second before the cooldown ends still refuses");
		ctx.EqualStr("", LF_PrisonBreakRules.Refuse(config, true, true, 2, 4, T0 + 600), "a spent cooldown allows the next break");

		// Closing the window early keeps the cooldown.
		LF_PrisonBreakRules.CloseWindow();
		ctx.False(LF_PrisonBreakRules.IsWindowOpen(T0 + 5), "a closed window is shut");
		ctx.EqualStr(LF_PrisonBreakRules.REFUSE_COOLDOWN, LF_PrisonBreakRules.Refuse(config, true, true, 2, 4, T0 + 5), "closing the door does not skip the cooldown");

		// Out-of-range config values are clamped.
		config.m_iWindowSeconds = 0;
		config.m_iMinOfficersOnDuty = 999;
		ctx.Equal(LF_PrisonBreakConfig.WINDOW_MIN, config.WindowSeconds(), "a window below the minimum is clamped up");
		ctx.Equal(LF_PrisonBreakConfig.OFFICERS_MAX, config.MinOfficers(), "an officer floor above the maximum is clamped down");

		LF_PrisonBreakRules.ResetStatics();
	}
}
