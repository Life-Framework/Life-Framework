[EntityEditorProps(category: "EveronLife/Core", description: "Core gamemode")]
class EL_GameModeRoleplayClass: SCR_BaseGameModeClass
{
}

class EL_GameModeRoleplay: SCR_BaseGameMode
{
	protected ref EL_CharacterCreationManager m_CharacterCreationManager;
	protected ref EL_ATMManager m_ATMManager;
	protected ref EL_JobManager m_JobManager;

	//! Server opt-in for dev tools (debug menu, agent-harness RPCs). Off on the production prefab;
	//! on in GameMode_Roleplay_Debug.et (DebugWorld). Administrators pass regardless - see EL_DevGate.
	[Attribute("0", desc: "Let every player use dev tools (debug menu, agent harness). Development worlds only; administrators are always allowed.")]
	protected bool m_bDevToolsEnabled;

	//! The DebugWorld open-key listener is registered once per local player spawn so the menu
	//! is reachable in a dev session without a dedicated UI component on the prefab.
	protected bool m_bDebugMenuListenerRegistered;

	//------------------------------------------------------------------------------------------------
	//! \return true when this game mode opts every player into dev tools.
	bool AreDevToolsEnabled()
	{
		return m_bDevToolsEnabled;
	}

	//------------------------------------------------------------------------------------------------
	// Initialization moved to OnPlayerConnected (lazy init) to avoid constructor overloads
	//------------------------------------------------------------------------------------------------
	void ~EL_GameModeRoleplay()
	{
		EL_PlayerAccountManager.Reset();
		UnregisterDebugMenuListener();
	}

	//------------------------------------------------------------------------------------------------
	override void OnPlayerSpawned(int playerId, IEntity controlledEntity)
	{
		super.OnPlayerSpawned(playerId, controlledEntity);

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

		// Lazy initialization of managers (create on first use)
		if (!m_CharacterCreationManager)
			m_CharacterCreationManager = new EL_CharacterCreationManager();
		if (!m_ATMManager)
			m_ATMManager = new EL_ATMManager();
		if (!m_JobManager)
			m_JobManager = new EL_JobManager();

		m_CharacterCreationManager.OnPlayerConnected(playerId);
	}
}
