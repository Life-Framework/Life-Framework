//------------------------------------------------------------------------------------------------
//! Police events on the server bus (docs/architecture/events.md). Fields are plain values: uids and
//! ints, never IEntity or EntityID. Published on the server only, after the state change happened.
//! A new event of this feature is appended to this file, not to a central list.

//! A jail sentence started.
class LF_OnArrested : LF_Event
{
	string m_sPlayerUid;
	int m_iSentenceSeconds;
	int m_iWantedLevel;

	static LF_OnArrested Create(string playerUid, int seconds, int wanted)
	{
		LF_OnArrested e = new LF_OnArrested();
		e.m_sPlayerUid = playerUid;
		e.m_iSentenceSeconds = seconds;
		e.m_iWantedLevel = wanted;
		return e;
	}
}

//! A character's wanted level changed (a crime, a fine, a sentence, an admin, a load). The wanted
//! board's posters read it.
class LF_OnWantedChanged : LF_Event
{
	//! The character key the reports use (#331).
	string m_sPlayerUid;
	int m_iOldLevel;
	int m_iNewLevel;

	static LF_OnWantedChanged Create(string playerUid, int oldLevel, int newLevel)
	{
		LF_OnWantedChanged e = new LF_OnWantedChanged();
		e.m_sPlayerUid = playerUid;
		e.m_iOldLevel = oldLevel;
		e.m_iNewLevel = newLevel;
		return e;
	}
}

//! A suspect's open reports were marked identified: their name goes on the posters.
class LF_OnSuspectIdentified : LF_Event
{
	string m_sPlayerUid;
	//! How: "id check", "caught in the act", ...
	string m_sHow;
	int m_iReports;

	static LF_OnSuspectIdentified Create(string playerUid, string how, int reports)
	{
		LF_OnSuspectIdentified e = new LF_OnSuspectIdentified();
		e.m_sPlayerUid = playerUid;
		e.m_sHow = how;
		e.m_iReports = reports;
		return e;
	}
}

//! A player left while cuffed, escorted, downed, wanted or jailed (#691): what they were and what
//! the disconnect rules did. The incident log records it as kind "logout".
class LF_OnFlaggedLogout : LF_Event
{
	string m_sPlayerUid;
	//! LF_DisconnectState.Describe: "cuffed,wanted2".
	string m_sState;
	//! LF_DisconnectDecision.Describe: "jail min=300s, wanted+1, warrant kept".
	string m_sOutcome;
	vector m_vPosition;
	//! Sentence seconds or fine taken; 0 otherwise.
	int m_iAmount;

	static LF_OnFlaggedLogout Create(string playerUid, string state, string outcome, vector position, int amount)
	{
		LF_OnFlaggedLogout e = new LF_OnFlaggedLogout();
		e.m_sPlayerUid = playerUid;
		e.m_sState = state;
		e.m_sOutcome = outcome;
		e.m_vPosition = position;
		e.m_iAmount = amount;
		return e;
	}
}
