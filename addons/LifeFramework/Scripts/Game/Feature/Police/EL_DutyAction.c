class EL_DutyAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		EL_PlayerAccount account = GetPlayerAccount(user);
		if (!account || account.GetFaction() != EL_Faction.POLICE)
		{
			SetCannotPerformReason("Only police can toggle duty");
			return false;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		EL_PlayerAccount account = GetPlayerAccount(pUserEntity);
		if (account)
		{
			bool newDuty = !account.IsOnDuty();
			account.SetOnDuty(newDuty);
			
			string status;
			if (newDuty)
				status = "on duty";
			else
				status = "off duty";

			EL_Utils.Notify(string.Format("#EL-Duty_Status_Change", status), "#EL-Duty_Status", 3.0);
			
			// Save account
			EL_PlayerAccountManager accountManager = EL_PlayerAccountManager.GetInstance();
			if (accountManager)
			{
				accountManager.SaveAndReleaseAccount(account);
			}
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
				outName = "Go Off Duty";
			else
				outName = "Go On Duty";
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