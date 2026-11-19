//------------------------------------------------------------------------------------------------
//! The three actions of a crew robbery (#3404), placed beside LF_RobAction on a till whose
//! m_bCrewJob is set. They are shown to every civilian near the till; the server judges every press
//! and tells the player why it was refused. A client copy does nothing.
//------------------------------------------------------------------------------------------------
class LF_CrewRobAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	//! The crew till this action sits on, or null when the till is not marked for crew jobs.
	protected LF_RobAction Till()
	{
		LF_RobAction till = LF_RobAction.FromOwner(GetOwner());
		if (!till || !till.IsCrewJob())
			return null;

		return till;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!LF_Systems.IsEnabled(LF_RobAction.SYSTEM))
			return false;

		LF_RobAction till = Till();
		if (!till)
			return false;

		return LF_AccountStateBits.IsCivilian(LF_AccountReplication.GetState(user));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: tell `user` why the press was refused.
	protected static void TellRefusal(IEntity user, string refusal)
	{
		if (refusal == LF_RobAction.REFUSED_GRACE)
		{
			LF_Grace.Notify(user);
			return;
		}

		LF_Utils.NotifyEntity(user, LF_CrewRob.RefusalKey(refusal), "#LF-CrewRob_Title", 3.0);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: `user` as a crew member of `till`'s job (for the holder checks), or null.
	protected static LF_CrewRobMember MemberOf(IEntity user)
	{
		if (!user)
			return null;

		return LF_CrewRobMember.Make(user, LF_Utils.GetPlayerUid(user));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: false when this press must not run (not the server, no till, rate limited).
	protected bool Allowed(IEntity user)
	{
		if (!Replication.IsServer() || !user)
			return false;

		PlayerManager players = GetGame().GetPlayerManager();
		int playerId = players.GetPlayerIdFromControlledEntity(user);
		return LF_RateLimit.Allow(playerId, LF_ERequestKind.CRIME, "crew robbery", false);
	}
}

//------------------------------------------------------------------------------------------------
//! "Start the job": forms the crew from the armed civilians near the starter and sounds the alarm.
class LF_CrewRobStartAction : LF_CrewRobAction
{
	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#LF-CrewRob_ActionStart";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Allowed(pUserEntity))
			return;

		LF_RobAction till = Till();
		if (!till)
		{
			TellRefusal(pUserEntity, LF_CrewRob.REFUSED_NOT_CREW_TILL);
			return;
		}

		// The starter must hold a weapon: the crew is armed, the job is not a bluff.
		if (!LF_HoldUp.IsArmed(pUserEntity))
		{
			TellRefusal(pUserEntity, LF_CrewRob.REFUSED_CREW);
			return;
		}

		array<ref LF_CrewRobMember> crew = {};
		LF_CrewRob.GatherCrew(pUserEntity, crew);
		string refusal = LF_CrewRob.Start(till, pOwnerEntity.GetOrigin(), crew, LF_AccountReplication.CountPoliceOnDuty());
		if (!refusal.IsEmpty())
		{
			TellRefusal(pUserEntity, refusal);
			return;
		}

		foreach (LF_CrewRobMember member : crew)
		{
			IEntity body = member.Body();
			if (body)
				LF_Utils.NotifyEntity(body, "#LF-CrewRob_Started", "#LF-CrewRob_Title", 4.0);
		}
	}
}

//------------------------------------------------------------------------------------------------
//! "Hold the till": the holder keeps the spot while the take grows.
class LF_CrewRobHoldAction : LF_CrewRobAction
{
	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#LF-CrewRob_ActionHold";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Allowed(pUserEntity))
			return;

		string refusal = LF_CrewRob.Hold(Till(), MemberOf(pUserEntity));
		if (!refusal.IsEmpty())
		{
			TellRefusal(pUserEntity, refusal);
			return;
		}

		LF_Utils.NotifyEntity(pUserEntity, "#LF-CrewRob_Holding", "#LF-CrewRob_Title", 4.0);
	}
}

//------------------------------------------------------------------------------------------------
//! "Take the money": the holder ends the job and the crew present is paid.
class LF_CrewRobTakeAction : LF_CrewRobAction
{
	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#LF-CrewRob_ActionTake";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Allowed(pUserEntity))
			return;

		string refusal = LF_CrewRob.TakeMoney(Till(), MemberOf(pUserEntity));
		if (!refusal.IsEmpty())
			TellRefusal(pUserEntity, refusal);
	}
}
