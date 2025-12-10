class EL_Utils : EPF_Utils
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
			string uid = GetGame().GetBackendApi().GetPlayerIdentityId(playerId);
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

		EPF_PersistenceManager persistenceManager = EPF_PersistenceManager.GetInstance();
		if (persistenceManager)
		{
			IEntity resultEntity = persistenceManager.FindEntityByPersistentId(characterId);
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
	//! Get player UID - returns the PERSISTENT CHARACTER ID, not the Steam UID
	static override string GetPlayerUID(IEntity player)
	{
		if (!player)
			return "";
            
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(player);
		if (!character)
			return "";
        
		// Get the persistent ID from EPF
		EPF_PersistenceComponent persistence = EPF_Component<EPF_PersistenceComponent>.Find(character);
		if (persistence)
			return persistence.GetPersistentId();
        
		// Fallback to Steam UID if no persistence component (shouldn't happen in normal gameplay)
		return GetCharacterId(player);
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
			string uid = GetGame().GetBackendApi().GetPlayerIdentityId(playerId);
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
            
		string uid = GetGame().GetBackendApi().GetPlayerIdentityId(playerId);
		return uid;
	}

	//------------------------------------------------------------------------------------------------
	//! Get player name (wrapper for GetCharacterName)
	static string GetPlayerName(IEntity player)
	{
		return GetCharacterName(player);
	}
};
