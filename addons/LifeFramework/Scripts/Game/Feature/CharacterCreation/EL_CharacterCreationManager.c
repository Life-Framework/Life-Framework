class EL_CharacterCreationManager : Managed
{
	protected static EL_CharacterCreationManager s_Instance;

	protected const int FLOW_POLL_DELAY_MS = 100;

	//! Strong ref: the faction picker is a plain widget controller (not a MenuManager menu) and
	//! must survive until the player clicks a faction.
	protected ref EL_FactionSelectionMenu m_FactionMenu;

	//! Same for the name/age dialog.
	protected ref EL_CharacterCreationMenu m_CreationMenu;

	//------------------------------------------------------------------------------------------------
	static EL_CharacterCreationManager GetInstance()
	{
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	void EL_CharacterCreationManager()
	{
		s_Instance = this;
	}

	//------------------------------------------------------------------------------------------------
	void ~EL_CharacterCreationManager()
	{
		s_Instance = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Entry point from the game mode. The world is still loading when this fires, so the flow
	//! starts on the first tick after the game state reaches GAME; menus opened during loading
	//! never make it on screen. The poll self-reschedules (non-repeating): removing a call from
	//! inside its own callback corrupts the callqueue.
	void OnPlayerConnected(int playerId)
	{
		GetGame().GetCallqueue().CallLater(ContinueFlow, FLOW_POLL_DELAY_MS, false, playerId);
	}

	//------------------------------------------------------------------------------------------------
	protected void ContinueFlow(int playerId)
	{
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		if (!gameMode || gameMode.GetState() != SCR_EGameModeState.GAME)
		{
			GetGame().GetCallqueue().CallLater(ContinueFlow, FLOW_POLL_DELAY_MS, false, playerId);
			return;
		}

		PlayerController playerController = GetGame().GetPlayerManager().GetPlayerController(playerId);
		if (!playerController)
			return;

		EL_PlayerAccountManager accountManager = EL_PlayerAccountManager.GetInstance();
		if (!accountManager)
			return;

		// Resolved here rather than at connect: the platform identity is not guaranteed to be
		// available yet while the world is loading, and every consumer of the account must key
		// on the same UID the spawn logic uses.
		string playerUid = EL_Utils.GetPlayerUID(playerId);
		EL_PlayerAccount account = accountManager.GetAccount(playerUid);
		if (!account)
		{
			account = EL_PlayerAccount.Create(playerUid);
			accountManager.AddAccount(account);
		}

		EL_Debug.Info("CharacterCreation", "flow start for player " + playerId);

		if (!account.HasCharacters())
		{
			// Offer faction selection until the player made an explicit choice (CIVILIAN is both
			// the default and a valid pick).
			if (!account.WasFactionChosen())
			{
				ShowFactionSelectionMenu(playerController);
				return;
			}

			ShowCharacterCreationMenu(playerController);
			return;
		}

		SpawnPlayer(playerId, playerController);
	}

	//------------------------------------------------------------------------------------------------
	protected void ShowFactionSelectionMenu(PlayerController playerController)
	{
		// The respawn system's splash screen covers every screen until told otherwise, and the
		// audit chain that normally clears it never runs for the local player of an offline
		// session.
		SCR_RespawnSystemComponent respawnSystem = SCR_RespawnSystemComponent.GetInstance();
		if (respawnSystem)
			respawnSystem.DestroyLoadingPlaceholder();

		m_FactionMenu = EL_FactionSelectionMenu.Open(playerController);
		if (!m_FactionMenu)
			EL_Debug.Error("CharacterCreation", "failed to open the faction selection menu");
	}

	//------------------------------------------------------------------------------------------------
	protected void ShowCharacterCreationMenu(PlayerController playerController)
	{
		SCR_RespawnSystemComponent respawnSystem = SCR_RespawnSystemComponent.GetInstance();
		if (respawnSystem)
			respawnSystem.DestroyLoadingPlaceholder();

		m_CreationMenu = EL_CharacterCreationMenu.Open(playerController);
		if (!m_CreationMenu)
			EL_Debug.Error("CharacterCreation", "failed to open the character creation menu");
	}

	//------------------------------------------------------------------------------------------------
	//! Called by the creation menu when the player submitted a valid name/age pair.
	//! Creates the persistent character, spawns it through the spawn logic (which picks the faction
	//! spawn point, hands control over and clears the splash) - the ATM/survival components init in
	//! its post-spawn hook.
	void OnCharacterCreated(PlayerController playerController, string firstName, string lastName, int age)
	{
		ResourceName defaultPrefab = "{9B5BB216CC7FF18E}Prefabs/Characters/Core/Character_Roleplay.et";
		EL_PlayerCharacter character = EL_PlayerCharacter.Create(defaultPrefab, firstName, lastName, age);

		EL_PlayerAccountManager accountManager = EL_PlayerAccountManager.GetInstance();
		string playerUid = EL_Utils.GetPlayerUID(playerController.GetPlayerId());
		EL_PlayerAccount account = accountManager.GetAccount(playerUid);
		if (!account)
		{
			account = EL_PlayerAccount.Create(playerUid);
			accountManager.AddAccount(account);
		}
		account.AddCharacter(character, true);

		SpawnPlayer(playerController.GetPlayerId(), playerController);
	}