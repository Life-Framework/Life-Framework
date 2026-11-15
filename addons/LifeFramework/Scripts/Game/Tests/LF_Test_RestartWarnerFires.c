// red-proof: RED-OK 2026-11-15 `tools/cli test --no-build --red LF_Test_RestartWarnerFires`: with the ticks skipped no warning, due event or close (all counts 0); with Tick on, green.

// tier: LOGIC
//! O6T (#1348): the restart ticker reads the schedule from an injected clock and publishes
//! LF_OnRestartWarning once at each lead and LF_OnRestartDue once at the restart time. The flagship
//! schedule (06:00 and 18:00, leads 15, 5, 1) drives it second by second; the toast (#1265) and the
//! close (#1267) subscribe through StartWith, so the close request lands exactly at 18:00:00.
class LF_Test_RestartWarnerFires : LF_Test
{
	static const string TEST_MARKER = "$profile:lf-test-warner-restart";

	override string GetName()
	{
		return "ops/restart-warner-fires";
	}

	//------------------------------------------------------------------------------------------------
	protected static int At(int hour, int minute, int second)
	{
		return hour * 3600 + minute * 60 + second;
	}

	//------------------------------------------------------------------------------------------------
	//! Ticks once a second from `fromSeconds` to `toSeconds` inclusive, as the call queue would.
	protected void Drive(int fromSeconds, int toSeconds)
	{
		for (int now = fromSeconds; now <= toSeconds; now++)
		{
			// The red-proof hook skips the tick: nothing publishes, and every count below fails.
			if (!RedProof())
				LF_RestartWarner.Tick(now);
		}
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_EventBus.ResetStatics();
		LF_RestartWarner.ResetStatics();
		LF_RestartToast.ResetStatics();
		LF_RestartClose.ResetStatics();
		if (FileIO.FileExists(TEST_MARKER))
			FileIO.DeleteFile(TEST_MARKER);

		LF_RestartConfig config = LF_RestartConfig.CreateDefault();
		config.m_aTimes = {"06:00", "18:00"};

		LF_TestRestartWarnerRecorder rec = new LF_TestRestartWarnerRecorder();
		LF_EventBus.Subscribe(LF_OnRestartWarning, rec);
		LF_EventBus.Subscribe(LF_OnRestartDue, rec);

		LF_TestRestartWarnerToasts toasts = new LF_TestRestartWarnerToasts();
		LF_RestartToast.SetSender(toasts);
		LF_RestartClose.SetTestSeam(TEST_MARKER);

		ctx.True(LF_RestartWarner.StartWith(config), "a schedule with times starts the ticker");
		ctx.Equal(1, LF_EventBus.SubscriberCount(LF_OnRestartDue) - 1, "the start registered the close on LF_OnRestartDue");
		ctx.Equal(1, LF_EventBus.SubscriberCount(LF_OnRestartWarning) - 1, "the start subscribed the toast to LF_OnRestartWarning");

		// 17:44 to 17:46: the 15-minute lead (17:45:00) fires once, nothing else.
		Drive(At(17, 44, 0), At(17, 46, 0));
		ctx.Equal(1, rec.m_iWarnings, "one warning between 17:44 and 17:46");
		ctx.Equal(15, rec.m_iLastMinutes, "that warning is the 15-minute lead");
		ctx.Equal(900, rec.m_iLastSeconds, "it fired at 17:45:00, 900 s before the restart");
		ctx.Equal(1, toasts.m_iSends, "the toast went out for the 15-minute warning");
		ctx.True(toasts.m_sLastArg == "15", "the toast says 15 minutes");

		// 17:46:01 to 17:59:57: the 5- and 1-minute leads, each once.
		Drive(At(17, 46, 1), At(17, 59, 57));
		ctx.Equal(3, rec.m_iWarnings, "three warnings in all by 17:59:57");
		ctx.Equal(1, rec.m_iLastMinutes, "the last one is the 1-minute lead");
		ctx.Equal(3, toasts.m_iSends, "a toast for each warning");
		ctx.Equal(0, rec.m_iDue, "no due event before the restart time");
		ctx.Equal(0, LF_RestartClose.GetCloseRequests(), "no close before the restart time");

		// 17:59:58 to 18:00:02: exactly one due event, and the close fires on it.
		Drive(At(17, 59, 58), At(17, 59, 59));
		ctx.Equal(0, rec.m_iDue, "still no due event at 17:59:59");
		Drive(At(18, 0, 0), At(18, 0, 0));
		ctx.Equal(1, rec.m_iDue, "the due event published at 18:00:00");
		ctx.Equal(1, LF_RestartClose.GetCloseRequests(), "the close was requested at 18:00:00");
		ctx.True(LF_RestartClose.WasMarkerPresentAtClose(), "the restart marker was written before the close");
		Drive(At(18, 0, 1), At(18, 0, 2));
		ctx.Equal(1, rec.m_iDue, "one due event across 17:59:58 to 18:00:02");
		ctx.Equal(3, rec.m_iWarnings, "no stray warning after the restart");

		// A tick that skips the restart second (a slow frame) still publishes the due event once.
		LF_RestartWarner.ResetStatics();
		LF_RestartWarner.Use(config);
		if (!RedProof())
		{
			LF_RestartWarner.Tick(At(5, 59, 59));
			LF_RestartWarner.Tick(At(6, 0, 1));
			LF_RestartWarner.Tick(At(6, 0, 2));
		}
		ctx.Equal(2, rec.m_iDue, "a skipped 06:00:00 second still gives one due event");

		// No times: the ticker does not start.
		LF_RestartWarner.ResetStatics();
		ctx.False(LF_RestartWarner.StartWith(LF_RestartConfig.CreateDefault()), "an empty schedule starts nothing");
		ctx.True(LF_RestartWarner.GetLastLog().Contains("restart"), "the empty schedule logged its line");

		if (FileIO.FileExists(TEST_MARKER))
			FileIO.DeleteFile(TEST_MARKER);
		LF_RestartWarner.ResetStatics();
		LF_RestartToast.ResetStatics();
		LF_RestartClose.ResetStatics();
		LF_EventBus.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
class LF_TestRestartWarnerRecorder : LF_EventSubscriber
{
	int m_iWarnings;
	int m_iLastMinutes = -1;
	int m_iLastSeconds = -1;
	int m_iDue;

	override string GetName()
	{
		return "TestRestartWarnerRecorder";
	}

	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnRestartWarning warning = LF_OnRestartWarning.Cast(busEvent);
		if (warning)
		{
			m_iWarnings++;
			m_iLastMinutes = warning.m_iMinutesLeft;
			m_iLastSeconds = warning.m_iSecondsToRestart;
			return true;
		}

		if (LF_OnRestartDue.Cast(busEvent))
			m_iDue++;
		return true;
	}
}

//------------------------------------------------------------------------------------------------
class LF_TestRestartWarnerToasts : LF_RestartToastSender
{
	int m_iSends;
	string m_sLastArg;

	override void Send(string title, string message, string messageArg)
	{
		m_iSends++;
		m_sLastArg = messageArg;
	}
}
