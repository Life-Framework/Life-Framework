// LF_ResidentWorkLoop.c - a generic worker carries, hammers or sweeps at their spot instead of standing still
// (#4230, #4382, epic #4170). Each work activity is a body pose that loops while the resident works: a custom
// loiter on the work graph (Anims/Workspaces/Work/LF_WorkPick.agr, the lockpick's graph, LF_WorkAnim) with that
// job's own vanilla clips in an anim set instance:
//   hammer         LF_WorkHammer.asi: bent over, striking down with both hands (p_erc_supplies_demolitionUS_low_*)
//   carry, sweep   no vanilla clip reads as carrying a load or a broom stroke (the rally emote's npc_cardboard_*
//                  hold has a stale GUID in the game's own anim set and does not load), so they play our own clips
//                  (Anims/LF/Work/LF_WorkCarry.asi and LF_WorkSweep.asi, built by toolscli work-anim, #4396),
//                  looked up through LF_WorkAnimPlayer.JobPose. A job with no pose of its own plays the base game's
//                  plain idle loiter (ELoiteringType.LOITERING). A job gets its pose from an .asi on LF_WorkPick.ast
//                  and a PoseOf branch.
// A carry pose holds a box and a sweep pose a broom (LF_ResidentWorkProps: the loiter data carries an item preset,
// so every machine that shows the pose spawns the prop locally and deletes it when the pose ends, #4644).
// The base game replicates a loiter to every client (late joiners too, through the command handler's RplSave),
// so nothing here is sent by hand. A 1 s
// tick starts the pose again for a worker whose loiter ended by itself (a shove, a fall) and drops a dead or
// deleted body. Server only; nothing persists (the resident is re-spawned from the timetable and the pose starts
// again). Entries hold EntityID. A timetable line reaches it through LF_ResidentActivity.Apply (#4366); a placed
// character gets it from LF_ResidentWorkLoopComponent.

//------------------------------------------------------------------------------------------------
//! What one worker is doing now.
class LF_ResidentWorkState
{
	string m_sActivity;
	ResourceName m_sPose;
	int m_iStartedMs;
	int m_iStarts;
}

//------------------------------------------------------------------------------------------------
class LF_ResidentWorkLoop
{
	static const string FEATURE = "Residents";
	static const string CARRY = "carry";
	static const string HAMMER = "hammer";
	static const string SWEEP = "sweep";
	static const ResourceName HAMMER_POSE = "{B8807790A1C2C068}Anims/Workspaces/Work/LF_WorkHammer.asi";
	//! The loop looks at every worker this often. Housekeeping, not tuning.
	static const int TICK_MS = 1000; // lf-fixed: a housekeeping tick, not tuning
	//! A pose that is not showing this long after its start is started again.
	static const int RESTART_MS = 4000; // lf-fixed: the loiter's blend-in time with slack, not tuning

	protected static ref map<EntityID, ref LF_ResidentWorkState> s_mState;
	protected static bool s_bTicking;

