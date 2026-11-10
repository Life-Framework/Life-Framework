//------------------------------------------------------------------------------------------------
enum LF_EReviveAnimStep
{
	KEEP,
	STOP_REFUSED,
	STOP_TIMEOUT
}

//------------------------------------------------------------------------------------------------
//! One medic currently doing CPR on one patient. Server only; entity ids stay on the server.
class LF_ReviveAnimSession : Managed
{
	EntityID m_Medic;
	EntityID m_Patient;
	float m_fStartedAt;
}

//------------------------------------------------------------------------------------------------
//! The medic's CPR while a revive runs (#512). The server decides: the client only reports that the
//! revive hold began, and the server re-runs the revive rule (LF_Revive.Check) before it tells the
//! medic's owner to start the CPR loiter (LF_ReviveAnimPlayer), which the base game replicates. The
//! server then watches the pair: the animation stops when the hold finishes or is cancelled, the
//! medic leaves reach, the patient is no longer downed, or the hold overruns.
class LF_ReviveAnim
{
	static const string FEATURE = "Medic";
	static const float HOLD_SECONDS = 4;
	static const float MAX_SECONDS = 7;
	static const int WATCH_MS = 400;

	protected static ref array<ref LF_ReviveAnimSession> s_aSessions = {};
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
	//! Pure: should a running animation go on? `refusal` is LF_Revive.Check for the pair now.
	static LF_EReviveAnimStep Step(LF_EReviveRefusal refusal, float elapsedSeconds)
	{
		if (refusal != LF_EReviveRefusal.NONE)
			return LF_EReviveAnimStep.STOP_REFUSED;

		if (elapsedSeconds > MAX_SECONDS)
			return LF_EReviveAnimStep.STOP_TIMEOUT;

		return LF_EReviveAnimStep.KEEP;
	}

