//------------------------------------------------------------------------------------------------
//! What a working player does. The value is what the work graph's Gestures variable must hold to
//! enter that state (Anims/Workspaces/Work/LF_WorkLoop.agf).
enum LF_EWorkAnimKind
{
	NONE = 0,
	PICK = 561,
	DRILL = 562
}

//------------------------------------------------------------------------------------------------
enum LF_EWorkAnimStep
{
	KEEP,
	STOP_DOWN,
	STOP_RANGE,
	STOP_TIMEOUT
}

//------------------------------------------------------------------------------------------------
//! One player currently working. Server only; entity ids stay on the server.
class LF_WorkAnimSession : Managed
{
	EntityID m_Worker;
	vector m_vAnchor;
	LF_EWorkAnimKind m_eKind;
	float m_fStartedAt;
	float m_fSeconds;
	//! True for work nobody else ends (the drill): the watch ends it when its time is up.
	bool m_bEndsItself;
}

//------------------------------------------------------------------------------------------------
//! The working animation of the lockpick (#411, #412) and the vault drill (#273) (#513). The server
//! owns the timer: the lockpick attempt and the drill call Begin when their timed work starts and
//! End when it finishes, drops or is cancelled; the watch here also stops it when the worker goes
//! down, leaves reach or the work overruns. Begin tells the worker's owner to start the work loiter
//! (LF_WorkAnimPlayer); the base game replicates the loiter to every other client.
class LF_WorkAnim
{
	static const string FEATURE = "WorkAnim";
	static const float MAX_RANGE = 5;
	static const float SLACK_SECONDS = 2;
	static const float DRILL_SECONDS = 6;
	static const int WATCH_MS = 400;

	protected static ref array<ref LF_WorkAnimSession> s_aSessions = {};
	protected static bool s_bWatching;
	protected static bool s_bSilent;
	protected static int s_iStarts;
	protected static int s_iStops;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_aSessions.Clear();
		s_bWatching = false;
		s_bSilent = false;
		s_iStarts = 0;
		s_iStops = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: count starts and stops without sending anything to clients.
	static void SetSilent(bool silent)
	{
		s_bSilent = silent;
	}

	//------------------------------------------------------------------------------------------------
	static int GetStartCount()
	{
		return s_iStarts;
	}

	//------------------------------------------------------------------------------------------------
	static int GetStopCount()
	{
		return s_iStops;
	}

