//------------------------------------------------------------------------------------------------
//! What a tick of a running event should do.
enum LF_EEventPhaseDecision
{
	STAY,		//!< Nothing changes this tick.
	ADVANCE,	//!< Move to the next phase.
	COOLDOWN	//!< End the event into cooldown: finished, idle too long, or aborted.
}

//------------------------------------------------------------------------------------------------
//! Pure rules for an event's phases (#1948), no world and no statics, so the LOGIC tier tests
//! every branch. The shell (#757) feeds in the time and what its modules found: conditions are
//! AND-ed to advance, aborts are OR-ed and end the event, an idle timeout ends it too, and a
//! cooldown blocks a restart until it runs out. Server only; times are server seconds.
class LF_EventPhaseRules
{
	//------------------------------------------------------------------------------------------------
	//! \return "" when the definition is usable, else the first reason it is not. A bad definition
	//! disables that one event and nothing else (the shell logs the reason).
	static string Validate(LF_EventDef def)
	{
		if (!def)
			return "no definition";
		if (def.m_sEventId == "")
			return "empty event id";
		if (!def.m_aPhases || def.m_aPhases.Count() == 0)
			return "no phases";
		if (def.m_fCooldownSeconds < 0)
			return "negative cooldown";

		for (int i = 0; i < def.m_aPhases.Count(); i++)
		{
			LF_EventPhase phase = def.m_aPhases[i];
			if (!phase || phase.m_sName == "")
				return string.Format("phase %1 has no name", i);
			if (def.FindPhase(phase.m_sName) != i)
				return string.Format("duplicate phase name %1", phase.m_sName);
			if (phase.m_fCadenceSeconds < 0 || phase.m_fIdleTimeoutSeconds < 0)
				return string.Format("phase %1 has a negative time", phase.m_sName);
		}
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! \return true when every condition holds. No conditions is true: a phase with none advances
	//! on its first tick.
	static bool AllHold(notnull array<bool> conditions)
	{
		foreach (bool held : conditions)
		{
			if (!held)
				return false;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return true when any abort module fired. No aborts is false.
	static bool AnyHolds(notnull array<bool> aborts)
	{
		foreach (bool fired : aborts)
		{
			if (fired)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \return true when the phase's modules are due to run again. A cadence of 0 is always due.
	static bool CadenceDue(LF_EventPhase phase, float now, float lastRunAt)
	{
		if (!phase)
			return false;
		if (phase.m_fCadenceSeconds <= 0)
			return true;
		return now - lastRunAt >= phase.m_fCadenceSeconds;
	}

	//------------------------------------------------------------------------------------------------
	//! \return true when the phase has had no activity for its idle timeout. A timeout of 0 never fires.
	static bool IdleTimedOut(LF_EventPhase phase, float now, float lastActivityAt)
	{
		if (!phase || phase.m_fIdleTimeoutSeconds <= 0)
			return false;
		return now - lastActivityAt >= phase.m_fIdleTimeoutSeconds;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsRunning(LF_EventRunState state)
	{
		return state && state.m_sPhaseName != "";
	}

	//------------------------------------------------------------------------------------------------
	//! \return true while the event is cooling down after it ended.
	static bool InCooldown(LF_EventRunState state, float now)
	{
		return state && !IsRunning(state) && now < state.m_fCooldownUntil;
	}

	//------------------------------------------------------------------------------------------------
	//! \return true when the event may start: not running, and any cooldown has run out.
	static bool CanStart(LF_EventDef def, LF_EventRunState state, float now)
	{
		string reason = Validate(def);
		if (reason != "")
		{
			LF_Debug.Warn("Events", string.Format("event start refused: %1", reason));
			return false;
		}
		if (!state || IsRunning(state))
			return false;
		if (InCooldown(state, now))
		{
			LF_Debug.Info("Events", string.Format("event %1 start refused: cooling down for %2 s", def.m_sEventId, state.m_fCooldownUntil - now));
			return false;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Starts the event in its first phase. \return false (state untouched) when CanStart refuses.
	static bool Start(LF_EventDef def, LF_EventRunState state, float now)
	{
		if (!CanStart(def, state, now))
			return false;

		state.m_sPhaseName = def.m_aPhases[0].m_sName;
		state.m_fPhaseStartedAt = now;
		state.m_fLastActivityAt = now;
		state.m_fCooldownUntil = 0;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! What this tick should do for a running event. `conditionsHold` is the AND of the phase's
	//! condition modules and `abort` the OR of its abort modules. An unknown phase name (a save
	//! from a renamed phase) ends the event rather than guessing.
	static LF_EEventPhaseDecision Decide(LF_EventDef def, LF_EventRunState state, float now, bool conditionsHold, bool abort)
	{
		if (!IsRunning(state) || !def)
			return LF_EEventPhaseDecision.STAY;

		int index = def.FindPhase(state.m_sPhaseName);
		if (index < 0)
		{
			LF_Debug.Error("Events", string.Format("event %1 has no phase %2: ending it", def.m_sEventId, state.m_sPhaseName));
			return LF_EEventPhaseDecision.COOLDOWN;
		}
		if (abort)
			return LF_EEventPhaseDecision.COOLDOWN;
		if (IdleTimedOut(def.m_aPhases[index], now, state.m_fLastActivityAt))
			return LF_EEventPhaseDecision.COOLDOWN;
		if (!conditionsHold)
			return LF_EEventPhaseDecision.STAY;
		if (index + 1 >= def.m_aPhases.Count())
			return LF_EEventPhaseDecision.COOLDOWN;
		return LF_EEventPhaseDecision.ADVANCE;
	}

	//------------------------------------------------------------------------------------------------
	//! Applies a decision to the state. ADVANCE opens the next phase, COOLDOWN stops the event and
	//! starts the clock that blocks a restart. \return true when the state changed.
	static bool Apply(LF_EventDef def, LF_EventRunState state, LF_EEventPhaseDecision decision, float now)
	{
		if (!IsRunning(state) || !def || decision == LF_EEventPhaseDecision.STAY)
			return false;

		if (decision == LF_EEventPhaseDecision.ADVANCE)
		{
			int index = def.FindPhase(state.m_sPhaseName);
			if (index < 0 || index + 1 >= def.m_aPhases.Count())
				return false;
			state.m_sPhaseName = def.m_aPhases[index + 1].m_sName;
			state.m_fPhaseStartedAt = now;
			state.m_fLastActivityAt = now;
			return true;
		}

		state.m_sPhaseName = "";
		state.m_fCooldownUntil = now + def.m_fCooldownSeconds;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A module acted this tick: the idle clock restarts.
	static void NoteActivity(LF_EventRunState state, float now)
	{
		if (state)
			state.m_fLastActivityAt = now;
	}
}
