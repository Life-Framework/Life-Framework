class EL_CharacterCreationManager : ScriptedUserAction
{
	protected static EL_CharacterCreationManager s_Instance;

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
	void OnPlayerConnected(int playerId)
	{
		PlayerController playerController = GetGame().GetPlayerController();
		if (!playerController)
			return;

		// Check if player has account
		EL_PlayerAccountManager accountManager = EL_PlayerAccountManager.GetInstance();
		if (!accountManager)
			return;

		string playerUid = EL_Utils.GetPlayerUID(playerController.GetControlledEntity());
		EL_PlayerAccount account = accountManager.GetAccount(playerUid);

		if (!account)
		{
			// No account, create and show faction selection
			account = EL_PlayerAccount.Create(playerUid);
			accountManager.AddAccount(account);
			ShowFactionSelectionMenu(playerController);
		}
		else if (!account.HasCharacters())
		{
			// Has account but no characters, check faction
			if (account.GetFaction() == EL_Faction.CIVILIAN) // Default, assume not selected
			{
				ShowFactionSelectionMenu(playerController);
			}
			else
			{
				StartCharacterCreationFlow(playerController);
			}
		}
		else
		{
			// Has characters, spawn normally
			SpawnPlayerAtDefaultLocation(playerController);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void StartCharacterCreationFlow(PlayerController playerController)
	{
		// Create temporary character
		CreateTemporaryCharacter(playerController);

		// Teleport to lobby
		TeleportToLobby(playerController);

		// Show creation menu
		ShowCharacterCreationMenu(playerController);
	}

	//------------------------------------------------------------------------------------------------
	protected void CreateTemporaryCharacter(PlayerController playerController)
	{
		// Create a temporary entity for the player in lobby
		// This is a placeholder - implement possession according to your engine API
		ResourceName tempPrefab = "{YourTempCharacterPrefab}"; // Replace with actual prefab
		IEntity spawnedEntity = GetGame().SpawnEntityPrefab(Resource.Load(tempPrefab));
		if (!spawnedEntity)
		{
			Print("Failed to spawn temporary character prefab.", LogLevel.ERROR);
		}
		// NOTE: possession API differs between platforms. If you have an API to
		// set the player's controlled entity, perform it here (e.g. playerController.PossessEntity(spawnedEntity)).
	}

	//------------------------------------------------------------------------------------------------
	protected void TeleportToLobby(PlayerController playerController)
	{
		// Teleport to lobby position
		vector lobbyPos = "1000 0 1000"; // Replace with actual lobby coordinates
		playerController.GetControlledEntity().SetOrigin(lobbyPos);
	}

	//------------------------------------------------------------------------------------------------
	protected void ShowFactionSelectionMenu(PlayerController playerController)
	{
		// Open the faction selection menu
		EL_FactionSelectionMenu menu = EL_FactionSelectionMenu.Cast(GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.FactionSelection));
		if (menu)
		{
			menu.SetPlayerController(playerController);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void ShowCharacterCreationMenu(PlayerController playerController)
	{
		EL_CharacterCreationMenu menu = EL_CharacterCreationMenu.Cast(GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.CharacterCreationMenu));
		if (menu)
		{
			menu.SetPlayerController(playerController);
		}
	}

	//------------------------------------------------------------------------------------------------
	void OnCharacterCreated(PlayerController playerController, string firstName, string lastName, int age)
	{
		// Create persistent character
		ResourceName defaultPrefab = "{YourDefaultCharacterPrefab}"; // Replace with actual prefab
		EL_PlayerCharacter character = EL_PlayerCharacter.Create(defaultPrefab, firstName, lastName, age);

		// Add to account
		EL_PlayerAccountManager accountManager = EL_PlayerAccountManager.GetInstance();
		string playerUid = EL_Utils.GetPlayerUID(playerController.GetControlledEntity());
		EL_PlayerAccount account = accountManager.GetAccount(playerUid);
		if (!account)
		{
			account = EL_PlayerAccount.Create(playerUid);
			accountManager.AddAccount(account);
		}
		account.AddCharacter(character, true);

		// Initialize ATM component on the entity
		IEntity entity = playerController.GetControlledEntity();
		if (entity)
		{
			EL_CharacterATMComponent atmComponent = EL_CharacterATMComponent.Cast(entity.FindComponent(EL_CharacterATMComponent));
			if (atmComponent)
			{
				atmComponent.Init(playerUid);
			}

			EL_CharacterSurvivalComponent survivalComponent = EL_CharacterSurvivalComponent.Cast(entity.FindComponent(EL_CharacterSurvivalComponent));
			if (survivalComponent)
			{
				survivalComponent.Init(character.GetId(), EL_SurvivalInitCallback.Create(survivalComponent));
			}
		}

		// Spawn at default location
		SpawnPlayerAtDefaultLocation(playerController);
	}

	//------------------------------------------------------------------------------------------------
	protected void SpawnPlayerAtDefaultLocation(PlayerController playerController)
	{
		// Teleport to default spawn
		vector spawnPos = "0 0 0"; // Replace with actual spawn coordinates
		playerController.GetControlledEntity().SetOrigin(spawnPos);

		// Show survival HUD
		EL_SurvivalHUD hud = EL_SurvivalHUD.Cast(GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.SurvivalHUD));
		if (hud)
		{
			hud.SetPlayerController(playerController);
		}
	}
};

class EL_SurvivalInitCallback : EDF_DataCallbackSingle<EL_SurvivalStats>
{
	//------------------------------------------------------------------------------------------------
	override void OnComplete(EL_SurvivalStats data, Managed context)
	{
		EL_CharacterSurvivalComponent comp = EL_CharacterSurvivalComponent.Cast(context);
		if (comp)
		{
			comp.SetSurvivalStats(data);
		}
	}

	//------------------------------------------------------------------------------------------------
	static EL_SurvivalInitCallback Create(EL_CharacterSurvivalComponent comp)
	{
		EL_SurvivalInitCallback instance();
		instance.m_pContext = comp;
		return instance;
	}
};