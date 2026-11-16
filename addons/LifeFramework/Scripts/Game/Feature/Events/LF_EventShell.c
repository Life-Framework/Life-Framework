//------------------------------------------------------------------------------------------------
//! An operation module (#1950): what an event does while a phase runs (spawn a scene, lift a zone,
//! post a notice). Later slices subclass it and register an instance under an id; a phase names
//! the id in m_aModules. Server only.
class LF_EventOperation : Managed
{
	//------------------------------------------------------------------------------------------------
	//! Runs once per cadence of the phase that names this module.
	//! \return true when the module acted this run (the phase's idle clock restarts).
	bool Run(string eventId, string phaseName, LF_EventContext ctx)
	{
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! True while what Run started is still under way (a scene on stage, #2137). The phase that names
	//! a busy operation does not advance: its conditions count as not holding.
	bool Busy()
	{
		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! One configured event and what the shell remembers about it between ticks.
class LF_EventShellRun : Managed
{
	ref LF_EventDef m_Def;
	ref LF_EventRunState m_State;
	float m_fLastRunAt = -1000000;	// lf-fixed: "never ran", so the first tick is always due
	float m_fLastEndedAt = -1;		// negative: never ended (LF_EventContext)
	int m_iLastNearby;				// players inside the event's place at the last measure (#2105)
}

//------------------------------------------------------------------------------------------------
//! The event shell (#1950, epic #757): ticks every configured event on the server once a second
//! (call queue, never per frame), runs a phase's modules in authored order at the phase's cadence,
//! moves the event through its phases with LF_EventPhaseRules, publishes LF_OnEventPhase on the bus
//! for each change and announces it to clients by RPC on the game mode. Every change logs
//! [LFDebug:Events].
//!
//! Module ids: a phase's m_aModules names condition, abort and operation modules registered here
//! (RegisterCondition / RegisterAbort / RegisterOperation). Conditions are AND-ed, aborts OR-ed,
//! operations run in order. An id nobody registered is logged once and counts as a condition that
//! does not hold, so a typo holds the phase instead of rushing it. A busy operation holds its phase
//! too. At game start LF_EventModuleLoader registers the modules the config lists (#2137).
//!
//! Start: an idle event that is not cooling down starts when its FIRST phase's conditions hold, so
//! the first phase's conditions are also the event's start gate. A running phase advances when its
//! own conditions hold (LF_EventPhaseRules.Decide).
//!
//! Players nearby (#2105): Measure counts the players within the radius of the event's place with
//! LF_EventPlace (the scene's event_target marker through LF_Places); an event with no place counts 0.
//!
//! Not yet: run state is not persisted (a restart starts every event idle).
//!
//! Off by default: an event runs only with m_bEnabled set (the core example ships off; a pack patch
//! turns an event on). Events come from LF_EventOverlay, the core file with the pack overlays over it.
class LF_EventShell
{
	static const string FEATURE = "Events";
	static const int TICK_MS = 1000;	// lf-fixed: once a second; phases set their own cadence on top

	protected static ref array<ref LF_EventShellRun> s_aRuns;
	protected static ref map<string, ref LF_EventPrecondition> s_mConditions;
	protected static ref map<string, ref LF_EventAbort> s_mAborts;
	protected static ref map<string, ref LF_EventOperation> s_mOperations;
	protected static ref set<string> s_aWarnedIds;
	protected static bool s_bRunning;
	protected static float s_fHourOverride = -1;
	protected static int s_iChanges;

	//------------------------------------------------------------------------------------------------
	//! Server, game start: loads the config and starts the ticker.
	static void StartIfServer()
	{
		if (!Replication.IsServer() || s_bRunning || !GetGame())
			return;

		if (!s_aRuns)
		{
			LF_EventsConfig config = LoadConfig();
			Use(config);
			LF_EventModuleLoader.RegisterFrom(config);
		}

		s_bRunning = true;
		GetGame().GetCallqueue().CallLater(TickNow, TICK_MS, true);
		LF_Debug.Info(FEATURE, string.Format("event shell started: %1 event(s)", RunCount()));
	}

	//------------------------------------------------------------------------------------------------
	static void Stop()
	{
		if (s_bRunning && GetGame())
			GetGame().GetCallqueue().Remove(TickNow);
		s_bRunning = false;
	}

	//------------------------------------------------------------------------------------------------
	//! \return the core event config with the pack overlays over it, or null (LF_EventOverlay logs why).
	static LF_EventsConfig LoadConfig()
	{
		return LF_EventOverlay.Get();
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces the events the shell runs (game start, tests). A definition that fails
	//! LF_EventPhaseRules.Validate, or repeats an id, is logged and skipped; one that is off is
	//! logged and not run; the rest still run.
	static void Use(LF_EventsConfig config)
	{
		s_aRuns = {};
		if (!config || !config.m_aEvents)
			return;

		set<string> seen = new set<string>();
		foreach (LF_EventDef def : config.m_aEvents)
		{
			string reason = LF_EventPhaseRules.Validate(def);
			if (reason == "" && seen.Contains(def.m_sEventId))
				reason = "duplicate event id";
			if (reason != "")
			{
				string id = "?";
				if (def)
					id = def.m_sEventId;
				LF_Debug.Error(FEATURE, string.Format("event %1 disabled: %2", id, reason));
				continue;
			}

			if (!def.m_bEnabled)
			{
				LF_Debug.Info(FEATURE, string.Format("event %1 is off (m_bEnabled 0): not run", def.m_sEventId));
				continue;
			}

			seen.Insert(def.m_sEventId);
			LF_EventShellRun run = new LF_EventShellRun();
			run.m_Def = def;
			run.m_State = new LF_EventRunState();
			s_aRuns.Insert(run);
		}
	}

	//------------------------------------------------------------------------------------------------
	static int RunCount()
	{
		if (!s_aRuns)
			return 0;
		return s_aRuns.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! \return the running phase of `eventId`, "" when idle, cooling down or unknown.
	static string PhaseOf(string eventId)
	{
		LF_EventShellRun run = FindRun(eventId);
		if (!run)
			return "";
		return run.m_State.m_sPhaseName;
	}

	//------------------------------------------------------------------------------------------------
	//! Phase changes since the world started. Tests read it.
	static int ChangeCount()
	{
		return s_iChanges;
	}

	//------------------------------------------------------------------------------------------------
	//! \return players inside the event's place at its last measure, 0 for an unknown event.
	static int NearbyOf(string eventId)
	{
		LF_EventShellRun run = FindRun(eventId);
		if (!run)
			return 0;
		return run.m_iLastNearby;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: measure this in-game hour instead of the world clock; a negative value clears it.
	static void ForceHour(float hour)
	{
		s_fHourOverride = hour;
	}

	//------------------------------------------------------------------------------------------------
	static bool RegisterCondition(string id, LF_EventPrecondition condition)
	{
		if (!CheckRegister(id, condition != null, "condition"))
			return false;
		Conditions().Set(id, condition);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static bool RegisterAbort(string id, LF_EventAbort abort)
	{
		if (!CheckRegister(id, abort != null, "abort"))
			return false;
		Aborts().Set(id, abort);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static bool RegisterOperation(string id, LF_EventOperation operation)
	{
		if (!CheckRegister(id, operation != null, "operation"))
			return false;
		Operations().Set(id, operation);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return the operation module registered under `id`, or null.
	static LF_EventOperation FindOperation(string id)
	{
		return Operations().Get(id);
	}

	//------------------------------------------------------------------------------------------------
	//! \return true when a condition, abort or operation is registered under `id`.
	static bool IsRegistered(string id)
	{
		return Conditions().Contains(id) || Aborts().Contains(id) || Operations().Contains(id);
	}

	//------------------------------------------------------------------------------------------------
	//! Removes a module of any kind by id (tests, a feature that shuts down). \return true when one was removed.
	static bool Unregister(string id)
	{
		bool removed = Conditions().Contains(id) || Aborts().Contains(id) || Operations().Contains(id);
		Conditions().Remove(id);
		Aborts().Remove(id);
		Operations().Remove(id);
		return removed;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool CheckRegister(string id, bool hasModule, string kind)
	{
		if (id == "" || !hasModule)
		{
			LF_Debug.Error(FEATURE, string.Format("%1 module refused: empty id or null module (%2)", kind, id));
			return false;
		}
		if (Conditions().Contains(id) || Aborts().Contains(id) || Operations().Contains(id))
		{
			LF_Debug.Error(FEATURE, string.Format("%1 module refused: id %2 is taken", kind, id));
			return false;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static void TickNow()
	{
		if (!GetGame() || !GetGame().GetWorld())
			return;
		Tick(GetGame().GetWorld().GetWorldTime() / 1000);
	}

	//------------------------------------------------------------------------------------------------
	//! One pass over every event at server second `now`. The ticker calls it; tests drive it.
	static void Tick(float now)
	{
		if (!Replication.IsServer() || !s_aRuns)
			return;

		foreach (LF_EventShellRun run : s_aRuns)
		{
			if (run)
				TickRun(run, now);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void TickRun(notnull LF_EventShellRun run, float now)
	{
		LF_EventDef def = run.m_Def;
		LF_EventRunState state = run.m_State;

		if (!LF_EventPhaseRules.IsRunning(state))
		{
			// Checked first so a cooling event does not log a refusal every tick (CanStart does).
			if (LF_EventPhaseRules.InCooldown(state, now))
				return;

			LF_EventPhase first = def.m_aPhases[0];
			if (!LF_EventPhaseRules.CadenceDue(first, now, run.m_fLastRunAt))
				return;
			run.m_fLastRunAt = now;

			if (!ConditionsHold(first, Measure(run, now)))
				return;
			if (!LF_EventPhaseRules.Start(def, state, now))
				return;

			Announce(def.m_sEventId, "", state.m_sPhaseName, "start", "");
			return;
		}

		int index = def.FindPhase(state.m_sPhaseName);
		LF_EventPhase phase;
		if (index >= 0)
			phase = def.m_aPhases[index];

		if (phase && !LF_EventPhaseRules.CadenceDue(phase, now, run.m_fLastRunAt))
			return;
		run.m_fLastRunAt = now;

		LF_EventContext ctx = Measure(run, now);
		bool hold;
		bool abort;
		if (phase)
		{
			if (RunOperations(def.m_sEventId, phase, ctx))
				LF_EventPhaseRules.NoteActivity(state, now);
			hold = ConditionsHold(phase, ctx);
			abort = AbortFires(phase, ctx);
		}

		LF_EEventPhaseDecision decision = LF_EventPhaseRules.Decide(def, state, now, hold, abort);
		if (decision == LF_EEventPhaseDecision.STAY)
			return;

		// Mirrors Decide's order, read before Apply moves the clocks.
		string reason;
		if (decision == LF_EEventPhaseDecision.COOLDOWN)
		{
			if (!phase)
				reason = "lost";
			else if (abort)
				reason = "abort";
			else if (LF_EventPhaseRules.IdleTimedOut(phase, now, state.m_fLastActivityAt))
				reason = "idle";
			else
				reason = "done";
		}

		string from = state.m_sPhaseName;
		if (!LF_EventPhaseRules.Apply(def, state, decision, now))
			return;

		if (decision == LF_EEventPhaseDecision.ADVANCE)
		{
			Announce(def.m_sEventId, from, state.m_sPhaseName, "advance", "");
			return;
		}

		run.m_fLastEndedAt = now;
		Announce(def.m_sEventId, from, "", "end", reason);
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_EventContext Measure(notnull LF_EventShellRun run, float now)
	{
		float hour = s_fHourOverride;
		if (hour < 0)
			hour = LF_Atmosphere.HourNow();

		int online;
		if (GetGame() && GetGame().GetPlayerManager())
			online = GetGame().GetPlayerManager().GetPlayerCount();

		// The running phase's radius, or the first phase's while the event is idle (its start gate).
		string phaseName = run.m_State.m_sPhaseName;
		if (phaseName.IsEmpty() && run.m_Def.m_aPhases.Count() > 0)
			phaseName = run.m_Def.m_aPhases[0].m_sName;
		run.m_iLastNearby = LF_EventPlace.CountNearby(run.m_Def.m_sEventId, phaseName);

		LF_EventContext ctx = LF_EventContext.Make(hour, online, run.m_iLastNearby, now);
		ctx.m_fLastEndedAt = run.m_fLastEndedAt;
		if (LF_EventPhaseRules.IsRunning(run.m_State))
			ctx.m_fPhaseStartedAt = run.m_State.m_fPhaseStartedAt;
		return ctx;
	}

	//------------------------------------------------------------------------------------------------
	//! AND of the phase's condition modules. An unknown id does not hold (logged once).
	protected static bool ConditionsHold(notnull LF_EventPhase phase, LF_EventContext ctx)
	{
		array<ref LF_EventPrecondition> conditions = {};
		if (phase.m_aModules)
		{
			foreach (string id : phase.m_aModules)
			{
				LF_EventPrecondition condition = Conditions().Get(id);
				if (condition)
				{
					conditions.Insert(condition);
					continue;
				}
				// A busy operation (a scene still on stage) holds its phase.
				LF_EventOperation operation = Operations().Get(id);
				if (operation && operation.Busy())
					return false;
				if (!Aborts().Contains(id) && !Operations().Contains(id))
				{
					WarnUnknown(id, phase.m_sName);
					return false;
				}
			}
		}
		return LF_EventPrecondition.AllHold(conditions, ctx);
	}

	//------------------------------------------------------------------------------------------------
	//! OR of the phase's abort modules.
	protected static bool AbortFires(notnull LF_EventPhase phase, LF_EventContext ctx)
	{
		array<ref LF_EventAbort> aborts = {};
		if (phase.m_aModules)
		{
			foreach (string id : phase.m_aModules)
			{
				LF_EventAbort abort = Aborts().Get(id);
				if (abort)
					aborts.Insert(abort);
			}
		}
		return LF_EventAbort.AnyFires(aborts, ctx);
	}

	//------------------------------------------------------------------------------------------------
	//! Runs the phase's operation modules in authored order. \return true when any acted.
	protected static bool RunOperations(string eventId, notnull LF_EventPhase phase, LF_EventContext ctx)
	{
		if (!phase.m_aModules)
			return false;

		bool acted;
		foreach (string id : phase.m_aModules)
		{
			LF_EventOperation operation = Operations().Get(id);
			if (operation && operation.Run(eventId, phase.m_sName, ctx))
				acted = true;
		}
		return acted;
	}

	//------------------------------------------------------------------------------------------------
	protected static void WarnUnknown(string id, string phaseName)
	{
		if (!s_aWarnedIds)
			s_aWarnedIds = new set<string>();
		if (s_aWarnedIds.Contains(id))
			return;
		s_aWarnedIds.Insert(id);
		LF_Debug.Error(FEATURE, string.Format("phase %1 names module %2 that nobody registered: the phase holds", phaseName, id));
	}

	//------------------------------------------------------------------------------------------------
	//! One phase change: the log line, the bus event, and the clients' announcement.
	protected static void Announce(string eventId, string fromPhase, string toPhase, string kind, string reason)
	{
		s_iChanges++;
		LF_Debug.Info(FEATURE, string.Format("event %1 %2: %3 -> %4 %5", eventId, kind, fromPhase, toPhase, reason));
		LF_EventBus.Publish(LF_OnEventPhase.Create(eventId, fromPhase, toPhase, kind, reason));

		if (!GetGame())
			return;
		LF_GameModeRoleplay gameMode = LF_GameModeRoleplay.Cast(GetGame().GetGameMode());
		if (gameMode)
			gameMode.LF_AnnounceEventPhase(eventId, toPhase, kind);
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_EventShellRun FindRun(string eventId)
	{
		if (!s_aRuns)
			return null;
		foreach (LF_EventShellRun run : s_aRuns)
		{
			if (run && run.m_Def.m_sEventId == eventId)
				return run;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected static map<string, ref LF_EventPrecondition> Conditions()
	{
		if (!s_mConditions)
			s_mConditions = new map<string, ref LF_EventPrecondition>();
		return s_mConditions;
	}

	//------------------------------------------------------------------------------------------------
	protected static map<string, ref LF_EventAbort> Aborts()
	{
		if (!s_mAborts)
			s_mAborts = new map<string, ref LF_EventAbort>();
		return s_mAborts;
	}

	//------------------------------------------------------------------------------------------------
	protected static map<string, ref LF_EventOperation> Operations()
	{
		if (!s_mOperations)
			s_mOperations = new map<string, ref LF_EventOperation>();
		return s_mOperations;
	}

	//------------------------------------------------------------------------------------------------
	//! World end (LF_StaticReset) and tests.
	static void ResetStatics()
	{
		Stop();
		s_aRuns = null;
		s_mConditions = null;
		s_mAborts = null;
		s_mOperations = null;
		s_aWarnedIds = null;
		s_fHourOverride = -1;
		s_iChanges = 0;
	}
}

//------------------------------------------------------------------------------------------------
//! What the clients last heard about each event's phase (#1950). Client side; a HUD or map layer
//! reads it later. Not JIP-filled: a late joiner hears the next change.
class LF_EventPhaseBoard
{
	protected static ref map<string, string> s_mPhases;

	//------------------------------------------------------------------------------------------------
	static void OnAnnounced(string eventId, string phase, string kind)
	{
		if (!s_mPhases)
			s_mPhases = new map<string, string>();
		s_mPhases.Set(eventId, phase);
		LF_Debug.Info(LF_EventShell.FEATURE, string.Format("client heard event %1 %2: %3", eventId, kind, phase));
	}

	//------------------------------------------------------------------------------------------------
	//! \return the phase last announced for `eventId`, "" for none or ended.
	static string PhaseOf(string eventId)
	{
		if (!s_mPhases)
			return "";
		return s_mPhases.Get(eventId);
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mPhases = null;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_EventShell's and LF_EventPhaseBoard's statics (Core/LF_StaticReset.c).
class LF_EventShell_StaticResetHook : LF_StaticResetHook
{
	protected static bool s_bRegistered = LF_StaticReset.Register(new LF_EventShell_StaticResetHook());

	override string Owner()
	{
		return "LF_EventShell";
	}

	override void Apply()
	{
		LF_EventShell.ResetStatics();
		LF_EventPhaseBoard.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
//! Starts the shell at game start and carries its announcement to the clients, without editing
//! LF_GameModeRoleplay.c (M4 rule).
modded class LF_GameModeRoleplay
{
	//------------------------------------------------------------------------------------------------
	protected override void OnGameStart()
	{
		super.OnGameStart();
		LF_EventShell.StartIfServer();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: tells every client an event changed phase. A listen-server host hears it directly.
	void LF_AnnounceEventPhase(string eventId, string phase, string kind)
	{
		if (!Replication.IsServer())
			return;

		Rpc(RpcDo_LF_EventPhase, eventId, phase, kind);
		if (RplSession.Mode() != RplMode.Dedicated)
			LF_EventPhaseBoard.OnAnnounced(eventId, phase, kind);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_LF_EventPhase(string eventId, string phase, string kind)
	{
		LF_EventPhaseBoard.OnAnnounced(eventId, phase, kind);
	}
}
