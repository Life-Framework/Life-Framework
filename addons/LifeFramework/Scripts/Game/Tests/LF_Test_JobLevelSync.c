// red-proof: observed 2026-11-12 with `tools\cli test --no-build --only LF_Test_JobLevelSync`: LF_JobLevels.AddXp
// without its LF_OnJobLevelChanged publish failed `- a quest's xp that raises the DELIVERY level publishes
// the change (expected 'test-joblevel-sync:DELIVERY:1>2;', got '')`. Restored; green.
//------------------------------------------------------------------------------------------------
//! Records LF_OnJobLevelChanged events for the test.
class LF_TestJobLevelRecorder : LF_EventSubscriber
{
	string m_sLog;

	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnJobLevelChanged changed = LF_OnJobLevelChanged.Cast(busEvent);
		if (changed)
			m_sLog += string.Format("%1:%2:%3>%4;", changed.m_sPlayerUid, changed.m_sJob, changed.m_iLevelBefore, changed.m_iLevel);

		return true;
	}
}

// tier: WORLD
//! #749 review: the replicated DELIVERY level must follow every job level change, not only a run.
//! A level rise from any grant (here a quest's reward XP routed to DELIVERY) publishes
//! LF_OnJobLevelChanged, LF_JobLevelSync is subscribed at game start, and PullCareerFor puts the
//! level on the body's replicated LF_PlayerJobComponent.
class LF_Test_JobLevelSync : LF_Test
{
	static const string UID = "test-joblevel-sync";

	override string GetName()
	{
		return "jobs/job-level-sync";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_JobsConfig.Reload();
		LF_JobLevels.Forget(UID);
		// The game start subscribes it; a bus test earlier in the suite may have reset the bus.
		ctx.True(LF_JobLevelSync.EnsureSubscribed(), "the level sync is subscribed on the server");
		ctx.True(LF_EventBus.SubscriberCount(LF_OnJobLevelChanged) > 0, "the bus holds the level sync");

		LF_TestJobLevelRecorder recorder = new LF_TestJobLevelRecorder();
		LF_EventBus.Subscribe(LF_OnJobLevelChanged, recorder);
		LF_JobLevels.AddXp(UID, LF_JobLevels.JOB_DELIVERY, 499, "quest test");
		ctx.EqualStr("", recorder.m_sLog, "xp below the next threshold publishes no level change");
		LF_JobLevels.AddXp(UID, LF_JobLevels.JOB_DELIVERY, 1, "quest test");
		ctx.EqualStr(UID + ":DELIVERY:1>2;", recorder.m_sLog, "a quest's xp that raises the DELIVERY level publishes the change");
		LF_JobLevels.AddXp(UID, LF_JobLevels.JOB_CIVILIAN, 100, "quest test");
		ctx.EqualStr(UID + ":DELIVERY:1>2;" + UID + ":CIVILIAN:1>2;", recorder.m_sLog, "every job's level change is published");
		LF_EventBus.Unsubscribe(LF_OnJobLevelChanged, recorder);

		// The body the sync refreshes takes the DELIVERY level onto its replicated component.
		IEntity body = LF_TestCharacterFixture.Spawn(ctx);
		if (body)
		{
			LF_PlayerJobComponent jobs = LF_Component<LF_PlayerJobComponent>.Find(body);
			ctx.NotNull(jobs, "the body has a job component");
			if (jobs)
			{
				jobs.PullCareerFor(UID);
				ctx.Equal(2, jobs.GetJobLevel(), "the body's replicated job level is the DELIVERY level");
			}

			LF_TestCharacterFixture.Delete(body);
		}

		ctx.Equal(0, LF_JobLevelSync.RefreshBodies(UID), "a character with no live player has no body to refresh");
		LF_JobLevels.Forget(UID);
	}
}
