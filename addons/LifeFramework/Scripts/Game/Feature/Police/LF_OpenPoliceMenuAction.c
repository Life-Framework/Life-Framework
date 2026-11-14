class LF_OpenPoliceMenuAction : ScriptedUserAction
{
	//! The Police system is off (#1076): no prompt.
	override bool CanBeShownScript(IEntity user)
	{
		return LF_Systems.IsEnabled("Police");
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		// Same rule as LF_PoliceRanks.CanViewWantedList, read from the replicated account state so
		// a dedicated-server client can open it. The server re-checks on the list request.
		if (!LF_AccountStateBits.IsOnDutyPolice(LF_AccountReplication.GetState(user)))
		{
			SetCannotPerformReason("#EL-Rank_WantedListDenied");
			return false;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The menu is local UI: run PerformAction only on the acting client. Without this a listen
	//! host also runs it for every other player and opens the menu on the host screen. Server
	//! work goes through the menu RPCs, which the server validates.
	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (LF_Systems.Refuses("Police", "LF_OpenPoliceMenuAction"))
			return;

		LF_PoliceMenu menu = LF_PoliceMenu.Cast(GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.PoliceMenu));
		if (menu)
		{
			PlayerController pc = GetGame().GetPlayerController();
			menu.SetPlayerController(pc);
		}
	}
};