	//------------------------------------------------------------------------------------------------
	//! World end (and tests): stops the tick and forgets every worker.
	static void ResetStatics()
	{
		if (s_bTicking && GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(Sweep);

		s_bTicking = false;
		s_mState = null;
	}

	//------------------------------------------------------------------------------------------------
	//! The rule, with no world: the work activity a name stands for ("" when it is not one). Case and spaces
	//! are ignored. \param[out] known false when the name is not a work activity.
	static string Normalise(string activity, out bool known)
	{
		string name = activity;
		name.TrimInPlace();
		name.ToLower();
		known = name == CARRY || name == HAMMER || name == SWEEP;
		if (!known)
			return "";

		return name;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsWork(string activity)
	{
		bool known;
		Normalise(activity, known);
		return known;
	}

	//------------------------------------------------------------------------------------------------
	//! The rule, with no world: the anim set instance of the job's pose on the work graph, or "" when the job
	//! has no pose of its own yet (or is not work) and the resident plays the plain idle loiter.
	static ResourceName PoseOf(string activity)
	{
		bool known;
		string name = Normalise(activity, known);
		if (name == HAMMER)
			return HAMMER_POSE;

		// Carry and sweep play our own clips (#4396, Anims/LF/Work).
		if (name == CARRY || name == SWEEP)
			return LF_WorkAnimPlayer.JobPose(name);

		return ResourceName.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! The loiter data for a job pose: the work graph with the job's clips, entered through its Gestures value.
	//! param[in] item the prop the pose holds (LF_ResidentWorkProps.PresetOf): every machine that shows the pose
	//! spawns it locally and deletes it when the pose ends.
	static SCR_LoiterCustomAnimData PoseData(ResourceName pose, SCR_EAnimationItemID item = SCR_EAnimationItemID.NONE)
	{
		return SCR_LoiterCustomAnimData.CreateInstance(-1, -1, string.Empty, SCR_CustomAnimData_Properties.BINDING_NAME_NPC, LF_WorkAnimPlayer.PICK_GRAPH, pose, -1, LF_EWorkAnimKind.PICK, LF_WorkAnimPlayer.CONTROL_VARIABLE, item);
	}

	//------------------------------------------------------------------------------------------------
	//! The loiter data a worker's pose starts with: the job's pose and the prop it holds (#4644).
	static SCR_LoiterCustomAnimData DataOf(notnull LF_ResidentWorkState state)
	{
		return PoseData(state.m_sPose, LF_ResidentWorkProps.PresetOf(state.m_sActivity));
	}

	//------------------------------------------------------------------------------------------------
	protected static map<EntityID, ref LF_ResidentWorkState> State()
	{
		if (!s_mState)
			s_mState = new map<EntityID, ref LF_ResidentWorkState>();

		return s_mState;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the worker takes their job's pose and holds it until Stop. Beginning again with the same
	//! activity keeps the running pose; a different one swaps it. \return false for a name that is not work,
	//! a body with no character controller, or a client.
	static bool Begin(IEntity entity, string activity)
	{
		if (!entity || !Replication.IsServer())
			return false;

		bool known;
		string name = Normalise(activity, known);
		if (!known)
		{
			LF_Debug.Warn(FEATURE, string.Format("work loop: '%1' is not a work activity (carry, hammer, sweep)", activity));
			return false;
		}

		if (!Controller(entity))
		{
			LF_Debug.Warn(FEATURE, string.Format("work loop: %1 has no character controller; the resident stands", entity.GetID()));
			return false;
		}

		LF_ResidentWorkState current = State().Get(entity.GetID());
		if (current && current.m_sActivity == name)
			return true;

		LF_ResidentWorkState state = new LF_ResidentWorkState();
		state.m_sActivity = name;
		state.m_sPose = PoseOf(name);
		State().Set(entity.GetID(), state);
		string pose = "the idle loiter (no job pose yet)";
		if (!state.m_sPose.IsEmpty())
			pose = state.m_sPose;

		LF_Debug.Info(FEATURE, string.Format("work loop: %1 starts %2 with %3", entity.GetID(), name, pose));
		StartPose(entity, state);

		if (!s_bTicking)
		{
			s_bTicking = true;
			GetGame().GetCallqueue().CallLater(Sweep, TICK_MS, true);
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static void StartPose(notnull IEntity entity, notnull LF_ResidentWorkState state)
	{
		SCR_CharacterControllerComponent controller = Controller(entity);
		if (!controller)
			return;

		// A hand signal or an earlier pose gives way to the work.
		controller.StopCharacterGesture();
		if (controller.IsLoitering())
			controller.StopLoitering(true);

		vector none[4];
		if (state.m_sPose.IsEmpty())
			controller.StartLoitering(null, ELoiteringType.LOITERING, true, true, false, none);
		else
			controller.StartLoitering(null, ELoiteringType.CUSTOM, true, true, false, none, true, DataOf(state));

		state.m_iStartedMs = System.GetTickCount();
		state.m_iStarts++;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one look at the worker `elapsedMs` after their pose last started; a pose that is not showing
	//! after RESTART_MS starts again. The 1 s tick and the tests both come through here.
	static void Check(IEntity entity, int elapsedMs)
	{
		if (!entity || !s_mState)
			return;

		LF_ResidentWorkState state = s_mState.Get(entity.GetID());
		if (!state || elapsedMs < RESTART_MS)
			return;

		SCR_CharacterControllerComponent controller = Controller(entity);
		if (!controller || controller.IsLoitering())
			return;

		LF_Debug.Info(FEATURE, string.Format("work loop: %1 stopped posing; %2 starts again", entity.GetID(), state.m_sActivity));
		StartPose(entity, state);
	}

	//------------------------------------------------------------------------------------------------
	//! The job pose the loop gave the worker ("" for the idle loiter or when they are not working).
	static ResourceName PlayedPose(IEntity entity)
	{
		if (!entity || !s_mState)
			return ResourceName.Empty;

		LF_ResidentWorkState state = s_mState.Get(entity.GetID());
		if (!state)
			return ResourceName.Empty;

		return state.m_sPose;
	}

	//------------------------------------------------------------------------------------------------
	//! How often the loop started the worker's pose (0 when they are not working).
	static int StartsOf(IEntity entity)
	{
		if (!entity || !s_mState)
			return 0;

		LF_ResidentWorkState state = s_mState.Get(entity.GetID());
		if (!state)
			return 0;

		return state.m_iStarts;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsRunning(IEntity entity)
	{
		return entity && s_mState && s_mState.Contains(entity.GetID());
	}

	//------------------------------------------------------------------------------------------------
	//! How many workers are working now (tests read it).
	static int Count()
	{
		if (!s_mState)
			return 0;

		return s_mState.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Ends the worker's pose (a despawn, a new timetable line).
	static void Stop(IEntity entity)
	{
		if (!entity || !s_mState)
			return;

		if (!s_mState.Contains(entity.GetID()))
			return;

		s_mState.Remove(entity.GetID());
		SCR_CharacterControllerComponent controller = Controller(entity);
		if (controller && controller.IsLoitering())
			controller.StopLoitering(true);

		LF_Debug.Info(FEATURE, string.Format("work loop: %1 stops", entity.GetID()));
	}

	//------------------------------------------------------------------------------------------------
	//! The 1 s tick: every worker keeps posing; a body that is gone or down leaves the loop.
	protected static void Sweep()
	{
		if (!s_mState || s_mState.IsEmpty())
		{
			if (s_bTicking && GetGame() && GetGame().GetCallqueue())
				GetGame().GetCallqueue().Remove(Sweep);

			s_bTicking = false;
			return;
		}

		array<EntityID> ids = {};
		foreach (EntityID id, LF_ResidentWorkState state : s_mState)
		{
			ids.Insert(id);
		}

		int now = System.GetTickCount();
		foreach (EntityID id : ids)
		{
			IEntity entity = GetGame().GetWorld().FindEntityByID(id);
			LF_ResidentWorkState state = s_mState.Get(id);
			if (!entity || !state)
			{
				s_mState.Remove(id);
				continue;
			}

			SCR_CharacterControllerComponent controller = Controller(entity);
			if (!controller || controller.GetLifeState() != ECharacterLifeState.ALIVE)
			{
				s_mState.Remove(id);
				continue;
			}

			Check(entity, now - state.m_iStartedMs);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static SCR_CharacterControllerComponent Controller(IEntity entity)
	{
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(entity);
		if (!character)
			return null;

		return SCR_CharacterControllerComponent.Cast(character.GetCharacterController());
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_ResidentWorkLoop's statics (LF_StaticReset finds it by reflection).
class LF_ResidentWorkLoop_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_ResidentWorkLoop";
	}

	override void Apply()
	{
		LF_ResidentWorkLoop.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Feature/Residents", description: "A placed character carries, hammers or sweeps a few seconds after the world loads: a map maker's worker who needs no timetable, and the grid proof's cast.")]
class LF_ResidentWorkLoopComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_ResidentWorkLoopComponent : ScriptComponent
{
	[Attribute("carry", UIWidgets.EditBox, desc: "carry, hammer or sweep")]
	protected string m_sActivity;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!GetGame().InPlayMode() || !Replication.IsServer())
			return;

		// The world has loaded and the character controller is ready by then.
		GetGame().GetCallqueue().CallLater(Begin, 3000, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void Begin()
	{
		IEntity owner = GetOwner();
		if (!owner)
			return;

		AIControlComponent control = AIControlComponent.Cast(owner.FindComponent(AIControlComponent));
		if (control && control.IsAIActivated())
			control.DeactivateAI();

		LF_ResidentWorkLoop.Begin(owner, m_sActivity);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(Begin);

		LF_ResidentWorkLoop.Stop(owner);
		super.OnDelete(owner);
	}
}
