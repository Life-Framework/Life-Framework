//------------------------------------------------------------------------------------------------
//! Server-side checks for the creation RPC. The registration form only collects the answer; this
//! decides it. Returns "" to accept or the localization key shown to the refused player.
//! There is no faction pick: everyone registers as a civilian, and a whitelisted player becomes
//! police by clocking in at the station duty desk (LF_DutyAction).
//------------------------------------------------------------------------------------------------
class LF_CharacterCreationRules
{
	//------------------------------------------------------------------------------------------------
	//! Name/age submission (form R-89, LF_RegistrationRules). An account holds at most
	//! LF_CharacterConfig.MaxPerAccount() characters (#331).
	static string RefuseCharacter(LF_PlayerAccount account, string firstName, string lastName, int age)
	{
		if (!account)
			return "#EL-CC_Error_NoAccount";

		int maxPerAccount = LF_CharacterConfig.MaxPerAccount();
		if (account.GetCharacterCount() >= maxPerAccount)
		{
			if (maxPerAccount == 1)
				return "#EL-CC_Error_Exists";

			return "#LF-Char_Error_Full";
		}

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

	//------------------------------------------------------------------------------------------------
	//! A returning player picks a character only when the config allows more than one per account
	//! and nothing ties them to the active one: an officer still clocked in returns as that officer
	//! (the duty locker holds that character's clothes), and a living body is never swapped.
	static bool OffersCharacterPick(LF_PlayerAccount account, bool controlsLivingBody)
	{
		if (!account || !account.HasCharacters() || controlsLivingBody)
			return false;

		if (account.IsOnDuty())
			return false;

		return LF_CharacterConfig.MaxPerAccount() > 1;
	}

	//------------------------------------------------------------------------------------------------
	//! Server check of a pick. "" accepts; otherwise the localization key the player is told.
	//! \param slot 0-based roster slot, or LF_CharacterSelectSheet.SLOT_NEW to register another.
	static string RefusePick(LF_PlayerAccount account, int slot, bool controlsLivingBody)
	{
		if (!account)
			return "#EL-CC_Error_NoAccount";

		if (controlsLivingBody || account.IsOnDuty())
			return "#LF-CharSel_Refused";

		if (slot == LF_CharacterSelectSheet.SLOT_NEW)
		{
			if (account.GetCharacterCount() >= LF_CharacterConfig.MaxPerAccount())
				return "#LF-Char_Error_Full";

			return "";
		}

		if (!account.GetCharacterAt(slot))
			return "#LF-CharSel_Refused";

		return "";
	}
}

//------------------------------------------------------------------------------------------------
//! First-join flow. Runs on the server only. The registration form opens on the owning client
//! through an SCR_PlayerController owner RPC (LF_CharacterCreationController.c) and its answer
//! comes back as RpcAsk_LF_CreateCharacter, which lands in ServerCreateCharacter and is
//! re-validated by LF_CharacterCreationRules before the account is touched.
class LF_CharacterCreationManager : Managed
{
	//! Strong ref: the singleton owns itself (the game mode keeps no field for it).
	protected static ref LF_CharacterCreationManager s_Instance;

	protected const int FLOW_POLL_DELAY_MS = 100;
	protected static const ResourceName DEFAULT_CHARACTER_PREFAB = "{9B5BB216CC7FF18E}Prefabs/Characters/Core/Character_Roleplay.et";

	//------------------------------------------------------------------------------------------------
	static LF_CharacterCreationManager GetInstance()
	{
		if (!s_Instance)
			s_Instance = new LF_CharacterCreationManager();

		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	void ~LF_CharacterCreationManager()
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

		LF_PlayerAccountManager accountManager = LF_PlayerAccountManager.GetInstance();
		if (!accountManager)
			return;

		// Resolved here rather than at connect: the platform identity is not guaranteed to be
		// available yet while the world is loading, and every consumer of the account must key
		// on the same UID the spawn logic uses.
		string playerUid = LF_Utils.GetAccountUID(playerId);
		LF_PlayerAccount account = accountManager.GetAccount(playerUid);
		if (!account)
		{
			account = LF_PlayerAccount.Create(playerUid);
			accountManager.AddAccount(account);
		}

		LF_Debug.Info("CharacterCreation", "flow start for player " + playerId);

		// Nothing stands between joining and playing: no character means the registration form,
		// then a civilian spawn.
		if (!account.HasCharacters())
		{
			ShowRegistrationForm(playerController, "");
			return;
		}

		// More than one character allowed (#331): the player picks who they arrive as.
		if (LF_CharacterCreationRules.OffersCharacterPick(account, ControlsLivingBody(playerController)))
		{
			ShowCharacterSelect(playerController, account);
			return;
		}

		SpawnPlayer(playerId, playerController);
	}

	//------------------------------------------------------------------------------------------------
	//! A living body under the player's control (a save restored it, or they are already playing).
	protected static bool ControlsLivingBody(PlayerController playerController)
	{
		if (!playerController)
			return false;

		IEntity controlled = playerController.GetControlledEntity();
		if (!controlled)
			return false;

		CharacterControllerComponent characterController = CharacterControllerComponent.Cast(controlled.FindComponent(CharacterControllerComponent));
		return characterController && characterController.GetLifeState() != ECharacterLifeState.DEAD;
	}

