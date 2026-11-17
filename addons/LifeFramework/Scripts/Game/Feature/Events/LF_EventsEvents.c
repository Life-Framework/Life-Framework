//------------------------------------------------------------------------------------------------
//! Events (the scheduled-event shell, #757) on the server bus (docs/architecture/events.md).
//! Fields are plain values, never IEntity or EntityID. Published on the server only, after the
//! state change happened. A new event of this feature is appended to this file.

//! An event changed phase (#1950). m_sKind is "start" (idle into its first phase), "advance" (to
//! the next phase) or "end" (into cooldown; m_sToPhase is ""). m_sReason says why an end happened:
//! "done" (the last phase finished), "abort" (an abort module fired), "idle" (the phase timed
//! out) or "" for a start or an advance. One event per change, never per tick.
class LF_OnEventPhase : LF_Event
{
	string m_sEventId;
	string m_sFromPhase;
	string m_sToPhase;
	string m_sKind;
	string m_sReason;

	static LF_OnEventPhase Create(string eventId, string fromPhase, string toPhase, string kind, string reason)
	{
		LF_OnEventPhase e = new LF_OnEventPhase();
		e.m_sEventId = eventId;
		e.m_sFromPhase = fromPhase;
		e.m_sToPhase = toPhase;
		e.m_sKind = kind;
		e.m_sReason = reason;
		return e;
	}
}
