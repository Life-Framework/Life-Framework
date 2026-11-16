//------------------------------------------------------------------------------------------------
//! One event's saved state (#1951), the "worldevents" payload. The key is the event id and the phase
//! NAME (LF_EventPhase.m_sName), never a phase index, so a pack may add or reorder phases between
//! saves. Times are relative to the save, in seconds: the shell runs on world seconds since boot,
//! which restart from 0. The shape is frozen once it ships (validate-record-shapes).
class LF_EventShellRow
{
	static const int VERSION = 1;

	//! Row format, written first so a later format can read an old row.
	int m_iVersion;
	string m_sEventId;
	//! The running phase at the save, "" when idle or cooling down.
	string m_sPhaseName;
	//! Seconds the phase had run at the save (0 when idle).
	float m_fPhaseAge;
	//! Seconds of cooldown left at the save (0 when none).
	float m_fCooldownLeft;
	//! Seconds since the event last ended, negative when it never ended this run.
	float m_fEndedAgo;
	//! The event's int bag (module state: counts, ids), as parallel key and value lists.
	ref array<string> m_aIntKeys = {};
	ref array<int> m_aIntValues = {};
}

//------------------------------------------------------------------------------------------------
//! The int bag an event's modules keep between ticks (#1951). Saved with the event.
modded class LF_EventShellRun
{
	ref map<string, int> m_mInts = new map<string, int>();
}

