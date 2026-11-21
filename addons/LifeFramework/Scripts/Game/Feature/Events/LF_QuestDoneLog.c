//------------------------------------------------------------------------------------------------
//! The tip-off signal behind the QUEST_DONE event condition (#3784): the server notes the world
//! second each quest id was last turned in (LF_QuestTrackerComponent.TurnInQuest). An event waits
//! on it: "a police report was filed, the raid comes a few minutes later". Server memory only and
//! never saved: a restart forgets the tip-off, so a raid never fires from a report of an earlier
//! session. World seconds, the same clock as LF_EventShell.Tick.
class LF_QuestDoneLog
{
	protected static ref map<string, float> s_mDoneAt;

	//------------------------------------------------------------------------------------------------
	//! Record that `questId` was turned in at server second `now`; the latest turn-in wins.
	static void Note(string questId, float now)
	{
		if (questId.IsEmpty())
			return;
		if (!s_mDoneAt)
			s_mDoneAt = new map<string, float>();
		s_mDoneAt.Set(questId, now);
		LF_Debug.Info("Events", string.Format("quest-done signal %1 at %2 s", questId, now));
	}

	//------------------------------------------------------------------------------------------------
	//! \return the server second the quest was last turned in this boot, -1 when it has not been.
	static float DoneAt(string questId)
	{
		float at;
		if (s_mDoneAt && s_mDoneAt.Find(questId, at))
			return at;
		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Copy every noted turn-in into a measurement.
	static void Fill(notnull LF_EventContext ctx)
	{
		if (!s_mDoneAt)
			return;
		foreach (string questId, float at : s_mDoneAt)
		{
			ctx.NoteQuestDone(questId, at);
		}
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mDoneAt = null;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_QuestDoneLog's statics (LF_StaticReset finds it by reflection).
class LF_QuestDoneLog_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_QuestDoneLog";
	}

	override void Apply()
	{
		LF_QuestDoneLog.ResetStatics();
	}
}
