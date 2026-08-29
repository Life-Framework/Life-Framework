//------------------------------------------------------------------------------------------------
//! Server whitelist manager attached to the game mode. Owns the whitelist configs,
//! enforces the optional CONNECT gate on join, and pushes flag sets to players on spawn.
//! The fork's version extended a fork-only respawn handler; this one extends the base game's
//! SCR_BaseGameModeComponent and receives the spawn hook via EL_GameModeRoleplay.OnPlayerSpawned.
[ComponentEditorProps(category: "EveronLife/Feature/Whitelist", description: "Server Whitelist Manager")]
class EL_WhitelistRespawnHandlerComponentClass : SCR_BaseGameModeComponentClass
{
}

class EL_WhitelistRespawnHandlerComponent : SCR_BaseGameModeComponent
{
	[Attribute("", UIWidgets.Object)]
	ref array<ref EL_Whitelist> m_aWhitelists;

	//------------------------------------------------------------------------------------------------
	//! Join gate: when a CONNECT whitelist is configured, the player must be on it.
	override void OnPlayerAuditSuccess(int playerId)
	{
		if (!GetGameMode() || !GetGameMode().IsMaster())
		{
			super.OnPlayerAuditSuccess(playerId);
			return;
		}

		if (WhitelistExists(EL_WhitelistType.CONNECT) && !ConnectGateLoaded())
		{
			EL_Debug.Warn("Whitelist", "CONNECT whitelist configured but its file is missing/empty - join gate disabled (fail-safe)");
			super.OnPlayerAuditSuccess(playerId);
			return;
		}

		if (WhitelistExists(EL_WhitelistType.CONNECT) && !VerifyPlayer(EL_Utils.GetPlayerUID(playerId), EL_WhitelistType.CONNECT))
		{
			EL_Debug.Log("Whitelist", string.Format("join blocked: uuid=%1 not on CONNECT whitelist", EL_Utils.GetPlayerUID(playerId)));
			GetGame().GetPlayerManager().KickPlayer(playerId, PlayerManagerKickReason.KICK, 10);
			return;
		}

		EL_Debug.Log("Whitelist", string.Format("join allowed: uuid=%1", EL_Utils.GetPlayerUID(playerId)));
		super.OnPlayerAuditSuccess(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Fail-safe: the CONNECT gate only enforces when its UUID file actually loaded.
	//! A missing/empty file must not lock every player out of the server.
	protected bool ConnectGateLoaded()
	{
		foreach (EL_Whitelist whitelist : m_aWhitelists)
		{
			if (whitelist && whitelist.m_eType == EL_WhitelistType.CONNECT)
				return whitelist.WasLoaded();
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Spawn hook, forwarded from EL_GameModeRoleplay.OnPlayerSpawned. Pushes the
	//! player's whitelist flags to their character's owner-replicated component.
	void OnPlayerSpawned(int playerId, IEntity controlledEntity)
	{
		if (!GetGameMode() || !GetGameMode().IsMaster())
			return;

		EL_PlayerWhitelistComponent playerWhitelistComponent = EL_PlayerWhitelistComponent.Cast(controlledEntity.FindComponent(EL_PlayerWhitelistComponent));
		if (!playerWhitelistComponent)
			return;

		LoadAllWhitelists(playerWhitelistComponent);
	}

	//------------------------------------------------------------------------------------------------
	void LoadAllWhitelists(EL_PlayerWhitelistComponent playerWhitelistComponent)
	{
		EL_WhitelistType whitelistFlags;
		string uuid = EL_Utils.GetPlayerUID(playerWhitelistComponent.GetOwner());
		foreach (EL_Whitelist whitelist : m_aWhitelists)
		{
			if (whitelist && whitelist.VerifyPlayer(uuid))
				whitelistFlags |= whitelist.m_eType;
		}
		playerWhitelistComponent.RpcSetWhitelists(whitelistFlags);
	}

	//------------------------------------------------------------------------------------------------
	bool WhitelistExists(EL_WhitelistType whitelistType)
	{
		foreach (EL_Whitelist whitelist : m_aWhitelists)
		{
			if (whitelist && whitelist.m_eType == whitelistType)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \return true when the type is not gated or the player is on its list.
	bool VerifyPlayer(string uuid, EL_WhitelistType whitelistType)
	{
		foreach (EL_Whitelist whitelist : m_aWhitelists)
		{
			if (whitelist && whitelist.m_eType == whitelistType)
				return whitelist.VerifyPlayer(uuid);
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: persist the UUID, then enable the flag on the player if they are online.
	void EnableWhitelistOnPlayer(string uuid, EL_WhitelistType whitelistType)
	{
		AddUUIDToWhitelist(uuid, whitelistType);

		IEntity targetPlayer = GetPlayerByUID(uuid);
		if (!targetPlayer)
			return;

		EL_PlayerWhitelistComponent playerWhitelistComponent = EL_PlayerWhitelistComponent.Cast(targetPlayer.FindComponent(EL_PlayerWhitelistComponent));
		if (playerWhitelistComponent)
			playerWhitelistComponent.RpcEnableWhitelist(whitelistType);
	}

	//------------------------------------------------------------------------------------------------
	void AddUUIDToWhitelist(string uuid, EL_WhitelistType whitelistType)
	{
		foreach (EL_Whitelist whitelist : m_aWhitelists)
		{
			if (whitelist && whitelist.m_eType == whitelistType)
				whitelist.AddNewUuidToFile(uuid);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: remove the UUID, then disable the flag on the player if they are online.
	void DisableWhitelistOnPlayer(string uuid, EL_WhitelistType whitelistType)
	{
		if (!WhitelistExists(whitelistType))
		{
			Print(string.Format("[%1-WHITELIST] Trying to remove a UUID from a whitelist that is not active in the gamemode", typename.EnumToString(EL_WhitelistType, whitelistType)), LogLevel.WARNING);
			return;
		}

		RemoveUUIDFromWhitelist(uuid, whitelistType);

		IEntity targetPlayer = GetPlayerByUID(uuid);
		if (!targetPlayer)
			return;

		EL_PlayerWhitelistComponent playerWhitelistComponent = EL_PlayerWhitelistComponent.Cast(targetPlayer.FindComponent(EL_PlayerWhitelistComponent));
		if (playerWhitelistComponent)
			playerWhitelistComponent.RpcDisableWhitelist(whitelistType);
	}

	//------------------------------------------------------------------------------------------------
	void RemoveUUIDFromWhitelist(string uuid, EL_WhitelistType whitelistType)
	{
		foreach (EL_Whitelist whitelist : m_aWhitelists)
		{
			if (whitelist && whitelist.m_eType == whitelistType)
				whitelist.RemoveUUIDFromFile(uuid);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \return the currently controlled entity of the player with the given platform UID, or null.
	protected IEntity GetPlayerByUID(string uuid)
	{
		if (uuid.IsEmpty())
			return null;

		PlayerManager playerManager = GetGame().GetPlayerManager();
		array<int> playerIds = {};
		playerManager.GetPlayers(playerIds);
		foreach (int playerId : playerIds)
		{
			if (EL_Utils.GetPlayerUID(playerId) == uuid)
				return playerManager.GetPlayerControlledEntity(playerId);
		}
		return null;
	}
}