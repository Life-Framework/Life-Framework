class EL_OpenPoliceMenuAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		EL_PlayerAccount account = GetPlayerAccount(user);
		if (!account || account.GetFaction() != EL_Faction.POLICE || !account.IsOnDuty())
		{
			SetCannotPerformReason("Only on-duty police can access police menu");
			return false;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		EL_PoliceMenu menu = EL_PoliceMenu.Cast(GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.PoliceMenu));
		if (menu)
		{
			PlayerController pc = GetGame().GetPlayerController();
			menu.SetPlayerController(pc);
		}
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