	//------------------------------------------------------------------------------------------------
	protected static float Now()
	{
		return GetGame().GetWorld().GetWorldTime() / 1000;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_ReviveAnimSession Find(IEntity medic)
	{
		if (!medic)
			return null;

		foreach (LF_ReviveAnimSession session : s_aSessions)
		{
			if (session.m_Medic == medic.GetID())
				return session;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the medic began the revive hold. Starts the animation on every machine when the
	//! revive rule allows it; `medicOnDuty` is resolved by the caller.
	//! \return true when the animation is running for this medic.
	static bool Begin(IEntity medic, IEntity patient, bool medicOnDuty)
	{
		if (!Replication.IsServer() || !medic || !patient)
			return false;

		LF_EReviveRefusal refusal = LF_Revive.Check(medic, patient, medicOnDuty);
		if (refusal != LF_EReviveRefusal.NONE)
		{
			LF_Debug.Info(FEATURE, "revive animation refused: " + typename.EnumToString(LF_EReviveRefusal, refusal));
			return false;
		}

		if (Find(medic))
			return true;

		LF_ReviveAnimSession session = new LF_ReviveAnimSession();
		session.m_Medic = medic.GetID();
		session.m_Patient = patient.GetID();
		session.m_fStartedAt = Now();
		s_aSessions.Insert(session);
		s_iStarts++;
		LF_Debug.Info(FEATURE, "revive animation started for " + LF_Utils.GetPlayerUid(medic));
		Broadcast(medic, true);
		if (!s_bWatching && !s_bSilent)
		{
			s_bWatching = true;
			GetGame().GetCallqueue().CallLater(Watch, WATCH_MS, true);
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the hold ended (done or cancelled) or a rule stopped it.
	static void End(IEntity medic, string reason)
	{
		LF_ReviveAnimSession session = Find(medic);
		if (!session)
			return;

		s_aSessions.RemoveItem(session);
		s_iStops++;
		LF_Debug.Info(FEATURE, "revive animation stopped: " + reason);
		Broadcast(medic, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one watch pass over the running sessions (the timer calls it; tests call it directly).
	static void Watch()
	{
		for (int i = s_aSessions.Count() - 1; i >= 0; i--)
		{
			LF_ReviveAnimSession session = s_aSessions[i];
			IEntity medic = GetGame().GetWorld().FindEntityByID(session.m_Medic);
			IEntity patient = GetGame().GetWorld().FindEntityByID(session.m_Patient);
			if (!medic || !patient)
			{
				s_aSessions.Remove(i);
				s_iStops++;
				LF_Debug.Info(FEATURE, "revive animation stopped: medic or patient gone");
				continue;
			}

			LF_EReviveAnimStep step = Step(LF_Revive.Check(medic, patient, true), Now() - session.m_fStartedAt);
			if (step == LF_EReviveAnimStep.KEEP)
				continue;

			End(medic, typename.EnumToString(LF_EReviveAnimStep, step));
		}

		if (s_aSessions.IsEmpty() && s_bWatching)
		{
			s_bWatching = false;
			GetGame().GetCallqueue().Remove(Watch);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void Broadcast(IEntity medic, bool start)
	{
		if (s_bSilent)
			return;

		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(medic);
		if (character)
			character.LF_BroadcastReviveAnim(start);
	}
}

//------------------------------------------------------------------------------------------------
//! The CPR on the character (#512). The player character has no CharacterAnimGraphComponent, so the
//! clips cannot play on top of the graph; they run as a custom loiter instead: our attachment graph
//! (Anims/Workspaces/Medic/LF_ReviveCPR.agr) holds the orphaned base-game p_cro_revive_CPR_* clips and
//! plugs into the player graph's NPC attachment, the way the rally emotes do. StartLoitering runs on
//! the medic's owner; the base game sends it to every other machine (Rpc_StartLoitering_BCNO, and
//! the command handler's RplSave for late joiners).
class LF_ReviveAnimPlayer
{
	static const ResourceName GRAPH = "{5409EFFEA8E73EA6}Anims/Workspaces/Medic/LF_ReviveCPR.agr";
	static const ResourceName GRAPH_INSTANCE = "{EBB8FFFCA77F5DDC}Anims/Workspaces/Medic/LF_ReviveCPR.asi";
	static const string CONTROL_VARIABLE = "Gestures";
	//! The value of Gestures that enters the CPR state of LF_ReviveCPR.agf.
	static const int CONTROL_VALUE = 560;
	//! How long a machine waits before it checks what it shows.
	static const int CHECK_MS = 1500;
	//! Model-space head height under which the medic counts as kneeling (standing is about 1.6 m).
	static const float KNEEL_HEAD_MAX = 1.3;

	//------------------------------------------------------------------------------------------------
	//! The loiter data for the CPR: our graph, driven by the Gestures variable (no command).
	static SCR_LoiterCustomAnimData CreateAnimData()
	{
		return SCR_LoiterCustomAnimData.CreateInstance(-1, -1, string.Empty, SCR_CustomAnimData_Properties.BINDING_NAME_NPC, GRAPH, GRAPH_INSTANCE, -1, CONTROL_VALUE, CONTROL_VARIABLE);
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
	//! Owner: kneel and do CPR. Holsters a weapon first; input stays locked so only the server's
	//! stop (or the base game's fall and possession exits) ends it.
	static void Start(IEntity character)
	{
		SCR_CharacterControllerComponent controller = FindController(character);
		if (!controller)
		{
			LF_Debug.Error(LF_ReviveAnim.FEATURE, "revive animation: the medic has no character controller");
			return;
		}

		if (controller.IsLoitering())
		{
			LF_Debug.Warn(LF_ReviveAnim.FEATURE, "revive animation: the medic is already in a loiter; CPR not started");
			return;
		}

		vector transform[4];
		character.GetWorldTransform(transform);
		controller.StartLoitering(null, ELoiteringType.CUSTOM, true, false, false, transform, true, CreateAnimData());
		LF_Debug.Info(LF_ReviveAnim.FEATURE, "revive animation: CPR loiter requested on the owner of " + character.GetID());
	}

	//------------------------------------------------------------------------------------------------
	//! Owner: stop at once. The fast exit detaches the graph; a soft exit would wait for a
	//! Gesture_End event that the CPR out clip does not carry, and the loiter would never finish.
	static void Stop(IEntity character)
	{
		SCR_CharacterControllerComponent controller = FindController(character);
		if (!controller)
			return;

		controller.StopLoitering(true);
		LF_Debug.Info(LF_ReviveAnim.FEATURE, "revive animation: CPR loiter stopped on the owner of " + character.GetID());
	}

	//------------------------------------------------------------------------------------------------
	//! Model-space height of the head bone, or -1 when the character has no head bone.
	static float HeadHeight(IEntity character)
	{
		Animation anim = character.GetAnimation();
		if (!anim)
			return -1;

		TNodeId head = anim.GetBoneIndex("Head");
		vector mat[4];
		if (!anim.GetBoneMatrix(head, mat))
			return -1;

		return mat[3][1];
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
		float head = HeadHeight(character);
		string seen = string.Format("loitering %1, head %2 m", loitering, head);
		if (!start)
		{
			LF_Debug.Info(LF_ReviveAnim.FEATURE, "revive animation ended on " + character.GetID() + " (" + seen + ")");
			return;
		}

		if (loitering && head > 0 && head < KNEEL_HEAD_MAX)
			LF_Debug.Info(LF_ReviveAnim.FEATURE, "revive animation playing on " + character.GetID() + " (" + seen + ")");
		else
			LF_Debug.Error(LF_ReviveAnim.FEATURE, "revive animation not visible on " + character.GetID() + " (" + seen + ")");
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_ChimeraCharacter
{
	//------------------------------------------------------------------------------------------------
	//! Server: start or stop the medic CPR. The owner drives the loiter; every machine checks it.
	void LF_BroadcastReviveAnim(bool start)
	{
		RplComponent rpl = GetRplComponent();
		if (!rpl || rpl.IsOwner())
			RpcDo_LF_ReviveAnimOwner(start);
		else
			Rpc(RpcDo_LF_ReviveAnimOwner, start);

		Rpc(RpcDo_LF_ReviveAnimCheck, start);
		RpcDo_LF_ReviveAnimCheck(start);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_ReviveAnimOwner(bool start)
	{
		if (start)
			LF_ReviveAnimPlayer.Start(this);
		else
			LF_ReviveAnimPlayer.Stop(this);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_LF_ReviveAnimCheck(bool start)
	{
		// A dedicated server renders nothing.
		if (RplSession.Mode() == RplMode.Dedicated)
			return;

		LF_ReviveAnimPlayer.Expect(this, start);
	}
}

//------------------------------------------------------------------------------------------------
//! Client -> server: the revive hold began or ended. The server takes the medic from this
//! controller's character and the patient from an RplId, and re-checks everything.
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	void LF_AskReviveAnim(RplId patient, bool start)
	{
		if (Replication.IsServer())
			RpcAsk_LF_ReviveAnim(patient, start);
		else
			Rpc(RpcAsk_LF_ReviveAnim, patient, start);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_ReviveAnim(RplId patient, bool start)
	{
		IEntity medic = GetControlledEntity();
		if (!start)
		{
			LF_ReviveAnim.End(medic, "hold cancelled");
			return;
		}

		bool onDuty;
		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		if (accounts && medic)
			onDuty = LF_MedicDuty.IsOnDuty(accounts.GetAccount(LF_Utils.GetPlayerUid(medic)));

		LF_ReviveAnim.Begin(medic, LF_NetworkUtils.FindEntityByRplId(patient), onDuty);
	}
}
