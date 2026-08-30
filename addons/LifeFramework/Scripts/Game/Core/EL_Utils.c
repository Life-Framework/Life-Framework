class EL_Utils : Managed
{
	//------------------------------------------------------------------------------------------------
	static int MaxInt(int a, int b)
	{
		if (a > b) return a;
		return b;
	}

	//------------------------------------------------------------------------------------------------
	static int MinInt(int a, int b)
	{
		if (a < b) return a;
		return b;
	}

	protected static ref array<IEntity> s_aNearbyEntities = {};
	protected static string s_sRequiredComponent = "";

	//------------------------------------------------------------------------------------------------
	static array<IEntity> GetNearbyVehicles(vector center, float radius, string requiredComponent = "")
	{
		s_aNearbyEntities = new array<IEntity>();
		s_sRequiredComponent = requiredComponent;
		GetGame().GetWorld().QueryEntitiesBySphere(center, radius, InsertNearbyEntity, FilterVehicleEntity);
		return s_aNearbyEntities;
	}

	//------------------------------------------------------------------------------------------------
	static array<IEntity> GetNearbyCharacters(vector center, float radius)
	{
		s_aNearbyEntities = new array<IEntity>();
		s_sRequiredComponent = "";
		GetGame().GetWorld().QueryEntitiesBySphere(center, radius, InsertNearbyEntity, FilterCharacterEntity);
		return s_aNearbyEntities;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool InsertNearbyEntity(IEntity entity)
	{
		if (!entity)
			return true;

		s_aNearbyEntities.Insert(entity);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool FilterVehicleEntity(IEntity entity)
	{
		if (!entity || !entity.IsInherited(Vehicle))
			return false;

		if (s_sRequiredComponent.IsEmpty())
			return true;

		typename componentType = s_sRequiredComponent.ToType();
		if (!componentType)
			return true;

		return entity.FindComponent(componentType) != null;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool FilterCharacterEntity(IEntity entity)
	{
		return !!SCR_ChimeraCharacter.Cast(entity);
	}

	//------------------------------------------------------------------------------------------------
	static void Notify(string message, string title = "", float duration = 3.0)
	{
		SCR_HintManagerComponent.ShowCustomHint(message, title, duration);
	}

	//------------------------------------------------------------------------------------------------
	// Convenience: alert police (global notify for now)
	static void AlertPolice(string message, vector pos)
	{
		// For now, broadcast a police alert notification. Position currently unused.
		Notify(message, "Police Alert", 5.0);
	}

	//------------------------------------------------------------------------------------------------
	static SCR_ChimeraCharacter GetLocalCharacter()
	{
		PlayerController controller = GetGame().GetPlayerController();
		if (!controller)
			return null;

		return SCR_ChimeraCharacter.Cast(controller.GetControlledEntity());
	}

//------------------------------------------------------------------------------------------------
	static string GetCharacterId(SCR_ChimeraCharacter character)
	{
		if (!character)
			return "";

		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return "";

		int playerId = playerManager.GetPlayerIdFromControlledEntity(character);
		if (playerId > 0)
		{
			// Get the actual Steam UID, not just the player ID
			string uid = SCR_PlayerIdentityUtils.GetPlayerIdentityId(playerId);
			if (!uid.IsEmpty())
				return uid;
		}

		return "";
	}

//------------------------------------------------------------------------------------------------
	static string GetCharacterId(IEntity entity)
	{
		return GetCharacterId(SCR_ChimeraCharacter.Cast(entity));
	}

//------------------------------------------------------------------------------------------------
	static string GetCharacterName(SCR_ChimeraCharacter character)
	{
		if (!character)
			return "";

		CharacterIdentityComponent identityComp = CharacterIdentityComponent.Cast(character.FindComponent(CharacterIdentityComponent));
		if (identityComp)
		{
			Identity identity = identityComp.GetIdentity();
			if (identity && identity.GetFullName())
				return identity.GetFullName();
		}

		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (playerManager)
		{
			int playerId = playerManager.GetPlayerIdFromControlledEntity(character);
			if (playerId > 0)
			{
				string playerName = playerManager.GetPlayerName(playerId);
				if (playerName && !playerName.IsEmpty())
					return playerName;
			}
		}

		return "";
	}

//------------------------------------------------------------------------------------------------
	static string GetCharacterName(IEntity entity)
	{
		return GetCharacterName(SCR_ChimeraCharacter.Cast(entity));
	}

//------------------------------------------------------------------------------------------------
	static bool IsCharacterLocal(SCR_ChimeraCharacter character)
	{
		if (!character)
			return false;

		RplComponent rplComponent = RplComponent.Cast(character.FindComponent(RplComponent));
		if (!rplComponent)
			return false;

		return rplComponent.IsOwner();
	}

//------------------------------------------------------------------------------------------------
	static bool IsCharacterLocal(IEntity entity)
	{
		return IsCharacterLocal(SCR_ChimeraCharacter.Cast(entity));
	}

	//------------------------------------------------------------------------------------------------
	static SCR_ChimeraCharacter FindCharacterById(string characterId)
	{
		if (!characterId || characterId.IsEmpty())
			return null;

		// First-party lookup: a tracked instance by persistence id.
		SCR_PersistenceSystem persistence = SCR_PersistenceSystem.GetScriptedInstance();
		if (persistence)
		{
			UUID characterUuid = characterId;
			Managed instance = persistence.FindById(characterUuid);
			IEntity resultEntity = IEntity.Cast(instance);
			if (resultEntity)
				return SCR_ChimeraCharacter.Cast(resultEntity);
		}

		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return null;

		array<int> playerIds = {};
		playerManager.GetPlayers(playerIds);
		foreach (int playerId : playerIds)
		{
			IEntity entity = playerManager.GetPlayerControlledEntity(playerId);
			SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(entity);
		if (character && GetCharacterId(character) == characterId)
				return character;
		}

		return null;
	}
    
	//------------------------------------------------------------------------------------------------
	//! Recursively change color on entity and all children using material parameters
	static void ChangeColorRecursive(IEntity entity, Color color)
	{
		if (!entity)
			return;
        
		// Try to set color via signals manager (for vehicles with paintable surfaces)
		SignalsManagerComponent signalsManager = SignalsManagerComponent.Cast(entity.FindComponent(SignalsManagerComponent));
		if (signalsManager)
		{
			// Common vehicle color signals
			int signalR = signalsManager.AddOrFindSignal("VehicleColorR");
			int signalG = signalsManager.AddOrFindSignal("VehicleColorG");
			int signalB = signalsManager.AddOrFindSignal("VehicleColorB");
            
			if (signalR >= 0)
				signalsManager.SetSignalValue(signalR, color.R() / 255.0);
			if (signalG >= 0)
				signalsManager.SetSignalValue(signalG, color.G() / 255.0);
			if (signalB >= 0)
				signalsManager.SetSignalValue(signalB, color.B() / 255.0);
		}
        
		// Apply to children recursively
		IEntity child = entity.GetChildren();
		while (child)
		{
			ChangeColorRecursive(child, color);
			child = child.GetSibling();
		}
	}
    
	//------------------------------------------------------------------------------------------------
	//! Set color on vehicle slots
	static void SetSlotsColor(IEntity vehicle, int colorInt)
	{
		if (!vehicle)
			return;
        
		Color color = Color.FromInt(colorInt);
        
		// Find slot manager
		SlotManagerComponent slotManager = SlotManagerComponent.Cast(vehicle.FindComponent(SlotManagerComponent));
		if (!slotManager)
			return;
        
		// Apply color to all slotted entities
		array<EntitySlotInfo> slots = {};
		slotManager.GetSlotInfos(slots);
        
		foreach (EntitySlotInfo slotInfo : slots)
		{
			IEntity slottedEntity = slotInfo.GetAttachedEntity();
			if (slottedEntity)
			{
				ChangeColorRecursive(slottedEntity, color);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Gets the platform-stable identity id of a player (the player's Steam/Bohemia UID).
	//! \param playerId Index of the player inside player manager
	//! \return the uid as string
	static string GetPlayerUID(int playerId)
	{
		if (playerId <= 0)
			return "";

		string identity = SCR_PlayerIdentityUtils.GetPlayerIdentityId(playerId);

		// The backend can transiently answer the per-player lookup with the NULL UUID
		// ("00000000-...") instead of an empty string - it is non-empty, so it defeats both
		// vanilla's name-hash fallback and the empty-identity guards in every caller.
		// On a non-dedicated session (offline Workbench play / listen host) there is no
		// backend to ever resolve it, so fall back to a name-derived identity like vanilla's
		// peer-tool polyfill; without it the spawn flow retries forever. Dedicated servers
		// keep the strict path - the backend must provide the real identity.
		UUID identityUuid = identity;
		if (identity.IsEmpty() || identityUuid.IsNull())
		{
			if (RplSession.Mode() == RplMode.Dedicated)
				return "";

			return BuildLocalIdentity(playerId);
		}

		return identity;
	}

	//------------------------------------------------------------------------------------------------
	//! Derives a stable per-player identity from the player name when no backend identity
	//! exists (offline sessions). Mirrors vanilla's peer-tool polyfill so the result matches
	//! what vanilla would produce for the same name.
	//! \param playerId Index of the player inside player manager
	//! \return a stable identity string, never empty
	static string BuildLocalIdentity(int playerId)
	{
		string playerName = GetGame().GetPlayerManager().GetPlayerName(playerId);
		if (playerName.IsEmpty())
			playerName = "LocalPlayer" + playerId;

		const int splitLength = Math.Max(1, playerName.Length() / 3);
		const string split1 = Math.AbsInt(playerName.Substring(0, splitLength).Hash()).ToString(8, true);
		const string split2 = Math.AbsInt(playerName.Substring(splitLength, splitLength).Hash()).ToString(8, true);
		const int doubleSplit = splitLength * 2;
		const string split3 = Math.AbsInt(playerName.Substring(doubleSplit, playerName.Length() - doubleSplit).Hash()).ToString(8, true);

		string uid = string.Format("00bbbddd-%1-%2-%3-%4%5", split1.Substring(0, 4), split1.Substring(4, 4), split2.Substring(0, 4), split2.Substring(4, 4), split3);
		return uid;
	}

	//------------------------------------------------------------------------------------------------
	//! Gets the platform-stable identity id of a player.
	//! \param player Instance of the player
	//! \return the uid as string
	static string GetPlayerUID(IEntity player)
	{
		if (!player)
			return "";

		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return "";

		int playerId = playerManager.GetPlayerIdFromControlledEntity(player);
		if (playerId <= 0)
			return "";

		return GetPlayerUID(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Compatibility alias: some code uses GetPlayerUid (lowercase d)
	static string GetPlayerUid(IEntity player)
	{
		return GetPlayerUID(player);
	}

	//------------------------------------------------------------------------------------------------
	//! Get player Steam UID (not character ID) - for account lookups
	static string GetPlayerSteamUID(IEntity player)
	{
		if (!player)
			return "";
            
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return "";
        
		int playerId = playerManager.GetPlayerIdFromControlledEntity(player);
		if (playerId > 0)
		{
			string uid = SCR_PlayerIdentityUtils.GetPlayerIdentityId(playerId);
			if (!uid.IsEmpty())
				return uid;
		}
        
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Get player Steam UID from player ID - for account lookups
	static string GetPlayerSteamUID(int playerId)
	{
		if (playerId <= 0)
			return "";
            
		string uid = SCR_PlayerIdentityUtils.GetPlayerIdentityId(playerId);
		return uid;
	}

	//------------------------------------------------------------------------------------------------
	//! Get player name (wrapper for GetCharacterName)
	static string GetPlayerName(IEntity player)
	{
		return GetCharacterName(player);
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns a prefab
	//! \param prefab ResournceName of the prefab to be spawned
	//! \param origin Position(origin) where to spawn the entity
	//! \param orientation Angles(yaw, pitch, rolle in degrees) to apply to the entity
	//! \param global If true the entity is spawned in the global (networked) space
	//! \param parent Optional parent entity to attach the spawned entity to
	//! \return the spawned entity or null on failure
	static IEntity SpawnEntityPrefab(ResourceName prefab, vector origin, vector orientation = "0 0 0", bool global = true, IEntity parent = null)
	{
		EntitySpawnParams spawnParams();
		spawnParams.TransformMode = ETransformMode.WORLD;
		Math3D.AnglesToMatrix(orientation, spawnParams.Transform);
		spawnParams.Transform[3] = origin;
		spawnParams.Parent = parent;

		Resource resource = Resource.Load(prefab);
		if (!resource || !resource.IsValid())
		{
			EL_Debug.Error("Utils", string.Format("spawn failed: prefab does not resolve: %1", prefab));
			return null;
		}

		if (!global)
			return GetGame().SpawnEntityPrefabLocal(resource, GetGame().GetWorld(), spawnParams);

		return GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), spawnParams);
	}

	//------------------------------------------------------------------------------------------------
	//! Remaps every material on the entity's current mesh to the given material
	//! \param entity Entity whose mesh materials are replaced
	//! \param newMaterial ResourceName of the material to remap to
	static void ChangeEntityMaterial(IEntity entity, ResourceName newMaterial)
	{
		if (!entity || !newMaterial)
			return;

		VObject mesh = entity.GetVObject();
		if (!mesh)
			return;

		string remap;
		string materials[256];
		int numMats = mesh.GetMaterials(materials);
		if (numMats == 0)
			return;

		for (int i = 0; i < numMats; i++)
		{
			remap += string.Format("$remap '%1' '%2';", materials[i], newMaterial);
		}

		entity.SetObject(mesh, remap);
	}

	//------------------------------------------------------------------------------------------------
	//! Reads the MeshObject component of a prefab source (no instance needed)
	//! \param prefab Prefab to inspect
	//! \return the MeshObject BaseContainer or null when the prefab has none
	static BaseContainer GetPrefabMeshComponent(ResourceName prefab)
	{
		BaseContainer meshComponent;
		if (prefab.IsEmpty())
			return null;

		Resource resource = Resource.Load(prefab);
		if (!resource || !resource.IsValid())
			return null;

		IEntitySource prefabSource = resource.GetResource().ToEntitySource();
		if (!prefabSource)
			return null;

		int count = prefabSource.GetComponentCount();

		for (int i = 0; i < count; i++)
		{
			IEntityComponentSource comp = prefabSource.GetComponent(i);

			if (comp.GetClassName() == "MeshObject")
			{
				meshComponent = comp;
				break;
			}
		}
		return meshComponent;
	}

	//------------------------------------------------------------------------------------------------
	//! Gets the prefab the entity uses
	//! \param entity Instance of which to get the prefab name
	//! \return the resource name of the prefab or empty string if no prefab was used or entity is invalid
	static ResourceName GetPrefabName(IEntity entity)
	{
		if (!entity) return ResourceName.Empty;

		EntityPrefabData prefabData = entity.GetPrefabData();
		if (!prefabData) return ResourceName.Empty;

		return SCR_BaseContainerTools.GetPrefabResourceName(prefabData.GetPrefab());
	}

	//------------------------------------------------------------------------------------------------
	static bool IsAnyInherited(typename type, notnull array<typename> from)
	{
		if (type)
		{
			foreach (typename candiate : from)
			{
				if (type.IsInherited(candiate))
					return true;
			}
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsInstanceAnyInherited(Managed instance, notnull array<typename> from)
	{
		if (!instance)
			return false;

		typename type = instance.Type();
		return IsAnyInherited(type, from);
	}
};