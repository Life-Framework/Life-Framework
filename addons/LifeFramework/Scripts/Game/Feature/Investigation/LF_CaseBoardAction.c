//------------------------------------------------------------------------------------------------
//! The case board's two actions (#148), offered to on-duty police only. The offer reads the
//! replicated account state; the server checks the officer again before it changes the board.
class LF_CaseBoardActionBase : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return LF_AccountStateBits.IsOnDutyPolice(LF_AccountReplication.GetState(user));
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (!LF_AccountStateBits.IsOnDutyPolice(LF_AccountReplication.GetState(user)))
		{
			SetCannotPerformReason("#LF-Evidence_OnDutyOnly");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the user is an on-duty officer; anyone else is refused and logged.
	protected bool ServerAllows(IEntity user)
	{
		if (!Replication.IsServer() || !user)
			return false;

		if (LF_CaseBoard.MayRead(user))
			return true;

		LF_Debug.Warn(LF_CaseBoard.FEATURE, string.Format("%1 refused: %2 is not an on-duty officer", Type().ToString(), user));
		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Tell(IEntity user, string message, string arg = "")
	{
		LF_Utils.NotifyEntity(user, message, "#LF-CaseBoard_Title", 5.0, arg);
	}
}

//------------------------------------------------------------------------------------------------
//! "Pin evidence": the bags the officer carries are logged and pinned; with none, the next logged
//! card of an open case goes up.
class LF_CaseBoardPinAction : LF_CaseBoardActionBase
{
	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#LF-CaseBoard_Pin_Action";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!ServerAllows(pUserEntity))
			return;

		int pinned = LF_CaseBoard.Pin(pUserEntity);
		if (pinned > 0)
			Tell(pUserEntity, "#LF-CaseBoard_Pinned", pinned.ToString());
		else
			Tell(pUserEntity, "#LF-CaseBoard_NothingToPin");
	}
}

//------------------------------------------------------------------------------------------------
//! "Unpin evidence": the card pinned last comes down. It stays logged in the locker.
class LF_CaseBoardUnpinAction : LF_CaseBoardActionBase
{
	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#LF-CaseBoard_Unpin_Action";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!ServerAllows(pUserEntity))
			return;

		if (LF_CaseBoard.Unpin())
			Tell(pUserEntity, "#LF-CaseBoard_Unpinned");
		else
			Tell(pUserEntity, "#LF-CaseBoard_NothingPinned");
	}
}
