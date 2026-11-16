//------------------------------------------------------------------------------------------------
//! The scene operation (#2137): the operation module a pack's LF_EventScene conf becomes. Its first
//! run plans the scene against the free AI budget, spawns the walkers (LF_EventSceneSpawn) and starts
//! their walk (LF_EventSceneWalk); then it keeps the scene on its own one-second ticker until the walk
//! is over, waits the scene's despawn delay and removes the walkers once no player sees them. While
//! the scene is on stage the operation is Busy, so the phase that names it does not move on; the
//! phase's idle timeout still ends a scene that hangs. Server only.
class LF_EventSceneOperation : LF_EventOperation
{
	static const string FEATURE = "Events";
	static const int TICK_MS = 1000;			// lf-fixed: bookkeeping resolution; the scene conf sets the times
	static const float DESPAWN_RETRY_S = 5;	// lf-fixed: a walker in sight is tried again this often, not every tick

	string m_sModuleId;
	ref LF_EventScene m_Scene;
	//! Tests: the free AI budget to plan against (negative reads the AIWorld), whether the walk drives
	//! the AI and whether the operation and the walk put themselves on the call queue.
	int m_iFreeAiOverride = -1;
	bool m_bDriveAi = true;
	bool m_bTick = true;

	protected ref LF_EventSceneCrew m_Crew;
	//! The phase run that staged the scene (its start second): one scene per phase run.
	protected bool m_bStaged;
	protected float m_fStagedPhaseAt;
	protected float m_fSinceWalkOver;
	protected float m_fSinceDespawnTry;

	//------------------------------------------------------------------------------------------------
	void LF_EventSceneOperation(string moduleId, LF_EventScene scene)
	{
		m_sModuleId = moduleId;
		m_Scene = scene;
	}

	//------------------------------------------------------------------------------------------------
	//! The crew on stage, or null.
	LF_EventSceneCrew Crew()
	{
		return m_Crew;
	}

	//------------------------------------------------------------------------------------------------
	override bool Busy()
	{
		return m_Crew && LF_EventSceneSpawn.Running() == m_Crew;
	}

