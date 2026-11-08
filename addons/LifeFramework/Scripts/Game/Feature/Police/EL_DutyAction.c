//------------------------------------------------------------------------------------------------
//! Police duty toggle. Offer checks and the action name read EL_AccountReplication, which is the
//! account cache on the server and the character's replicated copy on a client. The toggle
//! itself runs on the server against the real account.
class EL_DutyAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (!EL_AccountStateBits.IsPolice(EL_AccountReplication.GetState(user)))
		{
			SetCannotPerformReason("#EL-Duty_PoliceOnly");
			return false;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		EL_PlayerAccount account = GetPlayerAccount(pUserEntity);
		if (!account)
		{
			EL_Debug.Warn("Police", string.Format("duty toggle failed: no account for %1", pUserEntity));
			return;
		}

		if (account.GetFaction() != EL_Faction.POLICE)
		{
			EL_Debug.Warn("Police", string.Format("duty toggle refused: %1 is not police", pUserEntity));
			return;
		}

		bool newDuty = !account.IsOnDuty();
		account.SetOnDuty(newDuty);
		LF_PoliceRanks.OnDutyChanged(account, newDuty);

		string status;
		string statusKey;
		if (newDuty)
		{
			status = "on duty";
			statusKey = "#LF-Duty_OnDuty";
		}
		else
		{
			status = "off duty";
			statusKey = "#LF-Duty_OffDuty";
		}

		EL_Debug.Log("Police", string.Format("duty toggled -> %1 (player %2)", status, pUserEntity));

		EL_Utils.NotifyEntity(pUserEntity, "#EL-Duty_Status_Change", "#EL-Duty_Status", 3.0, statusKey);

		// Save account
		EL_PlayerAccountManager accountManager = EL_PlayerAccountManager.GetInstance();
		if (accountManager)
		{
			accountManager.SaveAndReleaseAccount(account);
		}
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		PlayerController pc = GetGame().GetPlayerController();
		if (!pc)
			return false;
		IEntity entity = pc.GetControlledEntity();
		if (!entity)
			return false;

		int state = EL_AccountReplication.GetState(entity);
		if (!EL_AccountStateBits.HasAccount(state))
			return false;

		if (EL_AccountStateBits.IsOnDuty(state))
			outName = "#EL-Duty_GoOff";
		else
			outName = "#EL-Duty_GoOn";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected EL_PlayerAccount GetPlayerAccount(IEntity user)
	{
		EL_PlayerAccountManager accountManager = EL_PlayerAccountManager.GetInstance();
		if (accountManager)
		{
			string playerUid = EL_Utils.GetPlayerUid(user);
			return accountManager.GetAccount(playerUid);
		}
		return null;
	}
};