	//------------------------------------------------------------------------------------------------
	//! Opens the character pick on the player's machine: one line per character in slot order, and
	//! the "register a new resident" line while the account has a free slot.
	protected void ShowCharacterSelect(PlayerController playerController, notnull LF_PlayerAccount account)
	{
		SCR_PlayerController controller = SCR_PlayerController.Cast(playerController);
		if (!controller)
		{
			LF_Debug.Error("CharacterCreation", "character pick not sent: controller is not an SCR_PlayerController");
			return;
		}

		string names;
		for (int slot = 0; slot < account.GetCharacterCount(); slot++)
		{
			LF_PlayerCharacter character = account.GetCharacterAt(slot);
			if (slot > 0)
				names += LF_CharacterSelectSheet.NAME_SEPARATOR;

			if (character)
				names += character.GetFullName();
		}

		bool canRegister = account.GetCharacterCount() < LF_CharacterConfig.MaxPerAccount();
		LF_Debug.Info("CharacterCreation", string.Format("character pick sent to player %1: %2 character(s), new slot %3", controller.GetPlayerId(), account.GetCharacterCount(), canRegister));
		controller.LF_ShowCharacterSelect(names, canRegister);
	}

	//------------------------------------------------------------------------------------------------
	//! Server body of RpcAsk_LF_SelectCharacter. The pick is re-checked here: the slot must exist,
	//! and nobody switches character while alive or on duty.
	void ServerSelectCharacter(int playerId, int slot)
	{
		SCR_PlayerController controller = FindController(playerId);
		if (!controller)
			return;

		LF_PlayerAccount account = FindAccount(LF_Utils.GetAccountUID(playerId));
		string refusal = LF_CharacterCreationRules.RefusePick(account, slot, ControlsLivingBody(controller));
		if (!refusal.IsEmpty())
		{
			LF_Debug.Warn("CharacterCreation", string.Format("character pick refused for player %1 (slot %2): %3", playerId, slot, refusal));
			LF_NotificationManagerComponent.NotifyPlayer(playerId, "#EL-ID_Information", refusal, 5.0, LF_ENotificationType.WARNING);
			if (account && LF_CharacterCreationRules.OffersCharacterPick(account, ControlsLivingBody(controller)))
				ShowCharacterSelect(controller, account);
			return;
		}

		if (slot == LF_CharacterSelectSheet.SLOT_NEW)
		{
			LF_Debug.Info("CharacterCreation", string.Format("player %1 registers another character (%2 of %3)", playerId, account.GetCharacterCount() + 1, LF_CharacterConfig.MaxPerAccount()));
			ShowRegistrationForm(controller, "");
			return;
		}

		account.SetActiveCharacter(account.GetCharacterAt(slot));
		LF_AccountReplication.Publish(account);
		LF_Debug.Info("CharacterCreation", string.Format("player %1 plays character slot %2 (key %3)", playerId, slot + 1, account.GetActiveCharacterKey()));
		SpawnPlayer(playerId, controller);
	}

	//------------------------------------------------------------------------------------------------
	//! No character yet: the registration form (R-89) opens on the player's machine. Its answer
	//! comes back as RpcAsk_LF_CreateCharacter.
	//! \param refusal Localization key written on the form when the last answer was refused.
	protected void ShowRegistrationForm(PlayerController playerController, string refusal)
	{
		SCR_PlayerController controller = SCR_PlayerController.Cast(playerController);
		if (!controller)
		{
			LF_Debug.Error("CharacterCreation", "registration form not sent: controller is not an SCR_PlayerController");
			return;
		}

		LF_Debug.Info("CharacterCreation", "registration form sent to player " + controller.GetPlayerId());
		controller.LF_ShowRegistrationForm(refusal);
	}

