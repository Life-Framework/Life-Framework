class EL_FactionSelectionMenu : ChimeraMenuBase
{
	protected PlayerController m_PlayerController;

	//------------------------------------------------------------------------------------------------
	void SetPlayerController(PlayerController playerController)
	{
		m_PlayerController = playerController;
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuOpen()
	{
		// Show faction selection UI
		// For now, simple buttons
	}

	//------------------------------------------------------------------------------------------------
	void OnFactionSelected(EL_Faction faction)
	{
		// Set faction in account
		EL_PlayerAccountManager accountManager = EL_PlayerAccountManager.GetInstance();
		string playerUid = EL_Utils.GetPlayerUID(m_PlayerController.GetControlledEntity());
		EL_PlayerAccount account = accountManager.GetAccount(playerUid);
		if (account)
		{
			account.SetFaction(faction);
			accountManager.SaveAndReleaseAccount(account);
		}

		// Proceed to character creation
		EL_CharacterCreationManager.GetInstance().OnPlayerConnected(m_PlayerController.GetPlayerId());

		// Close menu
		GetGame().GetMenuManager().CloseMenu(this);
	}
};