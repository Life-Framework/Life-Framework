[EntityEditorProps(category: "EveronLife/Core", description: "Core gamemode")]
class EL_GameModeRoleplayClass: SCR_BaseGameModeClass
{
}

class EL_GameModeRoleplay: SCR_BaseGameMode
{
	//! Server opt-in for dev tools (debug menu, agent-harness RPCs). Off on the production prefab;
	//! on in GameMode_Roleplay_Debug.et (DebugWorld). Administrators pass regardless - see EL_DevGate.
	[Attribute("0", desc: "Let every player use dev tools (debug menu, agent harness). Development worlds only; administrators are always allowed.")]
	protected bool m_bDevToolsEnabled;

	//! The DebugWorld open-key listener is registered once per local player spawn so the menu
	//! is reachable in a dev session without a dedicated UI component on the prefab.
	protected bool m_bDebugMenuListenerRegistered;

	//! Connected on-duty police, written by the server (EL_AccountReplication) so client-side
	//! robbery offers can apply the minimum-police gate. The game mode is always replicated.
	[RplProp()]
	protected int m_iEL_PoliceOnDuty;

	protected bool m_bEL_AccountRefreshRunning;

	//------------------------------------------------------------------------------------------------
	//! \return true when this game mode opts every player into dev tools and the mission header
	//! (LF_MissionHeader, e.g. the playtest mission) does not close them.
	bool AreDevToolsEnabled()
	{
		return LF_MissionHeader.ResolveDevToolsEnabled(m_bDevToolsEnabled, GetGame().GetMissionHeader());
	}

	//------------------------------------------------------------------------------------------------
	protected override void OnGameStart()
	{
		super.OnGameStart();
		LF_HostStop.StartIfRequested();
	}

	//------------------------------------------------------------------------------------------------
	void ~EL_GameModeRoleplay()
	{
		EL_PlayerAccountManager.Reset();
		UnregisterDebugMenuListener();
		if (m_bEL_AccountRefreshRunning && GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(EL_RefreshAccountReplication);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: bumps only when the count changes.
	void EL_SetPoliceOnDuty(int count)
	{
		if (!Replication.IsServer() || count == m_iEL_PoliceOnDuty)
			return;

		m_iEL_PoliceOnDuty = count;
		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	int EL_GetPoliceOnDuty()
	{
		return m_iEL_PoliceOnDuty;
	}

	//------------------------------------------------------------------------------------------------
	override void OnPlayerSpawned(int playerId, IEntity controlledEntity)
	{
		super.OnPlayerSpawned(playerId, controlledEntity);

		EL_AccountReplication.PublishForPlayer(playerId, controlledEntity);

		EL_WhitelistRespawnHandlerComponent whitelist = EL_WhitelistRespawnHandlerComponent.Cast(FindComponent(EL_WhitelistRespawnHandlerComponent));
		if (whitelist)
			whitelist.OnPlayerSpawned(playerId, controlledEntity);

		RegisterDebugMenuListener(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Client-side debug menu open. Registered once per local player spawn (the listener is
	//! process-global, so a second spawn must not double-register). On a dedicated server there
	//! is no local input, so the listener never fires there. The key itself is gated at press time
	//! (OnOpenDebugMenu) because a player's admin role can change mid-session; the server-side
	//! EL_DevGate check in RpcAsk_EL_DebugAction is the real authority.
	protected void RegisterDebugMenuListener(int playerId)
	{
		if (m_bDebugMenuListenerRegistered)
			return;

		PlayerController local = GetGame().GetPlayerController();
		if (!local || local.GetPlayerId() != playerId)
			return;

		InputManager input = GetGame().GetInputManager();
		if (!input)
			return;

		input.AddActionListener("EL_OpenDebugMenu", EActionTrigger.DOWN, OnOpenDebugMenu);
		m_bDebugMenuListenerRegistered = true;
	}

	//------------------------------------------------------------------------------------------------
	protected void UnregisterDebugMenuListener()
	{
		if (!m_bDebugMenuListenerRegistered)
			return;

		InputManager input = GetGame().GetInputManager();
		if (input)
			input.RemoveActionListener("EL_OpenDebugMenu", EActionTrigger.DOWN, OnOpenDebugMenu);

		m_bDebugMenuListenerRegistered = false;
	}

	//------------------------------------------------------------------------------------------------
	protected void OnOpenDebugMenu()
	{
		MenuManager menuManager = GetGame().GetMenuManager();
		if (!menuManager)
			return;

		if (menuManager.FindMenuByPreset(ChimeraMenuPreset.DebugMenu))
		{
			menuManager.CloseMenuByPreset(ChimeraMenuPreset.DebugMenu);
			return;
		}

		// Cosmetic client gate on the key: the server refuses the actions anyway.
		PlayerController local = GetGame().GetPlayerController();
		int localPlayerId = 0;
		if (local)
			localPlayerId = local.GetPlayerId();
		if (!EL_DevGate.IsAllowedForPlayer(localPlayerId))
		{
			EL_Debug.Warn("DebugMenu", "open refused: dev tools are disabled in this game mode and the player is not an administrator");
			return;
		}

		menuManager.OpenMenu(ChimeraMenuPreset.DebugMenu);
	}

	//------------------------------------------------------------------------------------------------
	override void OnPlayerConnected(int playerId)
	{
		super.OnPlayerConnected(playerId);

		EL_CharacterCreationManager creation = ResolveConnectManagers();
		creation.OnPlayerConnected(playerId);

		// Self-healing account replication pass (server only, started once).
		if (Replication.IsServer() && !m_bEL_AccountRefreshRunning)
		{
			GetGame().GetCallqueue().CallLater(EL_RefreshAccountReplication, EL_AccountReplication.REFRESH_INTERVAL_MS, true);
			m_bEL_AccountRefreshRunning = true;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void EL_RefreshAccountReplication()
	{
		EL_AccountReplication.RefreshAll();
	}

	//------------------------------------------------------------------------------------------------
	//! The manager half of OnPlayerConnected. Every manager here is a self-owned singleton, so
	//! this only resolves them through GetInstance() and never constructs one. A `new` here
	//! replaced the bank cache the save load had already filled, which wiped every balance on
	//! the first connect after a restart. Public and static so a LOGIC test drives the same path.
	//! \return The character creation manager the connecting player's flow runs on.
	static EL_CharacterCreationManager ResolveConnectManagers()
	{
		EL_ATMManager.GetInstance();
		EL_JobManager.GetInstance();
		return EL_CharacterCreationManager.GetInstance();
	}
}
