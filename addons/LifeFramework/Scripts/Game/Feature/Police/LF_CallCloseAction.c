//------------------------------------------------------------------------------------------------
//! The police car's radio rows that close the call the officer holds (#4585): "Radio: call resolved",
//! "Radio: no one found" and "Radio: call unfounded". The rows sit on the car beside the unit status rows
//! (LF_UnitStatusAction), so the officer closes a call where they parked at the scene, not at the station
//! board. The prompt reads only the replicated duty bit; the server finds the call this officer took at
//! the board, the nearest when they hold several (LF_PoliceCalls.HeldBy), and closes it through LF_CallOutcome.Close, which checks the officer
//! holds it and stands within the call radius of the scene. The duty bonus, call XP and the 911-abuse
//! count hang off that close.
class LF_CallCloseAction : ScriptedUserAction
{
	[Attribute("1", UIWidgets.ComboBox, "The outcome this row radios.", "", ParamEnumArray.FromEnum(LF_CallOutcomeKind))]
	protected LF_CallOutcomeKind m_eOutcome;

	//! How far from the car the server accepts a radio press, in metres (inside the cab or at the door).
	static const float REACH = 6.0;	// lf-fixed: the car's own size (cab to driver door), same as LF_UnitStatusAction

	//------------------------------------------------------------------------------------------------
	LF_CallOutcomeKind Outcome()
	{
		return m_eOutcome;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the row's label for an outcome.
	static string ActionKey(LF_CallOutcomeKind outcome)
	{
		if (outcome == LF_CallOutcomeKind.NO_ONE_FOUND)
			return "#LF-CallOutcome_ActionNoOneFound";

		if (outcome == LF_CallOutcomeKind.UNFOUNDED)
			return "#LF-CallOutcome_ActionUnfounded";

		return "#LF-CallOutcome_ActionResolved";
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!LF_Systems.IsEnabled("Police"))
			return false;

		return LF_PlayerInteraction.IsOnDutyPolice(user);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = ActionKey(m_eOutcome);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the officer closes the call they hold with this outcome, standing at officerPosition.
	//! The account state is a parameter (LF_AccountStateBits), so no character is needed.
	//! \param messageKey The key to show the officer: the closing line, or the reason it was refused.
	//! \param messageArg The first argument of that key ("" when it takes none).
	//! \return True when the call closed.
	static bool ServerClose(string officerUid, int accountState, LF_CallOutcomeKind outcome, vector officerPosition, out string messageKey, out string messageArg)
	{
		messageArg = "";
		if (!Replication.IsServer())
			return false;

		if (!LF_AccountStateBits.IsOnDutyPolice(accountState))
		{
			messageKey = "#LF-CallOutcome_NoCall";
			LF_Debug.Warn(LF_CallOutcome.FEATURE, string.Format("radio close by %1 refused: not on duty", officerUid));
			return false;
		}

		LF_PoliceCall call = LF_PoliceCalls.HeldBy(officerUid, officerPosition);
		if (!call)
		{
			messageKey = "#LF-CallOutcome_NoneHeld";
			LF_Debug.Info(LF_CallOutcome.FEATURE, string.Format("radio close by %1 refused: they hold no open call", officerUid));
			return false;
		}

		return LF_CallOutcome.Close(call.m_iId, officerUid, outcome, officerPosition, messageKey, messageArg);
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (LF_Systems.Refuses("Police", "LF_CallCloseAction"))
			return;

		if (!Replication.IsServer() || !pOwnerEntity || !pUserEntity)
			return;

		if (vector.Distance(pOwnerEntity.GetOrigin(), pUserEntity.GetOrigin()) > REACH)
		{
			LF_Debug.Warn(LF_CallOutcome.FEATURE, "radio close refused: user is not at the car");
			return;
		}

		// The character key, the same key the board's take stored on the call (LF_PoliceCallBoardAction.PerformAction).
		string officerUid = LF_Utils.GetPlayerUID(pUserEntity);
		if (officerUid.IsEmpty())
		{
			LF_Debug.Error(LF_CallOutcome.FEATURE, "radio close refused: no player behind the user");
			return;
		}

		string key, arg;
		ServerClose(officerUid, LF_AccountReplication.GetState(pUserEntity), m_eOutcome, pUserEntity.GetOrigin(), key, arg);
		LF_Utils.NotifyEntity(pUserEntity, key, "#LF-Police_CallTitle", 5.0, arg);
	}
}
