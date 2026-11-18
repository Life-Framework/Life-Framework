//------------------------------------------------------------------------------------------------
//! Whitelist handler on the game mode. Owns the legacy whitelist configs: moves each file into the
//! roster once at start (#2575) and enforces the optional CONNECT gate on join. It extends the base
//! game's SCR_BaseGameModeComponent.
[ComponentEditorProps(category: "EveronLife/Feature/Whitelist", description: "Server Whitelist Manager")]
class LF_WhitelistRespawnHandlerComponentClass : SCR_BaseGameModeComponentClass
{
}

class LF_WhitelistRespawnHandlerComponent : SCR_BaseGameModeComponent
{
	[Attribute("", UIWidgets.Object)]
	ref array<ref LF_Whitelist> m_aWhitelists;

	//------------------------------------------------------------------------------------------------
	//! Server start: each configured legacy list moves into the roster once (#2575).
	override void OnGameModeStart()
	{
		super.OnGameModeStart();
		if (!GetGameMode() || !GetGameMode().IsMaster() || !m_aWhitelists)
			return;

		foreach (LF_Whitelist whitelist : m_aWhitelists)
		{
			if (whitelist)
				LF_WhitelistLegacyMigration.Migrate(whitelist.m_eType, whitelist.m_sWhitelistFilePath);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Join gate: when a CONNECT whitelist is configured, the player must be on it.
	override void OnPlayerAuditSuccess(int playerId)
	{
		if (!GetGameMode() || !GetGameMode().IsMaster())
		{
			super.OnPlayerAuditSuccess(playerId);
			return;
		}

		// Whitelist off (#1074): the join gate is open and nobody is added to a list.
		if (!LF_Systems.IsEnabled(LF_WhitelistManager.SYSTEM))
		{
			LF_Debug.Log("Whitelist", string.Format("join allowed: uuid=%1 (Whitelist is off)", LF_Utils.GetAccountUID(playerId)));
			super.OnPlayerAuditSuccess(playerId);
			return;
		}

		if (WhitelistExists(LF_WhitelistType.CONNECT) && !ConnectGateLoaded())
		{
			LF_Debug.Warn("Whitelist", "CONNECT whitelist configured but its file is missing/empty - join gate disabled (fail-safe)");
			super.OnPlayerAuditSuccess(playerId);
			return;
		}

		if (WhitelistExists(LF_WhitelistType.CONNECT) && !VerifyPlayer(LF_Utils.GetAccountUID(playerId), LF_WhitelistType.CONNECT))
		{
			LF_Debug.Log("Whitelist", string.Format("join blocked: uuid=%1 not on CONNECT whitelist", LF_Utils.GetAccountUID(playerId)));
			GetGame().GetPlayerManager().KickPlayer(playerId, PlayerManagerKickReason.KICK, 10);
			return;
		}

		LF_Debug.Log("Whitelist", string.Format("join allowed: uuid=%1", LF_Utils.GetAccountUID(playerId)));
		LF_WhitelistManager.AddFirstPlayerToPolice(LF_Utils.GetAccountUID(playerId));
		super.OnPlayerAuditSuccess(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Fail-safe: the CONNECT gate only enforces when its UUID file actually loaded.
	//! A missing/empty file must not lock every player out of the server.
	protected bool ConnectGateLoaded()
	{
		foreach (LF_Whitelist whitelist : m_aWhitelists)
		{
			if (whitelist && whitelist.m_eType == LF_WhitelistType.CONNECT)
				return whitelist.WasLoaded();
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Spawn hook, still forwarded from LF_GameModeRoleplay.OnPlayerSpawned. A no-op: the client flag
	//! copy it used to push is gone (#2576), and police duty reads LF_WhitelistManager.
	void OnPlayerSpawned(int playerId, IEntity controlledEntity)
	{
	}

	//------------------------------------------------------------------------------------------------
	bool WhitelistExists(LF_WhitelistType whitelistType)
	{
		foreach (LF_Whitelist whitelist : m_aWhitelists)
		{
			if (whitelist && whitelist.m_eType == whitelistType)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \return true when the type is not gated or the player is on its list.
	bool VerifyPlayer(string uuid, LF_WhitelistType whitelistType)
	{
		foreach (LF_Whitelist whitelist : m_aWhitelists)
		{
			if (whitelist && whitelist.m_eType == whitelistType)
				return whitelist.VerifyPlayer(uuid);
		}
		return true;
	}
}
