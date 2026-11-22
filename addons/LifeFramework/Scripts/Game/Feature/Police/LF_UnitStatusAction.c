//------------------------------------------------------------------------------------------------
//! The police car's radio rows (PO-D3, #4204): "Radio: available" and "Radio: busy". The prompt reads
//! the replicated duty bit; the server re-checks the officer's real account and the reach before it
//! sets the status (LF_UnitStatus), which every officer's dispatch board then shows.
class LF_UnitStatusAction : ScriptedUserAction
{
	[Attribute("0", UIWidgets.CheckBox, "On: this row radios 'busy'. Off: it radios 'available'.")]
	protected bool m_bBusy;

	//! How far from the car the server accepts a radio press, in metres (inside the cab or at the door).
	static const float REACH = 6.0;

	//------------------------------------------------------------------------------------------------
	bool IsBusyRow()
	{
		return m_bBusy;
	}

	//------------------------------------------------------------------------------------------------
	LF_EUnitStatus Status()
	{
		if (m_bBusy)
			return LF_EUnitStatus.BUSY;

		return LF_EUnitStatus.AVAILABLE;
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
		if (m_bBusy)
			outName = "#LF-UnitStatus_ActionBusy";
		else
			outName = "#LF-UnitStatus_ActionAvailable";

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (LF_Systems.Refuses("Police", "LF_UnitStatusAction"))
			return;

		if (!Replication.IsServer() || !pOwnerEntity || !pUserEntity)
			return;

		if (vector.Distance(pOwnerEntity.GetOrigin(), pUserEntity.GetOrigin()) > REACH)
		{
			LF_Debug.Warn(LF_UnitStatus.FEATURE, "radio refused: user is not at the car");
			return;
		}

		// Keyed by the account UID, the key the clock-out and player-left hooks clear (any character slot).
		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		LF_PlayerAccount account;
		if (accounts)
			account = accounts.GetFromCache(LF_Utils.GetPlayerUID(pUserEntity));	// a character key resolves to its account

		string uid;
		if (account)
			uid = account.GetPersistentId();

		if (!LF_UnitStatus.Set(uid, LF_Utils.GetPlayerName(pUserEntity), Status(), LF_PoliceUtils.IsOnDutyOfficer(account)))
			return;

		string message = "#LF-UnitStatus_SetAvailable";
		if (m_bBusy)
			message = "#LF-UnitStatus_SetBusy";

		LF_Utils.NotifyEntity(pUserEntity, message, "#LF-UnitStatus_Title", 4.0);
	}
}
