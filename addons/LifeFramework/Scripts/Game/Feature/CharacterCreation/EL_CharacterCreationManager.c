class EL_CharacterCreationManager : Managed
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
			// Has account but no characters; offer faction selection until the player has made
			// an explicit choice (CIVILIAN is both the default and a valid pick).
			if (!account.WasFactionChosen())
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
		// Spawn the real character prefab (carries the EL ATM/survival components) at a spawn
		// point, mirroring EL_SpawnLogic.CreateCharacter's transform setup.
		ResourceName tempPrefab = "{9B5BB216CC7FF18E}Prefabs/Characters/Core/Character_Roleplay.et";
		vector position, yawPitchRoll;
		if (!ResolveSpawnPosition(position, yawPitchRoll))
			return;

		EntitySpawnParams spawnParams();
		spawnParams.TransformMode = ETransformMode.WORLD;
		Math3D.AnglesToMatrix(yawPitchRoll, spawnParams.Transform);
		spawnParams.Transform[3] = position + "0 0.1 0";
		IEntity spawnedEntity = GetGame().SpawnEntityPrefab(Resource.Load(tempPrefab), GetGame().GetWorld(), spawnParams);
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
		// There is no dedicated lobby; park the player at a valid spawn point while the creation
		// menu is open (same source EL_SpawnLogic.GetCreationPosition uses).
		IEntity entity = playerController.GetControlledEntity();
		if (!entity)
			return;

		vector position, yawPitchRoll;
		if (!ResolveSpawnPosition(position, yawPitchRoll))
			return;

		entity.SetOrigin(position);
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
		ResourceName defaultPrefab = "{9B5BB216CC7FF18E}Prefabs/Characters/Core/Character_Roleplay.et";
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
				survivalComponent.Init(character.GetId());
			}
		}

		// Spawn at default location
		SpawnPlayerAtDefaultLocation(playerController);
	}

	//------------------------------------------------------------------------------------------------
	protected void SpawnPlayerAtDefaultLocation(PlayerController playerController)
	{
		// Move the player to a real spawn point (the old "0 0 0" dropped players at world origin).
		IEntity entity = playerController.GetControlledEntity();
		if (!entity)
			return;

		vector position, yawPitchRoll;
		if (!ResolveSpawnPosition(position, yawPitchRoll))
			return;

		entity.SetOrigin(position);

		// Show survival HUD
		EL_SurvivalHUD hud = EL_SurvivalHUD.Cast(GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.SurvivalHUD));
		if (hud)
		{
			hud.SetPlayerController(playerController);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Resolves a world position from the same spawn-point source EL_SpawnLogic.GetCreationPosition
	//! uses. \return false when the map has no spawn point.
	protected bool ResolveSpawnPosition(out vector position, out vector yawPitchRoll)
	{
		SCR_SpawnPoint spawnPoint = SCR_SpawnPoint.GetRandomSpawnPointDeathmatch();
		if (!spawnPoint)
			return false;

		spawnPoint.GetPositionAndRotation(position, yawPitchRoll);
		return true;
	}
};