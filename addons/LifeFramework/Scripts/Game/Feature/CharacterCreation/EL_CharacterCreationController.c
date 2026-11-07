//------------------------------------------------------------------------------------------------
//! Client <-> server bridge for the first-join menus. Hosted on the PlayerController: it exists
//! (and is owned by the joining client) before any character is spawned.
//!   server -> owner : EL_ShowFactionMenu, EL_ShowSpawnLocationMenu (RpcDo_*, RplRcver.Owner)
//!   owner -> server : EL_AskSelectFaction, EL_AskSpawnLocation, EL_AskCreateCharacter (RpcAsk_*)
//! The server identifies the player by this controller's id, never by a client-sent value, and
//! EL_CharacterCreationManager re-validates every choice. A listen host's own controller is
//! local on both ends, so it calls the handlers directly instead of RPCing to itself.
modded class SCR_PlayerController
{
	//! Strong refs: the menus are plain widget controllers and must live until clicked.
	protected ref EL_FactionSelectionMenu m_EL_FactionMenu;
	protected ref EL_SpawnLocationMenu m_EL_LocationMenu;

	//------------------------------------------------------------------------------------------------
	protected bool EL_IsLocalController()
	{
		return GetGame().GetPlayerController() == this;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: open the faction menu on this player's machine.
	void EL_ShowFactionMenu()
	{
		if (EL_IsLocalController())
			RpcDo_EL_ShowFactionMenu();
		else
			Rpc(RpcDo_EL_ShowFactionMenu);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: open the spawn location menu for an accepted faction on this player's machine.
	void EL_ShowSpawnLocationMenu(EL_Faction faction)
	{
		if (EL_IsLocalController())
			RpcDo_EL_ShowSpawnLocationMenu(faction);
		else
			Rpc(RpcDo_EL_ShowSpawnLocationMenu, faction);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: send the faction pick to the server.
	void EL_AskSelectFaction(EL_Faction faction)
	{
		if (Replication.IsServer())
			RpcAsk_EL_SelectFaction(faction);
		else
			Rpc(RpcAsk_EL_SelectFaction, faction);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: send the spawn location pick. An empty key finalizes without a location.
	void EL_AskSpawnLocation(string locationKey)
	{
		if (Replication.IsServer())
			RpcAsk_EL_SpawnLocation(locationKey);
		else
			Rpc(RpcAsk_EL_SpawnLocation, locationKey);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: send the name/age form.
	void EL_AskCreateCharacter(string firstName, string lastName, int age)
	{
		if (Replication.IsServer())
			RpcAsk_EL_CreateCharacter(firstName, lastName, age);
		else
			Rpc(RpcAsk_EL_CreateCharacter, firstName, lastName, age);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_EL_ShowFactionMenu()
	{
		if (!GetGame().GetWorkspace())
		{
			EL_Debug.Error("CharacterCreation", "faction menu not opened: no workspace on this machine");
			return;
		}

		EL_DestroySpawnSplash();
		if (m_EL_FactionMenu)
			m_EL_FactionMenu.Close();

		m_EL_FactionMenu = EL_FactionSelectionMenu.Open(this);
		if (!m_EL_FactionMenu)
			EL_Debug.Error("CharacterCreation", "failed to open the faction selection menu");
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_EL_ShowSpawnLocationMenu(int faction)
	{
		if (!GetGame().GetWorkspace())
		{
			EL_Debug.Error("CharacterCreation", "spawn location menu not opened: no workspace on this machine");
			return;
		}

		EL_DestroySpawnSplash();
		if (m_EL_LocationMenu)
			m_EL_LocationMenu.Close();

		m_EL_LocationMenu = EL_SpawnLocationMenu.Open(this, faction);
		if (!m_EL_LocationMenu)
		{
			// Degrade to the two-button flow: finalize the faction without a location pick.
			EL_Debug.Error("CharacterCreation", "failed to open the spawn location menu, finalizing faction without one");
			EL_AskSpawnLocation("");
		}
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_EL_SelectFaction(int faction)
	{
		EL_CharacterCreationManager manager = EL_CharacterCreationManager.GetInstance();
		if (!manager)
		{
			EL_Debug.Error("CharacterCreation", "faction request dropped: no creation manager on the server");
			return;
		}

		manager.ServerSelectFaction(GetPlayerId(), faction);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_EL_SpawnLocation(string locationKey)
	{
		EL_CharacterCreationManager manager = EL_CharacterCreationManager.GetInstance();
		if (!manager)
		{
			EL_Debug.Error("CharacterCreation", "spawn location request dropped: no creation manager on the server");
			return;
		}

		manager.ServerSelectSpawnLocation(GetPlayerId(), locationKey);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_EL_CreateCharacter(string firstName, string lastName, int age)
	{
		EL_CharacterCreationManager manager = EL_CharacterCreationManager.GetInstance();
		if (!manager)
		{
			EL_Debug.Error("CharacterCreation", "character request dropped: no creation manager on the server");
			return;
		}

		manager.ServerCreateCharacter(GetPlayerId(), firstName, lastName, age);
	}

	//------------------------------------------------------------------------------------------------
	//! The respawn system's splash covers every screen until told otherwise, and the audit chain
	//! that normally clears it never runs for the local player of an offline session.
	protected void EL_DestroySpawnSplash()
	{
		SCR_RespawnSystemComponent respawnSystem = SCR_RespawnSystemComponent.GetInstance();
		if (respawnSystem)
			respawnSystem.DestroyLoadingPlaceholder();
	}
}
