// LF_ResidentWorkLoop.c - a generic worker carries, hammers or sweeps at their spot instead of standing still
// (#4230, epic #4170). Vanilla has no hammer or broom animation, so each work activity is a short cycle of
// vanilla character gestures with rests between them; a step runs for its seconds, then the next one starts,
// and the last step hands back to step 0:
//   carry   move, follow, rest        (haul one load across, set it down)
//   hammer  stop, rest, stop, rest    (two strikes, a pause)
//   sweep   move, rest, follow, rest  (a stroke each way)
// The loop is driven by the clock (seconds since Begin), never by frames: TickAt(entity, seconds) is the one
// path the 1 s sweep and the tests both take. Server only; nothing persists (the resident is re-spawned from the
// timetable and the loop starts again). Entries hold EntityID. A timetable line reaches it through
// LF_ResidentActivity.Apply (#4366); a placed character gets it from LF_ResidentWorkLoopComponent.

//------------------------------------------------------------------------------------------------
//! One step of a work cycle: a vanilla gesture (ECharacterGestures, NONE for a rest) and how long it lasts.
class LF_ResidentWorkStep
{
	int m_iGesture;
	int m_iSeconds;

	//------------------------------------------------------------------------------------------------
	static LF_ResidentWorkStep Make(int gesture, int seconds)
	{
		LF_ResidentWorkStep step = new LF_ResidentWorkStep();
		step.m_iGesture = gesture;
		step.m_iSeconds = seconds;
		return step;
	}
}

//------------------------------------------------------------------------------------------------
//! What one worker is doing now.
class LF_ResidentWorkState
{
	string m_sActivity;
	int m_iStartMs;
	int m_iStep = -1;
	int m_iGesture;
}

//------------------------------------------------------------------------------------------------
class LF_ResidentWorkLoop
{
	static const string FEATURE = "Residents";
	static const string CARRY = "carry";
	static const string HAMMER = "hammer";
	static const string SWEEP = "sweep";
	//! The loop re-reads the clock this often; steps are whole seconds. Housekeeping, not tuning.
	static const int TICK_MS = 1000; // lf-fixed: a housekeeping tick, not tuning

	protected static ref map<EntityID, ref LF_ResidentWorkState> s_mState;
	protected static ref map<string, ref array<ref LF_ResidentWorkStep>> s_mCycles;
	protected static bool s_bTicking;

