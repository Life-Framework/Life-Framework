[EntityEditorProps(category: "EveronLife/Core", description: "Core gamemode")]
class EL_GameModeRoleplayClass: SCR_BaseGameModeClass
{
}

class EL_GameModeRoleplay: SCR_BaseGameMode
{
	//! The DebugWorld open-key listener is registered once per local player spawn so the menu
	//! is reachable in a dev session without a dedicated UI component on the prefab.
	protected bool m_bDebugMenuListenerRegistered;

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
	//! is no local input, so the listener never fires there; the RPC gate on the server is the
	//! real authority anyway.
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

		menuManager.OpenMenu(ChimeraMenuPreset.DebugMenu);
	}

	//------------------------------------------------------------------------------------------------
	override void OnPlayerConnected(int playerId)
	{
		super.OnPlayerConnected(playerId);

		EL_CharacterCreationManager creation = ResolveConnectManagers();
		creation.OnPlayerConnected(playerId);
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
