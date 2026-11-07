[EntityEditorProps(category: "EveronLife/Core", description: "Core gamemode")]
class EL_GameModeRoleplayClass: SCR_BaseGameModeClass
{
}

class EL_GameModeRoleplay: SCR_BaseGameMode
{
	protected ref EL_CharacterCreationManager m_CharacterCreationManager;
	protected ref EL_ATMManager m_ATMManager;
	protected ref EL_JobManager m_JobManager;

	//! The DebugWorld open-key listener is registered once per local player spawn so the menu
	//! is reachable in a dev session without a dedicated UI component on the prefab.
	protected bool m_bDebugMenuListenerRegistered;

	//------------------------------------------------------------------------------------------------
	// Initialization moved to OnPlayerConnected (lazy init) to avoid constructor overloads
	//------------------------------------------------------------------------------------------------
	void ~EL_GameModeRoleplay()
	{
		EL_PlayerAccountManager.Reset();
		// World end: drop process-global feature state so a reload starts clean (see LF_StaticReset).
		LF_StaticReset.ResetAll();
		UnregisterDebugMenuListener();
	}

	//------------------------------------------------------------------------------------------------
	//! Releases per-player server state the player can no longer act on: escort holds where they
	//! are officer or prisoner, and a running convoy (fails it and drops the vehicle watch).
	//! Jail sentences are kept on purpose: they persist and resume on reconnect.
	override void OnPlayerDisconnected(int playerId, KickCauseCode cause, int timeout)
	{
		if (Replication.IsServer())
			ReleaseDisconnectedPlayer(playerId);

		super.OnPlayerDisconnected(playerId, cause, timeout);
	}

	//------------------------------------------------------------------------------------------------
	protected void ReleaseDisconnectedPlayer(int playerId)
	{
		string uid = EL_Utils.GetPlayerUID(playerId);
		IEntity controlled;
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (playerManager)
			controlled = playerManager.GetPlayerControlledEntity(playerId);

		if (uid.IsEmpty() && !controlled)
		{
			EL_Debug.Warn("Session", string.Format("disconnect cleanup skipped: player %1 has no uid and no entity", playerId));
			return;
		}

		int escorts = LF_JailEscort.ReleasePlayer(uid);

		string characterId;
		EL_PlayerAccountManager accounts = EL_PlayerAccountManager.GetInstance();
		if (accounts && !uid.IsEmpty())
		{
			EL_PlayerAccount account = accounts.GetAccount(uid);
			if (account && account.GetActiveCharacter())
				characterId = account.GetActiveCharacter().GetId();
		}

		LF_Contraband.ReleasePlayer(characterId, controlled);
		EL_Debug.Log("Session", string.Format("disconnect cleanup player=%1 escorts=%2", playerId, escorts));
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