//------------------------------------------------------------------------------------------------
//! Events survive a restart by phase name (#1951, chapter #1015). Server only.
//!
//! Save: one LF_EventShellRow per event the shell runs.
//! Load, per row, against the events this boot runs:
//!   - an event id this boot does not run (removed, off, refused) is skipped with a warning;
//!   - idle: the cooldown left carries over;
//!   - a phase that still exists: the event goes to cooldown (its full cooldown from the load);
//!     a scene is never resumed, its actors are gone with the restart;
//!   - a phase name the event no longer has: idle, no cooldown, with a warning.
//! The int bag is restored in every case but the skipped id. Load replaces state, so a second
//! apply of the same rows gives the same result (idempotent).
modded class LF_EventShell
{
	protected static int s_iLoadWarnings;

	//------------------------------------------------------------------------------------------------
	//! \return the value under `key` in the event's int bag, `fallback` when unset or unknown event.
	static int GetInt(string eventId, string key, int fallback = 0)
	{
		LF_EventShellRun run = FindRun(eventId);
		int value;
		if (run && run.m_mInts.Find(key, value))
			return value;
		return fallback;
	}

	//------------------------------------------------------------------------------------------------
	//! Sets `key` in the event's int bag. \return false for an event the shell does not run.
	static bool SetInt(string eventId, string key, int value)
	{
		LF_EventShellRun run = FindRun(eventId);
		if (!run || key.IsEmpty())
			return false;
		run.m_mInts.Set(key, value);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return seconds of cooldown left for `eventId` at `now`, 0 when none or unknown.
	static float CooldownLeftOf(string eventId, float now)
	{
		LF_EventShellRun run = FindRun(eventId);
		if (!run || run.m_State.m_fCooldownUntil <= now)
			return 0;
		return run.m_State.m_fCooldownUntil - now;
	}

	//------------------------------------------------------------------------------------------------
	//! Warnings the last ApplySaved logged (skipped ids, phase names that are gone). Tests read it.
	static int LoadWarningCount()
	{
		return s_iLoadWarnings;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: clears the warning count after their own loads.
	static void ResetLoadWarnings()
	{
		s_iLoadWarnings = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Server seconds on the shell's clock (TickNow's), 0 before a world.
	static float ClockNow()
	{
		if (!GetGame() || !GetGame().GetWorld())
			return 0;
		return GetGame().GetWorld().GetWorldTime() / 1000;
	}

	//------------------------------------------------------------------------------------------------
	//! The "worldevents" payload at server second `now`: one row per event the shell runs.
	static array<ref LF_EventShellRow> ExportSaved(float now)
	{
		array<ref LF_EventShellRow> rows = {};
		if (!s_aRuns)
			return rows;

		foreach (LF_EventShellRun run : s_aRuns)
		{
			if (!run || !run.m_Def || !run.m_State)
				continue;

			LF_EventShellRow row = new LF_EventShellRow();
			row.m_iVersion = LF_EventShellRow.VERSION;
			row.m_sEventId = run.m_Def.m_sEventId;
			row.m_sPhaseName = run.m_State.m_sPhaseName;
			if (!row.m_sPhaseName.IsEmpty())
				row.m_fPhaseAge = Math.Max(0, now - run.m_State.m_fPhaseStartedAt);
			row.m_fCooldownLeft = CooldownLeftOf(row.m_sEventId, now);
			row.m_fEndedAgo = -1;
			if (run.m_fLastEndedAt >= 0)
				row.m_fEndedAgo = Math.Max(0, now - run.m_fLastEndedAt);

			foreach (string key, int value : run.m_mInts)
			{
				row.m_aIntKeys.Insert(key);
				row.m_aIntValues.Insert(value);
			}
			rows.Insert(row);
		}
		return rows;
	}

	//------------------------------------------------------------------------------------------------
	//! Applies saved rows at server second `now` (see the class comment). Loads the config first when
	//! the shell has not yet (a load before game start); StartIfServer then keeps these runs.
	//! \return rows applied (skipped ids not counted).
	static int ApplySaved(array<ref LF_EventShellRow> rows, float now)
	{
		s_iLoadWarnings = 0;
		if (!rows || !Replication.IsServer())
			return 0;
		if (!s_aRuns)
			Use(LoadConfig());

		int applied;
		foreach (LF_EventShellRow row : rows)
		{
			if (!row || row.m_sEventId.IsEmpty())
			{
				LoadWarn("skipped a saved event row with no id");
				continue;
			}

			LF_EventShellRun run = FindRun(row.m_sEventId);
			if (!run)
			{
				LoadWarn(string.Format("saved event %1 is not run by this server: its state is dropped", row.m_sEventId));
				continue;
			}

			// A re-apply onto a live session never cuts a running scene short: the live state wins.
			if (LF_EventPhaseRules.IsRunning(run.m_State))
			{
				LF_Debug.Info(FEATURE, string.Format("event %1 is running in phase %2: its saved state is not applied", row.m_sEventId, run.m_State.m_sPhaseName));
				continue;
			}

			LF_EventRunState state = new LF_EventRunState();
			run.m_fLastRunAt = -1000000;
			run.m_fLastEndedAt = -1;
			if (row.m_fEndedAgo >= 0)
			{
				// The clock restarted at 0, so an end older than this boot reads as "at boot" (0), never
				// as negative, which LF_EventContext takes for "never ended". A gap condition waits
				// longer than it would have, never less.
				run.m_fLastEndedAt = Math.Max(0, now - row.m_fEndedAgo);
			}

			if (row.m_sPhaseName.IsEmpty())
			{
				if (row.m_fCooldownLeft > 0)
					state.m_fCooldownUntil = now + row.m_fCooldownLeft;
			}
			else if (run.m_Def.FindPhase(row.m_sPhaseName) >= 0)
			{
				// A hot phase is never resumed: the scene died with the restart. Cool down from here.
				state.m_fCooldownUntil = now + run.m_Def.m_fCooldownSeconds;
				run.m_fLastEndedAt = now;
				LF_Debug.Info(FEATURE, string.Format("event %1 was in phase %2 at the save: it cools down %3 s", row.m_sEventId, row.m_sPhaseName, run.m_Def.m_fCooldownSeconds));
			}
			else
			{
				LoadWarn(string.Format("event %1 was saved in phase %2, which it no longer has: it loads idle", row.m_sEventId, row.m_sPhaseName));
			}
			run.m_State = state;

			run.m_mInts = new map<string, int>();
			int pairs;
			if (row.m_aIntKeys && row.m_aIntValues)
				pairs = row.m_aIntKeys.Count();
			if (pairs > 0 && row.m_aIntValues.Count() < pairs)
				pairs = row.m_aIntValues.Count();
			for (int i = 0; i < pairs; i++)
			{
				run.m_mInts.Set(row.m_aIntKeys[i], row.m_aIntValues[i]);
			}
			applied++;
		}
		return applied;
	}

	//------------------------------------------------------------------------------------------------
	protected static void LoadWarn(string msg)
	{
		s_iLoadWarnings++;
		LF_Debug.Warn(FEATURE, msg);
	}
}
