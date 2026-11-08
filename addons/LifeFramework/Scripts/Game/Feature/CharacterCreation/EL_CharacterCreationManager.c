//------------------------------------------------------------------------------------------------
//! Server-side checks for the creation RPC. The registration form only collects the answer; this
//! decides it. Returns "" to accept or the localization key shown to the refused player.
//! There is no faction pick: everyone registers as a civilian, and a whitelisted player becomes
//! police by clocking in at the station duty desk (EL_DutyAction).
//------------------------------------------------------------------------------------------------
class EL_CharacterCreationRules
{
	//------------------------------------------------------------------------------------------------
	//! Name/age submission (form R-89, LF_RegistrationRules). One character per account.
	static string RefuseCharacter(EL_PlayerAccount account, string firstName, string lastName, int age)
	{
		if (!account)
			return "#EL-CC_Error_NoAccount";

		if (account.HasCharacters())
			return "#EL-CC_Error_Exists";

		string first = firstName.Trim();
		string last = lastName.Trim();
		if (first.IsEmpty() || last.IsEmpty())
			return "#EL-CC_Error_Fields";

		string refusal = LF_RegistrationRules.RefuseName(first);
		if (!refusal.IsEmpty())
			return refusal;

		refusal = LF_RegistrationRules.RefuseName(last);
		if (!refusal.IsEmpty())
			return refusal;

		return LF_RegistrationRules.RefuseAge(age);
	}
}

//------------------------------------------------------------------------------------------------
//! First-join flow. Runs on the server only. The registration form opens on the owning client
//! through an SCR_PlayerController owner RPC (EL_CharacterCreationController.c) and its answer
//! comes back as RpcAsk_EL_CreateCharacter, which lands in ServerCreateCharacter and is
//! re-validated by EL_CharacterCreationRules before the account is touched.
class EL_CharacterCreationManager : Managed
{
	//! Strong ref: the singleton owns itself (the game mode keeps no field for it).
	protected static ref EL_CharacterCreationManager s_Instance;

	protected const int FLOW_POLL_DELAY_MS = 100;
	protected static const ResourceName DEFAULT_CHARACTER_PREFAB = "{9B5BB216CC7FF18E}Prefabs/Characters/Core/Character_Roleplay.et";

