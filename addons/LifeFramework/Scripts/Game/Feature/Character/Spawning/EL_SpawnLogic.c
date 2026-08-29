[BaseContainerProps(category: "Respawn")]
class EL_SpawnLogic : SCR_SpawnLogic
{
	[Attribute(category: "New character defaults")]
	protected ref array<ResourceName> m_aDefaultCharacterPrefabs;

	[Attribute(category: "New character defaults")]
	protected ref array<ref EL_DefaultLoadoutItem> m_aDefaultCharacterItems;

	//------------------------------------------------------------------------------------------------
	//! Account manager owns player data, not vanilla collections. Leaving the vanilla Player/Character
	//! collections unresolved keeps SCR_SpawnLogic.RequestPlayerData_S on its synchronous no-persistence
	//! path; without this, an ACTIVE persistence system reroutes player spawn through an async
	//! RequestLoad and DoSpawn_S never runs.
	override protected void SetupPersistenceCollections(SCR_RespawnSystemComponent owner)
	{
	}

	//------------------------------------------------------------------------------------------------
	//! Starts the spawn sequence once the joining player has been audited.
	//! Vanilla's implementation stops after registering the controller with persistence, so the spawn
	//! kick-off has to be added here (the old persistence-framework base did exactly this).
	override void OnPlayerAuditSuccess_S(int playerId)
	{
		super.OnPlayerAuditSuccess_S(playerId);
		ExcuteInitialLoadOrSpawn_S(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Handles failed player registration attempts by scheduling a retry.
	protected void OnPlayerRegisterFailed(int playerId)
	{
		int delay = Math.RandomFloat(900, 1100);
		GetGame().GetCallqueue().CallLater(OnPlayerAuditSuccess_S, delay, false, playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Re-creates the player's character after death.
	//! The old persistence-framework base respawned one frame after OnPlayerKilled_S; this game mode
	//! has no respawn menu, so without this hook a dead player is stranded forever.
	//! \param playerId The player who died.
	//! \param playerEntity The dead body.
	//! \param killerEntity The killer, may be null.
	//! \param killer The instigator of the kill.
	override void OnPlayerKilled_S(int playerId, IEntity playerEntity, IEntity killerEntity, notnull Instigator killer)
	{
		super.OnPlayerKilled_S(playerId, playerEntity, killerEntity, killer);

		// Fresh character spawn (NOTE: We need to push this to next frame due to a bug where on the
		// same death frame we can not hand over a new char).
		GetGame().GetCallqueue().Call(RespawnPlayer, playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Poll of the death respawn: resolves the player's account and creates their character.
	//! \param playerId The player to respawn.
	protected void RespawnPlayer(int playerId)
	{
		string playerUid = EL_Utils.GetPlayerUID(playerId);
		if (playerUid.IsEmpty())
			return;

		EL_PlayerAccount account = EL_PlayerAccountManager.GetInstance().GetAccount(playerUid);
		if (!account)
			return;

		CreateCharacter(playerId, account);
	}

	//------------------------------------------------------------------------------------------------
	//! Implement the actual spawn behaviour.
	//! Resolves (or creates) the player's account synchronously, then creates their character from the
	//! account's active character.
	override protected void DoSpawn_S(int playerId)
	{
		string playerUid = EL_Utils.GetPlayerUID(playerId);
		if (playerUid.IsEmpty())
		{
			Print("[LifeFramework] WARNING: Persistent UID not available yet for playerId: " + playerId + ", retrying...", LogLevel.WARNING);
			OnPlayerRegisterFailed(playerId);
			return;
		}

		EL_PlayerAccount account = EL_PlayerAccountManager.GetInstance().GetOrCreate(playerUid);
		if (!account)
		{
			OnPlayerRegisterFailed(playerId);
			return;
		}

		CreateCharacter(playerId, account);
	}

	//------------------------------------------------------------------------------------------------
	//! Creates the player's character from their account and hands control of it over to them.
	//! \param playerId The player who will control the character.
	//! \param account The player's account (already resolved synchronously).
	protected void CreateCharacter(int playerId, notnull EL_PlayerAccount account)
	{
		PlayerController playerController = GetGame().GetPlayerManager().GetPlayerController(playerId);
		if (!playerController)
		{
			Print("[LifeFramework] Player " + playerId + " left before their character could be created - not spawning one", LogLevel.WARNING);
			return;
		}

		if (playerController.GetControlledEntity())
		{
			// A corpse does not count as a playable character - the death respawn path must proceed.
			IEntity controlled = playerController.GetControlledEntity();
			CharacterControllerComponent characterController = CharacterControllerComponent.Cast(controlled.FindComponent(CharacterControllerComponent));
			if (!characterController || characterController.GetLifeState() != ECharacterLifeState.DEAD)
			{
				Print("[LifeFramework] Player " + playerId + " already controls a character - not spawning another", LogLevel.WARNING);
				return;
			}
		}

		EL_PlayerCharacter activeCharacter = account.GetActiveCharacter();
		if (!activeCharacter)
		{
			// No character yet: the character-creation flow owns first spawn (faction pick, name,
			// age) and calls SpawnPlayer_S once the account has one. Auto-creating a default here
			// would bypass the faction choice entirely.
			Print("[LifeFramework] Player " + playerId + " has no character yet, waiting for the character-creation flow", LogLevel.NORMAL);
			return;
		}

		string characterPersistenceId = activeCharacter.GetId();

		ResourceName prefab = GetCreationPrefab(playerId, characterPersistenceId);
		if (!prefab)
		{
			Print("[LifeFramework] Could not resolve a character prefab for player " + playerId, LogLevel.ERROR);
			return;
		}

		vector position, yawPitchRoll;
		GetCreationPosition(playerId, characterPersistenceId, position, yawPitchRoll);

		EntitySpawnParams spawnParams();
		spawnParams.TransformMode = ETransformMode.WORLD;
		Math3D.AnglesToMatrix(yawPitchRoll, spawnParams.Transform);
		spawnParams.Transform[3] = position + "0 0.1 0"; // Anti lethal terrain clipping

		IEntity character = GetGame().SpawnEntityPrefab(Resource.Load(prefab), GetGame().GetWorld(), spawnParams);
		if (!character)
		{
			Print("[LifeFramework] Failed to spawn player character from prefab: " + prefab, LogLevel.ERROR);
			return;
		}

		OnCharacterCreated(playerId, characterPersistenceId, character);
		HandoverToPlayer(playerId, character);
	}

	//------------------------------------------------------------------------------------------------
	//! Picks the spawn position and orientation for a player character.
	protected void GetCreationPosition(int playerId, string characterPersistenceId, out vector position, out vector yawPitchRoll)
	{
		SCR_SpawnPoint spawnPoint = ResolveSpawnPoint(playerId);
		if (!spawnPoint)
		{
			Print("Could not spawn character, no spawn point on the map.", LogLevel.ERROR);
			return;
		}

		spawnPoint.GetPositionAndRotation(position, yawPitchRoll);
	}

	//------------------------------------------------------------------------------------------------
	//! Resolves the spawn point for a player's account faction, falling back to any spawn point when
	//! the map has none for that faction. Shared with the character-creation flow so both spawn paths
	//! land in the same area.
	//! \param playerId Player whose account faction drives the choice.
	//! \return Spawn point, or null when the map has no spawn point at all.
	static SCR_SpawnPoint ResolveSpawnPoint(int playerId)
	{
		EL_PlayerAccount account = EL_PlayerAccountManager.GetInstance().GetFromCache(playerId);
		if (account)
		{
			SCR_SpawnPoint factionPoint = SCR_SpawnPoint.GetRandomSpawnPointForFaction(typename.EnumToString(EL_Faction, account.GetFaction()));
			if (factionPoint)
				return factionPoint;
		}

		return SCR_SpawnPoint.GetRandomSpawnPointDeathmatch();
	}

	//------------------------------------------------------------------------------------------------
	//! \return The prefab of the player's active character, or null if none can be resolved.
	protected ResourceName GetCreationPrefab(int playerId, string characterPersistenceId)
	{
		EL_PlayerAccount account = EL_PlayerAccountManager.GetInstance().GetFromCache(playerId);
		if (!account)
			return ResourceName.Empty;

		EL_PlayerCharacter activeCharacter = account.GetActiveCharacter();
		if (!activeCharacter)
			return ResourceName.Empty;

		return activeCharacter.GetPrefab();
	}

	//------------------------------------------------------------------------------------------------
	//! Called after a player character has been created and spawned into the world.
	//! Initializes the character's inventory with the configured default loadout items.
	protected void OnCharacterCreated(int playerId, string characterPersistenceId, IEntity character)
	{
		InventoryStorageManagerComponent storageManager = EL_Component<InventoryStorageManagerComponent>.Find(character);
		foreach (EL_DefaultLoadoutItem loadoutItem : m_aDefaultCharacterItems)
		{
			if (loadoutItem.m_ePurpose != EStoragePurpose.PURPOSE_LOADOUT_PROXY)
			{
				Debug.Error(string.Format("Failed to add '%1' as default character item. Only clothing/backpack/vest etc. with purpose '%2' are allowed as top level entries.", loadoutItem.m_rPrefab, typename.EnumToString(EStoragePurpose, EStoragePurpose.PURPOSE_LOADOUT_PROXY)));
				continue;
			}

			IEntity slotEntity = SpawnDefaultCharacterItem(storageManager, loadoutItem);
			if (!slotEntity)
				continue;

			if (!storageManager.TryInsertItem(slotEntity, loadoutItem.m_ePurpose))
				SCR_EntityHelper.DeleteEntityAndChildren(slotEntity);
		}

		// The account components need their ids bound to the entity; this runs on every spawn
		// path (first spawn, death respawn) so the bindings survive a fresh character entity.
		EL_CharacterATMComponent atmComponent = EL_CharacterATMComponent.Cast(character.FindComponent(EL_CharacterATMComponent));
		if (atmComponent)
			atmComponent.Init(EL_Utils.GetPlayerUID(playerId));

		EL_CharacterSurvivalComponent survivalComponent = EL_CharacterSurvivalComponent.Cast(character.FindComponent(EL_CharacterSurvivalComponent));
		if (survivalComponent)
			survivalComponent.Init(characterPersistenceId);
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns the player's active character and hands control over. Entry point for the
	//! character-creation flow, which cannot go through the platform-audit path: it never fires
	//! for the local player of an offline Workbench session.
	//! \param playerId Player to spawn.
	void SpawnPlayer_S(int playerId)
	{
		DoSpawn_S(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Hands network ownership and control of a spawned character to a player.
	//! The old persistence-framework base's HandoverToPlayer, minus its database callback.
	//! \param playerId The player taking control.
	//! \param character The character to hand over.
	protected void HandoverToPlayer(int playerId, IEntity character)
	{
		SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
		if (!playerController)
		{
			Print("[LifeFramework] Cannot hand over character - no player controller for playerId: " + playerId, LogLevel.ERROR);
			return;
		}

		playerController.SetInitialMainEntity(character);

		SCR_BaseGameMode gamemode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		if (gamemode)
			gamemode.OnPlayerEntityChanged_S(playerId, null, character);

		SCR_RespawnComponent respawn = GetPlayerRespawnComponent_S(playerId);
		if (respawn)
			respawn.NotifySpawn(character);

		// The splash placeholder stays on top until told otherwise. The audit chain that normally
		// clears it (HandlePlayFromCamera) never runs for the local player of an offline session,
		// so every handover path clears it here.
		SCR_RespawnSystemComponent respawnSystem = SCR_RespawnSystemComponent.GetInstance();
		if (respawnSystem)
			respawnSystem.DestroyLoadingPlaceholder();
	}

	//------------------------------------------------------------------------------------------------
	//! Recursively spawn items configured in the spawn loadout to the character
	//! \param storageManager Storage manager of the character to give the items to
	//! \param loadoutItem Loadout configuration item
	//! \return parent entity of the items spawned by the configuration or null on failure
	protected IEntity SpawnDefaultCharacterItem(InventoryStorageManagerComponent storageManager, EL_DefaultLoadoutItem loadoutItem)
	{
		EntitySpawnParams spawnParams();
        spawnParams.Transform[3] = storageManager.GetOwner().GetOrigin();
		IEntity slotEntity = GetGame().SpawnEntityPrefabEx(loadoutItem.m_rPrefab, false, null, spawnParams);
		if (!slotEntity) return null;

		if (loadoutItem.m_aComponentDefaults)
		{
			foreach (EL_DefaultLoadoutItemComponent componentDefault : loadoutItem.m_aComponentDefaults)
			{
				componentDefault.ApplyTo(slotEntity);
			}
		}

		if (loadoutItem.m_aStoredItems)
		{
			array<Managed> outComponents();
			slotEntity.FindComponents(BaseInventoryStorageComponent, outComponents);

			foreach (EL_DefaultLoadoutItem storedItem : loadoutItem.m_aStoredItems)
			{
				for (int i = 0; i < storedItem.m_iAmount; i++)
				{
					IEntity spawnedItem = SpawnDefaultCharacterItem(storageManager, storedItem);

					foreach (Managed componentRef : outComponents)
					{
						BaseInventoryStorageComponent storageComponent = BaseInventoryStorageComponent.Cast(componentRef);
						if (storageComponent.GetPurpose() & storedItem.m_ePurpose)
						{
							if (!storageManager.TryInsertItemInStorage(spawnedItem, storageComponent)) continue;

							InventoryItemComponent inventoryItemComponent = InventoryItemComponent.Cast(spawnedItem.FindComponent(InventoryItemComponent));
							if (inventoryItemComponent && !inventoryItemComponent.GetParentSlot()) continue;

							break;
						}
					}
				}
			}
		}

		return slotEntity;
	}
}