	//------------------------------------------------------------------------------------------------
	//! World end (and tests): stops the sweep and forgets every worker.
	static void ResetStatics()
	{
		if (s_bTicking && GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(Sweep);

		s_bTicking = false;
		s_mState = null;
		s_mCycles = null;
	}

	//------------------------------------------------------------------------------------------------
	protected static map<string, ref array<ref LF_ResidentWorkStep>> Cycles()
	{
		if (s_mCycles)
			return s_mCycles;

		s_mCycles = new map<string, ref array<ref LF_ResidentWorkStep>>();

		array<ref LF_ResidentWorkStep> carry = {};
		carry.Insert(LF_ResidentWorkStep.Make(ECharacterGestures.COMMAND_MOVE, 5));
		carry.Insert(LF_ResidentWorkStep.Make(ECharacterGestures.COMMAND_FOLLOW, 5));
		carry.Insert(LF_ResidentWorkStep.Make(ECharacterGestures.NONE, 4));
		s_mCycles.Insert(CARRY, carry);

		array<ref LF_ResidentWorkStep> hammer = {};
		hammer.Insert(LF_ResidentWorkStep.Make(ECharacterGestures.COMMAND_STOP, 2));
		hammer.Insert(LF_ResidentWorkStep.Make(ECharacterGestures.NONE, 1));
		hammer.Insert(LF_ResidentWorkStep.Make(ECharacterGestures.COMMAND_STOP, 2));
		hammer.Insert(LF_ResidentWorkStep.Make(ECharacterGestures.NONE, 3));
		s_mCycles.Insert(HAMMER, hammer);

		array<ref LF_ResidentWorkStep> sweep = {};
		sweep.Insert(LF_ResidentWorkStep.Make(ECharacterGestures.COMMAND_MOVE, 3));
		sweep.Insert(LF_ResidentWorkStep.Make(ECharacterGestures.NONE, 2));
		sweep.Insert(LF_ResidentWorkStep.Make(ECharacterGestures.COMMAND_FOLLOW, 3));
		sweep.Insert(LF_ResidentWorkStep.Make(ECharacterGestures.NONE, 2));
		s_mCycles.Insert(SWEEP, sweep);

		return s_mCycles;
	}

	//------------------------------------------------------------------------------------------------
	//! The rule, with no world: the work activity a name stands for ("" when it is not one). Case and spaces
	//! are ignored. \param[out] known false when the name is not a work activity.
	static string Normalise(string activity, out bool known)
	{
		string name = activity;
		name.TrimInPlace();
		name.ToLower();
		known = Cycles().Contains(name);
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
	//! How many steps the activity's cycle has (0 for a name that is not work).
	static int StepCount(string activity)
	{
		bool known;
		string name = Normalise(activity, known);
		if (!known)
			return 0;

		return Cycles().Get(name).Count();
	}

	//------------------------------------------------------------------------------------------------
	//! How long step `step` lasts, in seconds (0 out of range).
	static int StepSeconds(string activity, int step)
	{
		LF_ResidentWorkStep one = StepOfCycle(activity, step);
		if (!one)
			return 0;

		return one.m_iSeconds;
	}

	//------------------------------------------------------------------------------------------------
	//! The vanilla gesture (ECharacterGestures) step `step` plays; NONE for a rest or an unknown step.
	static int GestureOf(string activity, int step)
	{
		LF_ResidentWorkStep one = StepOfCycle(activity, step);
		if (!one)
			return ECharacterGestures.NONE;

		return one.m_iGesture;
	}

	//------------------------------------------------------------------------------------------------
	//! Seconds one whole cycle takes (0 for a name that is not work).
	static int CycleSeconds(string activity)
	{
		int total = 0;
		for (int i = 0; i < StepCount(activity); i++)
		{
			total += StepSeconds(activity, i);
		}

		return total;
	}

	//------------------------------------------------------------------------------------------------
	//! The rule, with no world: the step the loop is on `seconds` after it began, wrapping around the cycle.
	//! -1 for a name that is not work.
	static int StepAt(string activity, int seconds)
	{
		int cycle = CycleSeconds(activity);
		if (cycle <= 0)
			return -1;

		int at = seconds % cycle;
		if (at < 0)
			at += cycle;

		for (int i = 0; i < StepCount(activity); i++)
		{
			at -= StepSeconds(activity, i);
			if (at < 0)
				return i;
		}

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_ResidentWorkStep StepOfCycle(string activity, int step)
	{
		bool known;
		string name = Normalise(activity, known);
		if (!known)
			return null;

		array<ref LF_ResidentWorkStep> cycle = Cycles().Get(name);
		if (step < 0 || step >= cycle.Count())
			return null;

		return cycle.Get(step);
	}

	//------------------------------------------------------------------------------------------------
	protected static map<EntityID, ref LF_ResidentWorkState> State()
	{
		if (!s_mState)
			s_mState = new map<EntityID, ref LF_ResidentWorkState>();

		return s_mState;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the worker starts their activity on step 0 and the clock keeps the cycle going. Beginning
	//! again with the same activity keeps the running loop. \return false for a name that is not work, a
	//! body with no character controller, or a client.
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
		state.m_iStartMs = System.GetTickCount();
		State().Set(entity.GetID(), state);
		LF_Debug.Info(FEATURE, string.Format("work loop: %1 starts %2", entity.GetID(), name));
		Apply(entity, state, 0);

		if (!s_bTicking)
		{
			s_bTicking = true;
			GetGame().GetCallqueue().CallLater(Sweep, TICK_MS, true);
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the worker's clock reads `seconds` since Begin; moves to the step that belongs there and plays
	//! its gesture when the step changed. The 1 s sweep and the tests both come through here.
	static void TickAt(IEntity entity, int seconds)
	{
		if (!entity)
			return;

		LF_ResidentWorkState state = State().Get(entity.GetID());
		if (!state)
			return;

		Apply(entity, state, StepAt(state.m_sActivity, seconds));
	}

	//------------------------------------------------------------------------------------------------
	protected static void Apply(notnull IEntity entity, notnull LF_ResidentWorkState state, int step)
	{
		if (step < 0 || step == state.m_iStep)
			return;

		state.m_iStep = step;
		state.m_iGesture = GestureOf(state.m_sActivity, step);

		SCR_CharacterControllerComponent controller = Controller(entity);
		if (!controller)
			return;

		controller.StopCharacterGesture();
		if (state.m_iGesture == ECharacterGestures.NONE)
			return;

		// The gesture is held for the step's length; a refused start (the body is busy) logs and waits for the next step.
		if (!controller.TryStartCharacterGesture(state.m_iGesture, StepSeconds(state.m_sActivity, step) * 1000))
			LF_Debug.Info(FEATURE, string.Format("work loop: %1 could not start gesture %2 on step %3", entity.GetID(), state.m_iGesture, step));
	}

	//------------------------------------------------------------------------------------------------
	//! The step the worker is on now (-1 when they are not working).
	static int StepOf(IEntity entity)
	{
		if (!entity || !s_mState)
			return -1;

		LF_ResidentWorkState state = s_mState.Get(entity.GetID());
		if (!state)
			return -1;

		return state.m_iStep;
	}

	//------------------------------------------------------------------------------------------------
	//! The gesture the loop last asked the worker to play (NONE for a rest or when they are not working).
	static int PlayedGesture(IEntity entity)
	{
		if (!entity || !s_mState)
			return ECharacterGestures.NONE;

		LF_ResidentWorkState state = s_mState.Get(entity.GetID());
		if (!state)
			return ECharacterGestures.NONE;

		return state.m_iGesture;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsRunning(IEntity entity)
	{
		return entity && s_mState && s_mState.Contains(entity.GetID());
	}

	//------------------------------------------------------------------------------------------------
	//! How many workers have a loop now (tests read it).
	static int Count()
	{
		if (!s_mState)
			return 0;

		return s_mState.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Ends the worker's loop and stops the gesture (a despawn, a new timetable line).
	static void Stop(IEntity entity)
	{
		if (!entity || !s_mState)
			return;

		if (!s_mState.Contains(entity.GetID()))
			return;

		s_mState.Remove(entity.GetID());
		SCR_CharacterControllerComponent controller = Controller(entity);
		if (controller)
			controller.StopCharacterGesture();

		LF_Debug.Info(FEATURE, string.Format("work loop: %1 stops", entity.GetID()));
	}

	//------------------------------------------------------------------------------------------------
	//! The 1 s tick: every worker moves to the step their clock says; a body that is gone leaves the loop.
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

			TickAt(entity, (now - state.m_iStartMs) / 1000);
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