	//------------------------------------------------------------------------------------------------
	static int GetActiveCount()
	{
		return s_aSessions.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: should the working animation go on?
	static LF_EWorkAnimStep Step(bool alive, float distance, float elapsedSeconds, float seconds)
	{
		if (!alive)
			return LF_EWorkAnimStep.STOP_DOWN;

		if (distance > MAX_RANGE)
			return LF_EWorkAnimStep.STOP_RANGE;

		if (elapsedSeconds > seconds + SLACK_SECONDS)
			return LF_EWorkAnimStep.STOP_TIMEOUT;

		return LF_EWorkAnimStep.KEEP;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: has self-ending work run its time?
	static bool IsDone(bool endsItself, float elapsedSeconds, float seconds)
	{
		return endsItself && elapsedSeconds >= seconds;
	}

	//------------------------------------------------------------------------------------------------
	protected static float Now()
	{
		return GetGame().GetWorld().GetWorldTime() / 1000;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_WorkAnimSession Find(IEntity worker)
	{
		if (!worker)
			return null;

		foreach (LF_WorkAnimSession session : s_aSessions)
		{
			if (session.m_Worker == worker.GetID())
				return session;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: timed work began at `anchor`. Starts the animation on every machine.
	//! \return true when the animation is running for this worker.
	static bool Begin(IEntity worker, vector anchor, LF_EWorkAnimKind kind, float seconds, bool endsItself = false)
	{
		if (!Replication.IsServer() || !worker || kind == LF_EWorkAnimKind.NONE || seconds <= 0)
			return false;

		if (Find(worker))
			return true;

		LF_WorkAnimSession session = new LF_WorkAnimSession();
		session.m_Worker = worker.GetID();
		session.m_vAnchor = anchor;
		session.m_eKind = kind;
		session.m_fStartedAt = Now();
		session.m_fSeconds = seconds;
		session.m_bEndsItself = endsItself;
		s_aSessions.Insert(session);
		s_iStarts++;
		LF_Debug.Info(FEATURE, string.Format("work animation started for %1: %2 for %3 s", LF_Utils.GetPlayerUid(worker), typename.EnumToString(LF_EWorkAnimKind, kind), seconds));
		Broadcast(worker, kind, true);
		if (!s_bWatching && !s_bSilent)
		{
			s_bWatching = true;
			GetGame().GetCallqueue().CallLater(Watch, WATCH_MS, true);
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the work finished, was cancelled or a rule stopped it. Harmless when nothing runs.
	static void End(IEntity worker, string reason)
	{
		LF_WorkAnimSession session = Find(worker);
		if (!session)
			return;

		s_aSessions.RemoveItem(session);
		s_iStops++;
		LF_Debug.Info(FEATURE, "work animation stopped: " + reason);
		Broadcast(worker, session.m_eKind, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one watch pass over the running sessions (the timer calls it; tests call it directly).
	static void Watch()
	{
		for (int i = s_aSessions.Count() - 1; i >= 0; i--)
		{
			LF_WorkAnimSession session = s_aSessions[i];
			IEntity worker = GetGame().GetWorld().FindEntityByID(session.m_Worker);
			if (!worker)
			{
				s_aSessions.Remove(i);
				s_iStops++;
				LF_Debug.Info(FEATURE, "work animation stopped: worker gone");
				continue;
			}

			bool alive = true;
			CharacterControllerComponent controller = LF_Component<CharacterControllerComponent>.Find(worker);
			if (controller && controller.GetLifeState() != ECharacterLifeState.ALIVE)
				alive = false;

			if (IsDone(session.m_bEndsItself, Now() - session.m_fStartedAt, session.m_fSeconds))
			{
				End(worker, "work finished");
				continue;
			}

			LF_EWorkAnimStep step = Step(alive, vector.Distance(worker.GetOrigin(), session.m_vAnchor), Now() - session.m_fStartedAt, session.m_fSeconds);
			if (step != LF_EWorkAnimStep.KEEP)
				End(worker, typename.EnumToString(LF_EWorkAnimStep, step));
		}

		if (s_aSessions.IsEmpty() && s_bWatching)
		{
			s_bWatching = false;
			GetGame().GetCallqueue().Remove(Watch);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void Broadcast(IEntity worker, LF_EWorkAnimKind kind, bool start)
	{
		if (s_bSilent)
			return;

		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(worker);
		if (character)
			character.LF_BroadcastWorkAnim(kind, start);
	}
}

//------------------------------------------------------------------------------------------------
//! The working pose on the character (#513). Like the CPR (LF_ReviveAnimPlayer), the clips run as a
//! custom loiter on our own attachment graph (Anims/Workspaces/Work/LF_WorkPick.agr and LF_WorkDrill.agr: base-game
//! repair and demolition work clips) because the player has no CharacterAnimGraphComponent. StartLoitering
//! runs on the worker's owner; the base game sends it to every other machine.
class LF_WorkAnimPlayer
{
	static const ResourceName PICK_GRAPH = "{602B8C98F90DC596}Anims/Workspaces/Work/LF_WorkPick.agr";
	static const ResourceName PICK_GRAPH_INSTANCE = "{B2DDAE6C2907A8B9}Anims/Workspaces/Work/LF_WorkPick.asi";
	static const ResourceName DRILL_GRAPH = "{94DB5A5051B77D67}Anims/Workspaces/Work/LF_WorkDrill.agr";
	static const ResourceName DRILL_GRAPH_INSTANCE = "{F5F6D8DB80E8A58F}Anims/Workspaces/Work/LF_WorkDrill.asi";
	static const string CONTROL_VARIABLE = "Gestures";
	//! How long a machine waits before it checks what it shows.
	static const int CHECK_MS = 1500;

	//------------------------------------------------------------------------------------------------
	static ResourceName GraphOf(int kind)
	{
		if (kind == LF_EWorkAnimKind.DRILL)
			return DRILL_GRAPH;

		return PICK_GRAPH;
	}

	//------------------------------------------------------------------------------------------------
	static ResourceName InstanceOf(int kind)
	{
		if (kind == LF_EWorkAnimKind.DRILL)
			return DRILL_GRAPH_INSTANCE;

		return PICK_GRAPH_INSTANCE;
	}

	//------------------------------------------------------------------------------------------------
	//! Owner side: workers whose loiter this machine already stopped, so a late start check does not call
	//! a finished short job (a lagging machine) "not visible".
	protected static ref array<EntityID> s_aStopped = {};

	//------------------------------------------------------------------------------------------------
	//! The loiter data for one kind of work: that kind's graph, driven by the Gestures variable (no command).
	static SCR_LoiterCustomAnimData CreateAnimData(int kind)
	{
		return SCR_LoiterCustomAnimData.CreateInstance(-1, -1, string.Empty, SCR_CustomAnimData_Properties.BINDING_NAME_NPC, GraphOf(kind), InstanceOf(kind), -1, kind, CONTROL_VARIABLE);
	}

	//------------------------------------------------------------------------------------------------
	protected static SCR_CharacterControllerComponent FindController(IEntity character)
	{
		ChimeraCharacter chimera = ChimeraCharacter.Cast(character);
		if (!chimera)
			return null;

		return SCR_CharacterControllerComponent.Cast(chimera.GetCharacterController());
	}

	//------------------------------------------------------------------------------------------------
	//! Owner: bend to the work. Input stays locked so only the server's stop ends it.
	static void Start(IEntity character, int kind)
	{
		SCR_CharacterControllerComponent controller = FindController(character);
		if (!controller)
		{
			LF_Debug.Error(LF_WorkAnim.FEATURE, "work animation: the worker has no character controller");
			return;
		}

		if (controller.IsLoitering())
		{
			LF_Debug.Warn(LF_WorkAnim.FEATURE, "work animation: the worker is already in a loiter; work pose not started");
			return;
		}

		s_aStopped.RemoveItem(character.GetID());
		vector transform[4];
		character.GetWorldTransform(transform);
		controller.StartLoitering(null, ELoiteringType.CUSTOM, true, false, false, transform, true, CreateAnimData(kind));
		LF_Debug.Info(LF_WorkAnim.FEATURE, "work animation: loiter requested on the owner of " + character.GetID());
	}

	//------------------------------------------------------------------------------------------------
	//! Owner: stop at once. The fast exit detaches the graph (#512: an out clip without a
	//! Gesture_End event never finishes a soft exit).
	static void Stop(IEntity character)
	{
		SCR_CharacterControllerComponent controller = FindController(character);
		if (!controller || !controller.IsLoitering())
			return;

		controller.StopLoitering(true);
		if (s_aStopped.Find(character.GetID()) == -1)
			s_aStopped.Insert(character.GetID());

		LF_Debug.Info(LF_WorkAnim.FEATURE, "work animation: loiter stopped on the owner of " + character.GetID());
	}

	//------------------------------------------------------------------------------------------------
	//! Every machine that renders: once the loiter had time to arrive, log what this machine shows.
	static void Expect(IEntity character, bool start)
	{
		GetGame().GetCallqueue().CallLater(Check, CHECK_MS, false, character, start);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Check(IEntity character, bool start)
	{
		if (!character)
			return;

		SCR_CharacterControllerComponent controller = FindController(character);
		bool loitering = controller && controller.IsLoitering();
		float head = LF_ReviveAnimPlayer.HeadHeight(character);
		string seen = string.Format("loitering %1, head %2 m", loitering, head);
		if (!start)
		{
			LF_Debug.Info(LF_WorkAnim.FEATURE, "work animation ended on " + character.GetID() + " (" + seen + ")");
			return;
		}

		if (!loitering && s_aStopped.Find(character.GetID()) != -1)
		{
			LF_Debug.Info(LF_WorkAnim.FEATURE, "work animation finished before the check on " + character.GetID() + " (" + seen + ")");
			return;
		}

		if (loitering)
			LF_Debug.Info(LF_WorkAnim.FEATURE, "work animation playing on " + character.GetID() + " (" + seen + ")");
		else
			LF_Debug.Error(LF_WorkAnim.FEATURE, "work animation not visible on " + character.GetID() + " (" + seen + ")");
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_ChimeraCharacter
{
	//------------------------------------------------------------------------------------------------
	//! Server: start or stop the working pose. The owner drives the loiter; every machine checks it.
	void LF_BroadcastWorkAnim(int kind, bool start)
	{
		RplComponent rpl = GetRplComponent();
		if (!rpl || rpl.IsOwner())
			RpcDo_LF_WorkAnimOwner(kind, start);
		else
			Rpc(RpcDo_LF_WorkAnimOwner, kind, start);

		Rpc(RpcDo_LF_WorkAnimCheck, start);
		RpcDo_LF_WorkAnimCheck(start);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_WorkAnimOwner(int kind, bool start)
	{
		if (start)
			LF_WorkAnimPlayer.Start(this, kind);
		else
			LF_WorkAnimPlayer.Stop(this);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_LF_WorkAnimCheck(bool start)
	{
		// A dedicated server renders nothing.
		if (RplSession.Mode() == RplMode.Dedicated)
			return;

		LF_WorkAnimPlayer.Expect(this, start);
	}
}