	//------------------------------------------------------------------------------------------------
	//! Server body of RpcAsk_LF_CreateCharacter. Name and age are re-checked here; the menu's
	//! own checks are only for its error text.
	void ServerCreateCharacter(int playerId, string firstName, string lastName, int age)
	{
		SCR_PlayerController controller = FindController(playerId);
		if (!controller)
			return;

		LF_PlayerAccount account = FindAccount(LF_Utils.GetAccountUID(playerId));
		string refusal = LF_CharacterCreationRules.RefuseCharacter(account, firstName, lastName, age);
		// A further character (#331) is registered from the pick, never from inside a living body.
		if (refusal.IsEmpty() && account && account.HasCharacters() && ControlsLivingBody(controller))
			refusal = "#LF-CharSel_Refused";

		if (!refusal.IsEmpty())
		{
			LF_Debug.Warn("CharacterCreation", string.Format("character refused for player %1: %2", playerId, refusal));
			// The form comes back with the reason written on it, unless the account has no free
			// slot (a double submit), none is loaded yet, or the player is already playing.
			bool playing = account && account.HasCharacters() && ControlsLivingBody(controller);
			if (account && account.GetCharacterCount() < LF_CharacterConfig.MaxPerAccount() && !playing)
				ShowRegistrationForm(controller, refusal);
			else
				LF_NotificationManagerComponent.NotifyPlayer(playerId, "#EL-ID_Information", refusal, 5.0, LF_ENotificationType.WARNING);
			return;
		}

		LF_PlayerCharacter character = LF_PlayerCharacter.Create(DEFAULT_CHARACTER_PREFAB, firstName.Trim(), lastName.Trim(), age);
		account.AddCharacter(character, true);
		LF_PlayerAccountManager.GetInstance().SaveAndReleaseAccount(account);
		LF_AccountReplication.Publish(account);
		// The new character's own records key (#331): the account uid for slot 1, its id after.
		string playerUid = account.GetActiveCharacterKey();
		LF_Debug.Info("CharacterCreation", string.Format("character registered for player %1, age %2, slot %3", playerId, age, account.GetCharacterCount()));
		OpenStartingBank(playerUid);

		SpawnPlayer(playerId, controller);
		GrantTestCars(playerId, playerUid);
	}

	//------------------------------------------------------------------------------------------------
	//! Playtest (LF_SpawnLogic.m_bGrantTestCars): a new character owns a garaged and an impounded car.
	protected void GrantTestCars(int playerId, string playerUid)
	{
		LF_SpawnLogic spawnLogic = LF_SpawnLogic.GetInstance();
		if (!spawnLogic || !spawnLogic.GrantsTestCars())
			return;

		IEntity body = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		IEntity lotEntity = GetGame().GetWorld().FindEntityByName(spawnLogic.GetTestCarLot());
		LF_VehicleLotComponent lot;
		if (lotEntity)
			lot = LF_Component<LF_VehicleLotComponent>.Find(lotEntity);

		if (!body || !lot)
		{
			LF_Debug.Error("CharacterCreation", string.Format("test cars not granted to player %1: no body or no lot named %2", playerId, spawnLogic.GetTestCarLot()));
			return;
		}

		array<LF_VehicleOffer> offers = {};
		for (int i = 0; i < lot.GetOfferCount(); i++)
		{
			offers.Insert(lot.GetOfferAt(i));
		}

		LF_VehicleOwnership.GrantTestCars(playerUid, body, offers, lot.GetShopId());
	}

	//------------------------------------------------------------------------------------------------
	protected SCR_PlayerController FindController(int playerId)
	{
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
		if (!controller)
			LF_Debug.Error("CharacterCreation", "request dropped: no player controller for player " + playerId);

		return controller;
	}

	//------------------------------------------------------------------------------------------------
	protected LF_PlayerAccount FindAccount(string playerUid)
	{
		if (playerUid.IsEmpty())
			return null;

		LF_PlayerAccountManager accountManager = LF_PlayerAccountManager.GetInstance();
		if (!accountManager)
			return null;

		return accountManager.GetAccount(playerUid);
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns the account's active character through the spawn logic. The spawn logic picks the
	//! spawn point, hands control over and clears the splash on handover; the ATM/survival
	//! components init in its post-spawn hook.
	//! The opening bank balance (LF_SpawnLogic.m_iStartingBank), paid once into an empty account
	//! when a new character is registered. A returning player never registers again.
	protected void OpenStartingBank(string playerUid)
	{
		LF_SpawnLogic spawnLogic = LF_SpawnLogic.GetInstance();
		if (!spawnLogic || playerUid.IsEmpty())
		{
			LF_Debug.Error("CharacterCreation", "starting bank not paid: no spawn logic or no player id");
			return;
		}

		int amount = spawnLogic.GetStartingBank();
		LF_BankAccount bank = LF_ATMManager.GetOrCreate(playerUid);
		if (amount <= 0 || !bank || bank.GetBalance() > 0)
			return;

		bank.Deposit(amount, "starting-bank");
		LF_Debug.Info("CharacterCreation", string.Format("starting bank %1 paid to %2", amount, playerUid));
	}

	//------------------------------------------------------------------------------------------------
	protected void SpawnPlayer(int playerId, PlayerController playerController)
	{
		SCR_RespawnSystemComponent respawnSystem = SCR_RespawnSystemComponent.GetInstance();
		LF_SpawnLogic spawnLogic;
		if (respawnSystem)
			spawnLogic = LF_SpawnLogic.Cast(respawnSystem.GetSpawnLogic());

		if (!spawnLogic)
		{
			LF_Debug.Error("CharacterCreation", "no LF_SpawnLogic on the respawn system, cannot spawn player " + playerId);
			return;
		}

		LF_Debug.Info("CharacterCreation", "spawning player " + playerId);
		spawnLogic.SpawnPlayer_S(playerId);

		// NOTE: the survival HUD is deliberately not opened here. Opened via OpenMenu it is a
		// full-screen menu that captures input and traps the player; it needs to be integrated as
		// a proper HUD element before it can come back.
	}
};
