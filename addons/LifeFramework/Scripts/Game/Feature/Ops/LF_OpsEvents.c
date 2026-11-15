//------------------------------------------------------------------------------------------------
//! Ops events on the server bus (docs/architecture/events.md). Fields are plain values: ints,
//! never IEntity or EntityID. Published on the server only, after the state change happened.
//! A new event of this feature is appended to this file, not to a central list.

//! The scheduled restart is near: published by LF_RestartWarner at each configured warning step.
//! The toast (O6) and the siren (O7) subscribe; neither waits for the ticker.
class LF_OnRestartWarning : LF_Event
{
	//! Whole minutes left before the restart, as configured for this step.
	int m_iMinutesLeft;
	//! Seconds to the restart when the step fired; the exact figure a countdown starts from.
	int m_iSecondsToRestart;

	static LF_OnRestartWarning Create(int minutesLeft, int secondsToRestart)
	{
		LF_OnRestartWarning e = new LF_OnRestartWarning();
		e.m_iMinutesLeft = minutesLeft;
		e.m_iSecondsToRestart = secondsToRestart;
		return e;
	}
}

//! The restart time has come: published once by LF_RestartWarner, before the close (O8) asks the
//! engine for a normal shutdown, so the SHUTDOWN save still runs. No fields: the moment is the fact.
class LF_OnRestartDue : LF_Event
{
	static LF_OnRestartDue Create()
	{
		return new LF_OnRestartDue();
	}
}
