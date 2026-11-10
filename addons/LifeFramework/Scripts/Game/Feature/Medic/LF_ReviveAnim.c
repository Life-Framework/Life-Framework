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
//! The medic's CPR while a revive runs (#512). The animation is a base-game clip played on top of
//! the character graph (PlayPostGraphAnimation), which is local to the machine that calls it, so
//! the server decides and broadcasts: the client only reports that the revive hold began, and the
//! server re-runs the revive rule (LF_Revive.Check) before telling every machine to animate. The
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
//! Client side of the CPR: loops the base-game in/loop clips on top of the graph, then the out clip.
class LF_ReviveAnimPlayer
{
	static const ResourceName CLIP_IN = "anims/anm/player/injury/p_cro_revive_CPR_in.anm";
	static const ResourceName CLIP_LOOP = "anims/anm/player/injury/p_cro_revive_CPR_loop.anm";
	static const ResourceName CLIP_OUT = "anims/anm/player/injury/p_cro_revive_CPR_out.anm";
	static const string BONE_MASK = "Body";
	static const float BLEND = 0.2;

	//------------------------------------------------------------------------------------------------
	static void Start(IEntity character)
	{
		CharacterAnimGraphComponent graph = CharacterAnimGraphComponent.Cast(character.FindComponent(CharacterAnimGraphComponent));
		if (!graph)
		{
			LF_Debug.Error(LF_ReviveAnim.FEATURE, "revive animation: the character has no animation graph");
			return;
		}

		if (!graph.PlayPostGraphAnimation(CLIP_IN, BONE_MASK, BLEND, BLEND))
			LF_Debug.Error(LF_ReviveAnim.FEATURE, "revive animation: the CPR clip did not play");

		LF_Debug.Info(LF_ReviveAnim.FEATURE, "revive animation playing on " + character.GetID());
		GetGame().GetCallqueue().CallLater(Loop, 250, true, character);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Loop(IEntity character)
	{
		if (!character)
			return;

		CharacterAnimGraphComponent graph = CharacterAnimGraphComponent.Cast(character.FindComponent(CharacterAnimGraphComponent));
		if (!graph)
			return;

		if (graph.GetPostGraphAnimationRemainingTime(CLIP_IN) > 0.3 || graph.GetPostGraphAnimationRemainingTime(CLIP_LOOP) > 0.3)
			return;

		graph.PlayPostGraphAnimation(CLIP_LOOP, BONE_MASK, BLEND, BLEND);
	}

	//------------------------------------------------------------------------------------------------
	static void Stop(IEntity character)
	{
		GetGame().GetCallqueue().Remove(Loop);
		CharacterAnimGraphComponent graph = CharacterAnimGraphComponent.Cast(character.FindComponent(CharacterAnimGraphComponent));
		if (!graph)
			return;

		graph.StopPostGraphAnimation(CLIP_IN, BLEND);
		graph.StopPostGraphAnimation(CLIP_LOOP, BLEND);
		graph.PlayPostGraphAnimation(CLIP_OUT, BONE_MASK, BLEND, BLEND);
		LF_Debug.Info(LF_ReviveAnim.FEATURE, "revive animation stopped on " + character.GetID());
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_ChimeraCharacter
{
	//------------------------------------------------------------------------------------------------
	//! Server: start or stop the medic CPR on every machine.
	void LF_BroadcastReviveAnim(bool start)
	{
		Rpc(RpcDo_LF_ReviveAnim, start);
		RpcDo_LF_ReviveAnim(start);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_LF_ReviveAnim(bool start)
	{
		// A dedicated server renders nothing.
		if (RplSession.Mode() == RplMode.Dedicated)
			return;

		if (start)
			LF_ReviveAnimPlayer.Start(this);
		else
			LF_ReviveAnimPlayer.Stop(this);
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
