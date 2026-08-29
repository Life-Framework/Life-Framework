class EL_CharacterCreationManager : Managed
{
	protected static EL_CharacterCreationManager s_Instance;

	protected const int FLOW_POLL_DELAY_MS = 100;

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

		Print("[EL_CharacterCreationManager] Flow start for player " + playerId, LogLevel.NORMAL);

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
		Print("[EL_CharacterCreationManager] Faction menu: clearing splash", LogLevel.NORMAL);
		EnsureLoadingPlaceholderDestroyed();
		Print("[EL_CharacterCreationManager] Faction menu: opening", LogLevel.NORMAL);

		EL_FactionSelectionMenu menu = EL_FactionSelectionMenu.Cast(GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.FactionSelection));
		Print("[EL_CharacterCreationManager] Faction menu: open call returned", LogLevel.NORMAL);
		if (menu)
			menu.SetPlayerController(playerController);
		else
			Print("[EL_CharacterCreationManager] Failed to open the faction selection menu", LogLevel.ERROR);
	}

	//------------------------------------------------------------------------------------------------
	protected void ShowCharacterCreationMenu(PlayerController playerController)
	{
		EnsureLoadingPlaceholderDestroyed();

		EL_CharacterCreationMenu menu = EL_CharacterCreationMenu.Cast(GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.CharacterCreationMenu));
		if (menu)
			menu.SetPlayerController(playerController);
		else
			Print("[EL_CharacterCreationManager] Failed to open the character creation menu", LogLevel.ERROR);
	}

	//------------------------------------------------------------------------------------------------
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

		// The spawn logic resolves the account's active character, spawns it at the faction spawn
		// point and hands control over; the ATM/survival components init in its post-spawn hook.
		SpawnPlayer(playerController.GetPlayerId(), playerController);
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns the account's active character through the spawn logic and opens the survival HUD.
	protected void SpawnPlayer(int playerId, PlayerController playerController)
	{
		SCR_RespawnSystemComponent respawnSystem = SCR_RespawnSystemComponent.GetInstance();
		EL_SpawnLogic spawnLogic;
		if (respawnSystem)
			spawnLogic = EL_SpawnLogic.Cast(respawnSystem.GetSpawnLogic());

		if (!spawnLogic)
		{
			Print("[EL_CharacterCreationManager] No EL_SpawnLogic on the respawn system, cannot spawn player " + playerId, LogLevel.ERROR);
			return;
		}

		Print("[EL_CharacterCreationManager] Spawning player " + playerId, LogLevel.NORMAL);
		spawnLogic.SpawnPlayer_S(playerId);

		// NOTE: the survival HUD is deliberately not opened here. Opened via OpenMenu it is a
		// full-screen menu that captures input and traps the player; it needs to be integrated as
		// a proper HUD element before it can come back.
	}

	//------------------------------------------------------------------------------------------------
	//! The respawn system's splash screen covers every menu until told otherwise, and the audit
	//! chain that normally clears it never runs for the local player of an offline session.
	protected void EnsureLoadingPlaceholderDestroyed()
	{
		SCR_RespawnSystemComponent respawnSystem = SCR_RespawnSystemComponent.GetInstance();
		if (respawnSystem)
			respawnSystem.DestroyLoadingPlaceholder();
	}
};
