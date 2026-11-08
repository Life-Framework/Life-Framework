//------------------------------------------------------------------------------------------------
//! Server-side checks for the creation RPCs. A client menu only collects the choice; these
//! decide it. Each returns "" to accept or the localization key shown to the refused player.
//------------------------------------------------------------------------------------------------
class EL_CharacterCreationRules
{
	//------------------------------------------------------------------------------------------------
	//! \return True when value is a faction the creation flow offers.
	static bool IsOfferedFaction(int value)
	{
		return value == EL_Faction.CIVILIAN || value == EL_Faction.POLICE;
	}

	//------------------------------------------------------------------------------------------------
	static EL_EFactionType ToWhitelistFaction(EL_Faction faction)
	{
		if (faction == EL_Faction.POLICE)
			return EL_EFactionType.POLICE;

		return EL_EFactionType.CIVILIAN;
	}

	//------------------------------------------------------------------------------------------------
	//! Faction pick. The faction is fixed once chosen or once a character exists, and a
	//! restricted faction needs the server whitelist (the client menu cannot see it).
	static string RefuseFaction(EL_PlayerAccount account, int factionValue, string playerUid)
	{
		if (!account || playerUid.IsEmpty())
			return "#EL-CC_Error_NoAccount";

		if (account.HasCharacters() || account.WasFactionChosen())
			return "#EL-CC_Error_FactionLocked";

		if (!IsOfferedFaction(factionValue))
			return "#EL-CC_Error_FactionDenied";

		if (!EL_WhitelistManager.MayJoinFaction(playerUid, ToWhitelistFaction(factionValue)))
			return "#EL-CC_Error_FactionDenied";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Spawn location pick for a faction. An empty key is the degrade path (no location menu):
	//! the spawn logic falls back to any spawn point of the faction.
	static string RefuseSpawnLocation(EL_Faction faction, string locationKey)
	{
		if (locationKey.IsEmpty())
			return "";

		EL_SpawnLocation location = EL_SpawnLocations.GetByKey(locationKey);
		if (!location || location.m_eFaction != faction)
			return "#EL-CC_Error_Location";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Name/age submission (form R-89, LF_RegistrationRules). One character per account, after
	//! the faction is chosen.
	static string RefuseCharacter(EL_PlayerAccount account, string firstName, string lastName, int age)
	{
		if (!account)
			return "#EL-CC_Error_NoAccount";

		if (account.HasCharacters())
			return "#EL-CC_Error_Exists";

		if (!account.WasFactionChosen())
			return "#EL-CC_Error_FactionLocked";

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
//! First-join flow. Runs on the server only. Menus open on the owning client through
//! SCR_PlayerController owner RPCs (EL_CharacterCreationController.c) and their choices come
//! back as RpcAsk_EL_* calls, which land in the Server* methods below and are re-validated by
//! EL_CharacterCreationRules before the account is touched.
class EL_CharacterCreationManager : Managed
{
	//! Strong ref: the singleton owns itself (the game mode keeps no field for it).
	protected static ref EL_CharacterCreationManager s_Instance;

	protected const int FLOW_POLL_DELAY_MS = 100;
	protected static const ResourceName DEFAULT_CHARACTER_PREFAB = "{9B5BB216CC7FF18E}Prefabs/Characters/Core/Character_Roleplay.et";

	//! Faction accepted in stage one, applied to the account once the location is accepted too.
	protected ref map<int, EL_Faction> m_mPendingFactions;

	//------------------------------------------------------------------------------------------------
	static EL_CharacterCreationManager GetInstance()
	{
		if (!s_Instance)
			s_Instance = new EL_CharacterCreationManager();

		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	//! Does not register itself; only GetInstance() sets the singleton.
	void EL_CharacterCreationManager()
	{
		m_mPendingFactions = new map<int, EL_Faction>();
	}

	//------------------------------------------------------------------------------------------------
	void ~EL_CharacterCreationManager()
	{
		if (s_Instance == this)
			s_Instance = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Disconnect: drops a faction accepted in stage one whose location never came. Does not
	//! create the manager.
	//! \return True when a pending faction was dropped.
	static bool ReleasePlayer(int playerId)
	{
		if (!s_Instance || !s_Instance.m_mPendingFactions || !s_Instance.m_mPendingFactions.Contains(playerId))
			return false;

		s_Instance.m_mPendingFactions.Remove(playerId);
		EL_Debug.Info("CharacterCreation", string.Format("pending faction dropped for player %1: disconnected", playerId));
		return true;
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

		if (!account.HasCharacters())
		{
			// Offer faction selection until the player made an explicit choice (CIVILIAN is both
			// the default and a valid pick).
			if (!account.WasFactionChosen())
			{
				ShowFactionSelectionMenu(playerController);
				return;
			}

			ShowRegistrationForm(playerController, "");
			return;
		}

		SpawnPlayer(playerId, playerController);
	}

	//------------------------------------------------------------------------------------------------
	//! Faction chosen but no character yet: the registration form (R-89) opens on the player's
	//! machine. Its answer comes back as RpcAsk_EL_CreateCharacter. The form never picks a faction.
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
	//! Opens the faction menu on the player's own machine (owner RPC; direct for a listen host).
	protected void ShowFactionSelectionMenu(PlayerController playerController)
	{
		SCR_PlayerController controller = SCR_PlayerController.Cast(playerController);
		if (!controller)
		{
			EL_Debug.Error("CharacterCreation", "faction menu not sent: controller is not an SCR_PlayerController");
			return;
		}

		EL_Debug.Info("CharacterCreation", "faction menu sent to player " + controller.GetPlayerId());
		controller.EL_ShowFactionMenu();
	}

	//------------------------------------------------------------------------------------------------
	//! Server body of RpcAsk_EL_SelectFaction. Stage one: validate and remember the faction,
	//! then open the location picker on the client. A refusal notifies and re-opens the faction
	//! menu, so a non-whitelisted police pick never reaches the account.
	void ServerSelectFaction(int playerId, int factionValue)
	{
		SCR_PlayerController controller = FindController(playerId);
		if (!controller)
			return;

		string playerUid = EL_Utils.GetPlayerUID(playerId);
		EL_PlayerAccount account = FindAccount(playerUid);
		string refusal = EL_CharacterCreationRules.RefuseFaction(account, factionValue, playerUid);
		if (!refusal.IsEmpty())
		{
			EL_Debug.Warn("CharacterCreation", string.Format("faction %1 refused for player %2: %3", factionValue, playerId, refusal));
			EL_NotificationManagerComponent.NotifyPlayer(playerId, "#EL-Faction_Title", refusal, 5.0, EL_ENotificationType.WARNING);
			if (account && !account.HasCharacters() && !account.WasFactionChosen())
				controller.EL_ShowFactionMenu();
			return;
		}

		EL_Faction faction = factionValue;
		m_mPendingFactions.Set(playerId, faction);
		EL_Debug.Info("CharacterCreation", string.Format("faction %1 accepted for player %2, awaiting location", typename.EnumToString(EL_Faction, faction), playerId));
		controller.EL_ShowSpawnLocationMenu(faction);
	}

	//------------------------------------------------------------------------------------------------
	//! Server body of RpcAsk_EL_SpawnLocation. Stage two: the location must belong to the
	//! pending faction. Then the faction is written and the spawn flow continues.
	void ServerSelectSpawnLocation(int playerId, string locationKey)
	{
		SCR_PlayerController controller = FindController(playerId);
		if (!controller)
			return;

		if (!m_mPendingFactions.Contains(playerId))
		{
			EL_Debug.Warn("CharacterCreation", string.Format("spawn location '%1' refused for player %2: no accepted faction", locationKey, playerId));
			return;
		}

		EL_Faction faction = m_mPendingFactions.Get(playerId);
		string refusal = EL_CharacterCreationRules.RefuseSpawnLocation(faction, locationKey);
		if (!refusal.IsEmpty())
		{
			EL_Debug.Warn("CharacterCreation", string.Format("spawn location '%1' refused for player %2: %3", locationKey, playerId, refusal));
			EL_NotificationManagerComponent.NotifyPlayer(playerId, "#EL-SpawnLoc_Title", refusal, 5.0, EL_ENotificationType.WARNING);
			controller.EL_ShowSpawnLocationMenu(faction);
			return;
		}

		if (!locationKey.IsEmpty())
			EL_SpawnLogic.SetSelectedLocation(playerId, locationKey);

		FinalizeFaction(playerId, faction);
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
			if (account && !account.HasCharacters() && account.WasFactionChosen())
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
	protected void FinalizeFaction(int playerId, EL_Faction faction)
	{
		m_mPendingFactions.Remove(playerId);

		string playerUid = EL_Utils.GetPlayerUID(playerId);
		EL_PlayerAccount account = FindAccount(playerUid);
		if (!account)
		{
			EL_Debug.Error("CharacterCreation", "faction not saved: no account for player " + playerId);
			return;
		}

		account.SetFaction(faction);
		EL_PlayerAccountManager.GetInstance().SaveAndReleaseAccount(account);
		EL_Debug.Info("CharacterCreation", string.Format("faction %1 saved for player %2", typename.EnumToString(EL_Faction, faction), playerId));

		// Advance the flow: the re-entrant pass sees the chosen faction and spawns the character.
		OnPlayerConnected(playerId);
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
	//! faction spawn point, hands control over and clears the splash on handover; the ATM/survival
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
