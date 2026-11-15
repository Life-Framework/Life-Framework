// red-proof: Observed 2026-11-15: before LF_OpsEvents.c and the stubs existed, `tools/cli test --no-build --only LF_Test_OpsEventsContract` failed (scripts do not compile); with them, green; `--red` printed `RED PROOF OK` (publishes skipped: the seconds, due and warning checks failed).

// tier: LOGIC
//! The Ops bus contract (#1344): the restart warning and the restart-due events reach a
//! subscriber with their published values, and the two M4 starter stubs the game mode calls run
//! without a VME and log their line. The toast, siren and close slices subscribe to these events.
class LF_Test_OpsEventsContract : LF_Test
{
	override string GetName()
	{
		return "ops/events";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_EventBus.ResetStatics();

		LF_TestOpsRecorder rec = new LF_TestOpsRecorder();
		LF_EventBus.Subscribe(LF_OnRestartWarning, rec);
		LF_EventBus.Subscribe(LF_OnRestartDue, rec);

		// The red-proof hook skips the publish: no subscriber is called and every check below fails.
		int warned = 0;
		int due = 0;
		if (!RedProof())
		{
			warned = LF_EventBus.Publish(LF_OnRestartWarning.Create(5, 300));
			due = LF_EventBus.Publish(LF_OnRestartDue.Create());
		}

		ctx.Equal(1, warned, "one subscriber handled the restart warning");
		ctx.Equal(1, due, "one subscriber handled the restart due event");
		ctx.Equal(1, rec.m_iWarnings, "the subscriber received exactly one LF_OnRestartWarning");
		ctx.Equal(5, rec.m_iMinutes, "the warning carried the published minutes left");
		ctx.Equal(300, rec.m_iSeconds, "the warning carried the seconds to the restart");
		ctx.Equal(1, rec.m_iDue, "the subscriber received exactly one LF_OnRestartDue");

		// A warning does not count as a due event: each class reaches the subscriber on its own.
		ctx.Equal(1, rec.m_iWarnings, "the due event did not count as a warning");
		LF_EventBus.ResetStatics();

		// The stubs the game mode calls beside LF_HostStop.StartIfRequested(): no VME, one line each.
		LF_RestartWarner.StartIfConfigured();
		ctx.True(LF_RestartWarner.GetLastLog().Contains("restart"), "LF_RestartWarner logged its stub line");
		LF_DayCycleApply.ApplyFromConfig();
		ctx.True(LF_DayCycleApply.GetLastLog().Contains("day cycle"), "LF_DayCycleApply logged its stub line");
	}
}

//------------------------------------------------------------------------------------------------
class LF_TestOpsRecorder : LF_EventSubscriber
{
	int m_iWarnings;
	int m_iMinutes;
	int m_iSeconds;
	int m_iDue;

	override string GetName()
	{
		return "OpsRecorder";
	}

	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnRestartWarning warning = LF_OnRestartWarning.Cast(busEvent);
		if (warning)
		{
			m_iWarnings++;
			m_iMinutes = warning.m_iMinutesLeft;
			m_iSeconds = warning.m_iSecondsToRestart;
			return true;
		}

		if (LF_OnRestartDue.Cast(busEvent))
		{
			m_iDue++;
			return true;
		}

		return false;
	}
}