	//------------------------------------------------------------------------------------------------
	static EL_CharacterCreationManager GetInstance()
	{
		if (!s_Instance)
			s_Instance = new EL_CharacterCreationManager();

		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	void ~EL_CharacterCreationManager()
	{
		if (s_Instance == this)
			s_Instance = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Entry point from the game mode. The world is still loading when this fires, so the flow
	//! starts on the first tick after the game state reaches GAME; menus opened during loading
	//! never make it on screen. The poll self-reschedules (non-repeating): removing a call from
	//! inside its own callback corrupts the callqueue.
	void OnPlayerConnected(int playerId)
	{
		if (!Replication.IsServer())
			return;

		GetGame().GetCallqueue().CallLater(ContinueFlow, FLOW_POLL_DELAY_MS, false, playerId);
	}

	//------------------------------------------------------------------------------------------------
	protected void ContinueFlow(int playerId)
	{
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		if (!gameMode || gameMode.GetState() != SCR_EGameModeState.GAME)
		{
			GetGame().GetCallqueue().CallLater(ContinueFlow, FLOW_POLL_DELAY_MS, false, playerId);
			return;
		}

		PlayerController playerController = GetGame().GetPlayerManager().GetPlayerController(playerId);
		if (!playerController)
			return;

		EL_PlayerAccountManager accountManager = EL_PlayerAccountManager.GetInstance();
		if (!accountManager)
			return;

		// Resolved here rather than at connect: the platform identity is not guaranteed to be
		// available yet while the world is loading, and every consumer of the account must key
		// on the same UID the spawn logic uses.
		string playerUid = EL_Utils.GetPlayerUID(playerId);
		EL_PlayerAccount account = accountManager.GetAccount(playerUid);
		if (!account)
		{
			account = EL_PlayerAccount.Create(playerUid);
			accountManager.AddAccount(account);
		}

		EL_Debug.Info("CharacterCreation", "flow start for player " + playerId);

		// Nothing stands between joining and playing: no character means the registration form,
		// then a civilian spawn.
		if (!account.HasCharacters())
		{
			ShowRegistrationForm(playerController, "");
			return;
		}

		SpawnPlayer(playerId, playerController);
	}

	//------------------------------------------------------------------------------------------------
	//! No character yet: the registration form (R-89) opens on the player's machine. Its answer
	//! comes back as RpcAsk_EL_CreateCharacter.
	//! \param refusal Localization key written on the form when the last answer was refused.
	protected void ShowRegistrationForm(PlayerController playerController, string refusal)
	{
		SCR_PlayerController controller = SCR_PlayerController.Cast(playerController);
		if (!controller)
		{
			EL_Debug.Error("CharacterCreation", "registration form not sent: controller is not an SCR_PlayerController");
			return;
		}

		EL_Debug.Info("CharacterCreation", "registration form sent to player " + controller.GetPlayerId());
		controller.LF_ShowRegistrationForm(refusal);
	}

	//------------------------------------------------------------------------------------------------
	//! Server body of RpcAsk_EL_CreateCharacter. Name and age are re-checked here; the menu's
	//! own checks are only for its error text.
	void ServerCreateCharacter(int playerId, string firstName, string lastName, int age)
	{
		SCR_PlayerController controller = FindController(playerId);
		if (!controller)
			return;

		string playerUid = EL_Utils.GetPlayerUID(playerId);
		EL_PlayerAccount account = FindAccount(playerUid);
		string refusal = EL_CharacterCreationRules.RefuseCharacter(account, firstName, lastName, age);
		if (!refusal.IsEmpty())
		{
			EL_Debug.Warn("CharacterCreation", string.Format("character refused for player %1: %2", playerId, refusal));
			// The form comes back with the reason written on it, unless the account already has a
			// character (a double submit) or none is loaded yet.
			if (account && !account.HasCharacters())
				ShowRegistrationForm(controller, refusal);
			else
				EL_NotificationManagerComponent.NotifyPlayer(playerId, "#EL-ID_Information", refusal, 5.0, EL_ENotificationType.WARNING);
			return;
		}

		EL_PlayerCharacter character = EL_PlayerCharacter.Create(DEFAULT_CHARACTER_PREFAB, firstName.Trim(), lastName.Trim(), age);
		account.AddCharacter(character, true);
		EL_PlayerAccountManager.GetInstance().SaveAndReleaseAccount(account);
		EL_Debug.Info("CharacterCreation", string.Format("character registered for player %1, age %2", playerId, age));

		SpawnPlayer(playerId, controller);
	}

	//------------------------------------------------------------------------------------------------
	protected SCR_PlayerController FindController(int playerId)
	{
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
		if (!controller)
			EL_Debug.Error("CharacterCreation", "request dropped: no player controller for player " + playerId);

		return controller;
	}

	//------------------------------------------------------------------------------------------------
	protected EL_PlayerAccount FindAccount(string playerUid)
	{
		if (playerUid.IsEmpty())
			return null;

		EL_PlayerAccountManager accountManager = EL_PlayerAccountManager.GetInstance();
		if (!accountManager)
			return null;

		return accountManager.GetAccount(playerUid);
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns the account's active character through the spawn logic. The spawn logic picks the
	//! spawn point, hands control over and clears the splash on handover; the ATM/survival
	//! components init in its post-spawn hook.
	protected void SpawnPlayer(int playerId, PlayerController playerController)
	{
		SCR_RespawnSystemComponent respawnSystem = SCR_RespawnSystemComponent.GetInstance();
		EL_SpawnLogic spawnLogic;
		if (respawnSystem)
			spawnLogic = EL_SpawnLogic.Cast(respawnSystem.GetSpawnLogic());

		if (!spawnLogic)
		{
			EL_Debug.Error("CharacterCreation", "no EL_SpawnLogic on the respawn system, cannot spawn player " + playerId);
			return;
		}

		EL_Debug.Info("CharacterCreation", "spawning player " + playerId);
		spawnLogic.SpawnPlayer_S(playerId);

		// NOTE: the survival HUD is deliberately not opened here. Opened via OpenMenu it is a
		// full-screen menu that captures input and traps the player; it needs to be integrated as
		// a proper HUD element before it can come back.
	}
};
