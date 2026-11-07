class EL_DutyAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		EL_PlayerAccount account = GetPlayerAccount(user);
		if (!account || account.GetFaction() != EL_Faction.POLICE)
		{
			SetCannotPerformReason("#LF-Duty_PoliceOnly");
			return false;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		EL_PlayerAccount account = GetPlayerAccount(pUserEntity);
		if (!account)
		{
			EL_Debug.Warn("Police", string.Format("duty toggle failed: no account for %1", pUserEntity));
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

		EL_Utils.Notify(WidgetManager.Translate("#EL-Duty_Status_Change", WidgetManager.Translate(statusKey)), "#EL-Duty_Status", 3.0);
		
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

		EL_PlayerAccount account = GetPlayerAccount(entity);
		if (account)
		{
			if (account.IsOnDuty())
				outName = "#LF-Duty_SignOff";
			else
				outName = "#LF-Duty_SignOn";
			return true;
		}
		return false;
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