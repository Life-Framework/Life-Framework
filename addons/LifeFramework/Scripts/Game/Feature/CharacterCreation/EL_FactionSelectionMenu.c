class EL_FactionSelectionMenu : ChimeraMenuBase
{
	protected ref PlayerController m_PlayerController;

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
		if (!m_PlayerController)
			return;

		// Set faction in account
		EL_PlayerAccountManager accountManager = EL_PlayerAccountManager.GetInstance();
		if (!accountManager)
			return;
		IEntity entity = m_PlayerController.GetControlledEntity();
		if (!entity)
			return;
		string playerUid = EL_Utils.GetPlayerUID(entity);
		EL_PlayerAccount account = accountManager.GetAccount(playerUid);
		if (account)
		{
			account.SetFaction(faction);
			accountManager.SaveAndReleaseAccount(account);
		}

		// Proceed to character creation
		EL_CharacterCreationManager manager = EL_CharacterCreationManager.GetInstance();
		if (manager)
			manager.OnPlayerConnected(m_PlayerController.GetPlayerId());

		// Close menu
		GetGame().GetMenuManager().CloseMenu(this);
	}
};