	//------------------------------------------------------------------------------------------------
	//! Puts the scene on stage once per phase run. \return true when it started this run; false while
	//! it is on stage, after it played in this phase run, or when the plan or the spawn refused it
	//! (both logged by the planner and the spawn; a refusal is tried again at the next cadence).
	override bool Run(string eventId, string phaseName, LF_EventContext ctx)
	{
		if (!Replication.IsServer() || m_Crew)
			return false;

		if (ctx && m_bStaged && ctx.m_fPhaseStartedAt == m_fStagedPhaseAt)
			return false;

		int free = m_iFreeAiOverride;
		if (free < 0)
			free = LF_EventSceneSpawn.FreeAiSlots();

		LF_EventScenePlan plan = LF_EventScenePlanner.Plan(m_Scene, free);
		if (!plan.m_bGo)
			return false;

		LF_EventSceneCrew crew = LF_EventSceneSpawn.Start(plan);
		if (!crew)
			return false;

		m_Crew = crew;
		m_bStaged = ctx != null;
		if (ctx)
			m_fStagedPhaseAt = ctx.m_fPhaseStartedAt;
		m_fSinceWalkOver = 0;
		m_fSinceDespawnTry = DESPAWN_RETRY_S;
		LF_Debug.Info(FEATURE, string.Format("event %1 phase %2: module %3 put scene %4 on stage (%5 walkers)", eventId, phaseName, m_sModuleId, crew.m_sSceneId, plan.m_aNpcs.Count()));

		if (!LF_EventSceneWalk.Begin(crew, m_bDriveAi, m_bTick))
			LF_Debug.Warn(FEATURE, string.Format("scene %1 is on stage but does not walk; it leaves after its despawn delay", crew.m_sSceneId));

		if (m_bTick)
			GetGame().GetCallqueue().CallLater(Tick, TICK_MS, true);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Call queue tick; comes off the queue when the scene is gone.
	void Tick()
	{
		if (Advance(TICK_MS / 1000.0))
			GetGame().GetCallqueue().Remove(Tick);
	}

	//------------------------------------------------------------------------------------------------
	//! Moves the scene's end along by `seconds`: once every walker is spawned and the walk is over (or
	//! never started), the despawn delay runs, then the walkers are removed when no player sees them.
	//! `eyes` (with `bodies`) stand in for the players as in LF_EventSceneSpawn.TryDespawn; null reads
	//! them. \return true when no scene is on stage any more.
	bool Advance(float seconds, array<vector> eyes = null, array<IEntity> bodies = null)
	{
		if (!m_Crew)
			return true;

		// Dropped by someone else (a world end, an admin): nothing left to end.
		if (LF_EventSceneSpawn.Running() != m_Crew)
		{
			LF_Debug.Info(FEATURE, string.Format("module %1: scene %2 is no longer running", m_sModuleId, m_Crew.m_sSceneId));
			m_Crew = null;
			return true;
		}

		if (!m_Crew.IsFullySpawned())
			return false;

		LF_EventSceneWalk walk = LF_EventSceneWalk.Active();
		bool ourWalk = walk && walk.m_Crew == m_Crew;
		if (ourWalk && !walk.IsDone())
			return false;

		m_fSinceWalkOver += seconds;
		if (m_fSinceWalkOver < m_Crew.m_Plan.m_fDespawnDelaySeconds)
			return false;

		m_fSinceDespawnTry += seconds;
		if (m_fSinceDespawnTry < DESPAWN_RETRY_S)
			return false;

		m_fSinceDespawnTry = 0;
		if (!LF_EventSceneSpawn.TryDespawn(m_Crew, eyes, bodies))
			return false;

		if (ourWalk)
			walk.Stop();

		m_Crew = null;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Takes the operation off the call queue (the loader replaces it, a test ends).
	void StopTicking()
	{
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(Tick);
	}
}

//------------------------------------------------------------------------------------------------
//! Registers the modules an event config lists (#2137): each LF_EventModuleRef names an id and a
//! .conf, and the .conf's root class decides the kind. LF_EventPrecondition (an hour window, players
//! online or nearby, a cooldown) registers as a condition, LF_EventAbort as an abort, LF_EventScene
//! as an LF_EventSceneOperation. A ref that does not load, has the wrong root or a taken id is logged
//! [LFDebug:Events] and skipped; the phase that names it then holds (LF_EventShell). The shell calls
//! RegisterFrom at game start with the core config and every pack overlay over it. Server only.
class LF_EventModuleLoader
{
	static const string FEATURE = "Events";

	protected static ref array<string> s_aLoadedIds;

	//------------------------------------------------------------------------------------------------
	//! Registers every module ref of `config`, first removing the ones an earlier call registered.
	//! \return How many registered.
	static int RegisterFrom(LF_EventsConfig config)
	{
		UnregisterLoaded();
		if (!config || !config.m_aModules)
			return 0;

		int registered;
		foreach (LF_EventModuleRef entry : config.m_aModules)
		{
			if (RegisterRef(entry))
				registered++;
		}

		LF_Debug.Info(FEATURE, string.Format("event modules: %1 of %2 registered from config", registered, config.m_aModules.Count()));
		return registered;
	}

	//------------------------------------------------------------------------------------------------
	//! Loads one ref's .conf and registers it. \return false (logged) when it is not usable.
	static bool RegisterRef(LF_EventModuleRef entry)
	{
		if (!entry || entry.m_sId.IsEmpty() || entry.m_Config.IsEmpty())
		{
			LF_Debug.Error(FEATURE, "event module ref refused: no id or no config");
			return false;
		}

		Resource resource = BaseContainerTools.LoadContainer(entry.m_Config);
		if (!resource || !resource.IsValid() || !resource.GetResource())
		{
			LF_Debug.Error(FEATURE, string.Format("event module %1: config %2 does not load; the phases that name it hold", entry.m_sId, entry.m_Config));
			return false;
		}

		Managed module = BaseContainerTools.CreateInstanceFromContainer(resource.GetResource().ToBaseContainer());
		return RegisterModule(entry.m_sId, module, entry.m_Config);
	}

	//------------------------------------------------------------------------------------------------
	//! Registers `module` under `id` by its class. `source` names it in the log.
	static bool RegisterModule(string id, Managed module, string source)
	{
		string kind;
		bool registered;
		LF_EventPrecondition condition = LF_EventPrecondition.Cast(module);
		LF_EventAbort abort = LF_EventAbort.Cast(module);
		LF_EventScene scene = LF_EventScene.Cast(module);
		if (condition)
		{
			kind = "condition";
			registered = LF_EventShell.RegisterCondition(id, condition);
		}
		else if (abort)
		{
			kind = "abort";
			registered = LF_EventShell.RegisterAbort(id, abort);
		}
		else if (scene)
		{
			kind = "scene";
			string reason = LF_EventScenePlanner.Validate(scene);
			if (!reason.IsEmpty())
			{
				LF_Debug.Error(FEATURE, string.Format("event module %1: scene in %2 refused: %3", id, source, reason));
				return false;
			}

			registered = LF_EventShell.RegisterOperation(id, new LF_EventSceneOperation(id, scene));
		}
		else
		{
			string root = "nothing";
			if (module)
				root = module.ClassName();
			LF_Debug.Error(FEATURE, string.Format("event module %1: %2 holds %3, not a condition, abort or scene", id, source, root));
			return false;
		}

		if (!registered)
			return false;

		if (!s_aLoadedIds)
			s_aLoadedIds = {};
		s_aLoadedIds.Insert(id);
		LF_Debug.Info(FEATURE, string.Format("event module %1 registered as a %2 from %3", id, kind, source));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Removes the modules this loader registered (a second RegisterFrom, a test's cleanup).
	static void UnregisterLoaded()
	{
		if (!s_aLoadedIds)
			return;

		foreach (string id : s_aLoadedIds)
		{
			LF_EventSceneOperation operation = LF_EventSceneOperation.Cast(LF_EventShell.FindOperation(id));
			if (operation)
				operation.StopTicking();
			LF_EventShell.Unregister(id);
		}

		s_aLoadedIds = null;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_aLoadedIds = null;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_EventModuleLoader's statics (Core/LF_StaticReset.c). The shell's own reset
//! drops the modules themselves.
class LF_EventModuleLoader_StaticResetHook : LF_StaticResetHook
{
	protected static bool s_bRegistered = LF_StaticReset.Register(new LF_EventModuleLoader_StaticResetHook());

	override string Owner()
	{
		return "LF_EventModuleLoader";
	}

	override void Apply()
	{
		LF_EventModuleLoader.ResetStatics();
	}
}
