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
	//! \param offered True while the server has a pick open for this player (sent on arrival).
	//! Without it a dead player could send a pick mid-session and switch character.
	static string RefusePick(LF_PlayerAccount account, int slot, bool controlsLivingBody, bool offered)
	{
		if (!account)
			return "#EL-CC_Error_NoAccount";

		if (!offered || controlsLivingBody || account.IsOnDuty())
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

	//------------------------------------------------------------------------------------------------
	//! Server check of the slot a switch pick names (LF_CharacterSwitch): another existing character,
	//! or a new resident while a slot is free. "" accepts.
	static string RefuseSwitchSlot(LF_PlayerAccount account, int slot)
	{
		if (!account)
			return "#EL-CC_Error_NoAccount";

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

	//------------------------------------------------------------------------------------------------
	//! Server check of anything that re-enters the arrival flow (the quiz's Continue, a form R-89
	//! answer): only a player the server is still arriving may. Anyone else is already in the world
	//! (alive, dead, downed, cuffed, jailed) and must not reach the pick or a fresh spawn (#2413).
	//! \param flowOpen True while the server has this player's arrival flow open (set on connect,
	//! cleared when they spawn).
	static string RefuseFlowRequest(bool flowOpen)
	{
		if (!flowOpen)
			return "#LF-CharSel_Refused";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Server check of a form R-89 answer for an account that already has a character: it registers
	//! only after the player chose "register a new resident" on the pick, and never from a living body.
	//! \param registerOffered True while the server has a "new resident" form open for this player.
	static string RefuseFurtherCharacter(LF_PlayerAccount account, bool controlsLivingBody, bool registerOffered)
	{
		if (!account || !account.HasCharacters())
			return "";

		if (controlsLivingBody || !registerOffered)
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

	// lf-fixed: poll cadence of a loop, not a balance value
	protected const int FLOW_POLL_DELAY_MS = 100;
	// lf-fixed: Workbench fast play waits this long for the control hand-over before the ID card
	protected const int FAST_PLAY_CARD_DELAY_MS = 1000;
	protected static const ResourceName DEFAULT_CHARACTER_PREFAB = "{9B5BB216CC7FF18E}Prefabs/Characters/Core/Character_Roleplay.et";

	//! Players the server sent the character pick and has no answer from yet (#331). A pick from
	//! anyone else is refused: only an arrival offers one.
	protected ref set<int> m_PickOffered = new set<int>();
	//! Players who chose "register a new resident" on the pick: their form R-89 may add a character.
	protected ref set<int> m_RegisterOffered = new set<int>();
	//! Players the server sent the switch sheet while they play (LF_CharacterSwitch, from the clerk).
	//! Their pick is a switch: checked again, then the body is stored and removed before the spawn.
	protected ref set<int> m_SwitchOffered = new set<int>();
	//! Players whose arrival flow is open (#2413): filled on connect, cleared when they spawn. The
	//! quiz's Continue and the registration form re-enter the flow only for these players, so a dead,
	//! downed, cuffed or jailed player cannot restart it and pick another character.
	protected ref set<int> m_FlowOpen = new set<int>();
	//! Players whose flow waits for their platform identity (logged once per wait).
	protected ref set<int> m_IdentityWaiting = new set<int>();
	//! Players told that the server is still connecting them (once per wait).
	protected ref set<int> m_IdentityNoticeSent = new set<int>();

	// lf-fixed: identity wait mechanics (poll, when to tell the player), not a server choice
	protected const int IDENTITY_POLL_MS = 500;
	// lf-fixed: see above
	protected const int IDENTITY_NOTICE_MS = 3000;
	//! A dedicated server with a backend waits this long for a player's identity before the form
	//! tells them it could not open their account. Past LF_PlayerIdentity.LISTEN_WAIT_MS.
	// lf-fixed: bounded by the spawn logic retry cap (MAX_REGISTER_RETRIES, about 60 s)
	protected const int IDENTITY_GIVE_UP_MS = 60000;

	//------------------------------------------------------------------------------------------------
	static LF_CharacterCreationManager GetInstance()
	{
		if (!s_Instance)
			s_Instance = new LF_CharacterCreationManager();

		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	//! World end: drop the singleton and with it the per-player offer sets, which are keyed by player
	//! id (ids restart at 1 in the next world).
	static void ResetStatics()
	{
		s_Instance = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a player left; their ids are reused by the next arrival, so no offer or wait may
	//! outlive the connection (a stale offer lets a new player 1 pass RefuseFurtherCharacter).
	static void ForgetPlayer(int playerId)
	{
		if (!s_Instance)
			return;

		s_Instance.m_PickOffered.RemoveItem(playerId);
		s_Instance.m_RegisterOffered.RemoveItem(playerId);
		s_Instance.m_SwitchOffered.RemoveItem(playerId);
		s_Instance.m_IdentityWaiting.RemoveItem(playerId);
		s_Instance.m_IdentityNoticeSent.RemoveItem(playerId);
		s_Instance.m_FlowOpen.RemoveItem(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: records the pick and register offers for a player id without a controller.
	void OfferForTest(int playerId)
	{
		m_PickOffered.Insert(playerId);
		m_RegisterOffered.Insert(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a player connected, so their arrival flow is open until they spawn (tests call it too).
	void OpenFlow(int playerId)
	{
		if (!m_FlowOpen.Contains(playerId))
			m_FlowOpen.Insert(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the player's arrival is over (they spawned), so nothing re-enters the flow.
	void CloseFlow(int playerId)
	{
		m_FlowOpen.RemoveItem(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: "" when this player's arrival flow is open, else the localization key to tell them.
	string RefuseFlowRequest(int playerId)
	{
		return LF_CharacterCreationRules.RefuseFlowRequest(m_FlowOpen.Contains(playerId));
	}

	//------------------------------------------------------------------------------------------------
	//! \return true when an offer or wait is still recorded for this player id.
	bool HasPlayerState(int playerId)
	{
		return m_PickOffered.Contains(playerId) || m_RegisterOffered.Contains(playerId) || m_SwitchOffered.Contains(playerId) || m_IdentityWaiting.Contains(playerId) || m_IdentityNoticeSent.Contains(playerId) || m_FlowOpen.Contains(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Server, from the player's ask after the clerk said yes: opens the switch sheet. Needs the
	//! clerk's fresh grant (LF_CharacterSwitch.ConsumeGrant), so a client cannot open it from anywhere,
	//! and every refusal is checked again here.
	//! \return True when the sheet was sent.
	bool OfferSwitch(int playerId)
	{
		if (!Replication.IsServer())
			return false;

		if (!LF_CharacterSwitch.ConsumeGrant(playerId, LF_CharacterSwitch.Now()))
		{
			LF_Debug.Warn(LF_CharacterSwitch.FEATURE, string.Format("switch sheet refused for player %1: the clerk granted no switch just now", playerId));
			return false;
		}

		string refusal = LF_CharacterSwitch.RefuseNow(playerId);
		if (!refusal.IsEmpty())
		{
			LF_CharacterSwitch.TellRefused(playerId, refusal, "the sheet");
			return false;
		}

		SCR_PlayerController controller = FindController(playerId);
		LF_PlayerAccount account = FindAccount(LF_Utils.GetAccountUID(playerId));
		if (!controller || !account || !account.GetActiveCharacter())
			return false;

		int currentSlot = account.GetCharacters().Find(account.GetActiveCharacter());
		bool canRegister = account.GetCharacterCount() < LF_CharacterConfig.MaxPerAccount();
		if (!m_SwitchOffered.Contains(playerId))
			m_SwitchOffered.Insert(playerId);

		LF_CharacterSwitch.NoteOffer(playerId, LF_CharacterSwitch.Now(), BodyOrigin(playerId));

		LF_Debug.Info(LF_CharacterSwitch.FEATURE, string.Format("switch sheet sent to player %1: %2 character(s), playing slot %3, new slot %4", playerId, account.GetCharacterCount(), currentSlot + 1, canRegister));
		controller.LF_ShowCharacterSwitch(CharacterNames(account), canRegister, currentSlot);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the player put the switch sheet away; they stay who they are.
	void CancelSwitch(int playerId)
	{
		if (!m_SwitchOffered.Contains(playerId))
			return;

		m_SwitchOffered.RemoveItem(playerId);
		LF_CharacterSwitch.ForgetOffer(playerId);
		LF_Debug.Info(LF_CharacterSwitch.FEATURE, string.Format("player %1 stays as %2", playerId, LF_Utils.GetPlayerUID(playerId)));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: where the player's body stands, or the origin without one (RefuseNow refuses that).
	protected static vector BodyOrigin(int playerId)
	{
		if (!GetGame() || !GetGame().GetPlayerManager())
			return vector.Zero;

		IEntity body = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (!body)
			return vector.Zero;

		return body.GetOrigin();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a pick from the switch sheet. Staying is a cancel; anything else is checked again, the
	//! current character stored and its body removed, then the arrival flow spawns the pick (or opens
	//! form R-89 for a new resident).
	protected void ServerSwitchPick(notnull SCR_PlayerController controller, int playerId, int slot)
	{
		m_SwitchOffered.RemoveItem(playerId);
		bool holds = LF_CharacterSwitch.OfferHolds(playerId, LF_CharacterSwitch.Now(), BodyOrigin(playerId));
		LF_CharacterSwitch.ForgetOffer(playerId);
		if (!holds)
		{
			LF_CharacterSwitch.TellRefused(playerId, "#LF-CharSel_Refused", "the pick (the sheet went stale or the player left the clerk)");
			return;
		}

		LF_PlayerAccount account = FindAccount(LF_Utils.GetAccountUID(playerId));
		if (!account || !account.GetActiveCharacter())
		{
			LF_Debug.Warn(LF_CharacterSwitch.FEATURE, string.Format("switch pick of player %1 dropped: no account or character", playerId));
			return;
		}

		int currentSlot = account.GetCharacters().Find(account.GetActiveCharacter());
		if (slot == currentSlot)
		{
			CancelSwitch(playerId);
			LF_Debug.Info(LF_CharacterSwitch.FEATURE, string.Format("player %1 picked the character they play: nothing changes", playerId));
			return;
		}

		string refusal = LF_CharacterSwitch.RefuseNow(playerId);
		if (refusal.IsEmpty())
			refusal = LF_CharacterCreationRules.RefuseSwitchSlot(account, slot);

		if (!refusal.IsEmpty())
		{
			LF_CharacterSwitch.TellRefused(playerId, refusal, "the pick");
			return;
		}

		string fromKey = account.GetActiveCharacterKey();
		if (!LF_CharacterSwitch.StoreAndRemoveBody(playerId))
		{
			LF_NotificationManagerComponent.NotifyPlayer(playerId, "#LF-CharSwitch_Title", "#LF-CharSel_Refused", 5.0, LF_ENotificationType.WARNING);
			return;
		}

		// From here the player is arriving again: the flow, the form and the spawn treat them so.
		OpenFlow(playerId);
		if (slot == LF_CharacterSelectSheet.SLOT_NEW)
		{
			m_RegisterOffered.Insert(playerId);
			LF_Debug.Info(LF_CharacterSwitch.FEATURE, string.Format("player %1 left %2 to register another character (%3 of %4)", playerId, fromKey, account.GetCharacterCount() + 1, LF_CharacterConfig.MaxPerAccount()));
			ShowRegistrationForm(controller, "");
			return;
		}

		account.SetActiveCharacter(account.GetCharacterAt(slot));
		LF_AccountReplication.Publish(account);
		LF_Debug.Info(LF_CharacterSwitch.FEATURE, string.Format("player %1 switched from %2 to slot %3 (key %4)", playerId, fromKey, slot + 1, account.GetActiveCharacterKey()));
		SpawnPlayer(playerId, controller);
	}

	//------------------------------------------------------------------------------------------------
	//! The account's character names in slot order, one per line, as the sheet reads them.
	protected static string CharacterNames(notnull LF_PlayerAccount account)
	{
		string names;
		for (int slot = 0; slot < account.GetCharacterCount(); slot++)
		{
			LF_PlayerCharacter character = account.GetCharacterAt(slot);
			if (slot > 0)
				names += LF_CharacterSelectSheet.NAME_SEPARATOR;

			if (character)
				names += character.GetFullName();
		}

		return names;
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

		OpenFlow(playerId);
		GetGame().GetCallqueue().CallLater(ContinueFlow, FLOW_POLL_DELAY_MS, false, playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the player read the quiz's pass page. The flow runs again from the top, so a client
	//! that sends this without passing is asked the questions again. Refused unless the server is
	//! still arriving this player (#2413): the RPC is open to any client, dead or alive.
	void ContinueAfterQuiz(int playerId)
	{
		if (!Replication.IsServer())
			return;

		if (!AllowFlowRequest(playerId, "quiz continue"))
			return;

		ContinueFlow(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: gate for the two client answers that re-enter the arrival flow. A refusal is logged and
	//! the player told, never silent.
	//! \return true when this player's arrival flow is open.
	protected bool AllowFlowRequest(int playerId, string what)
	{
		string refusal = RefuseFlowRequest(playerId);
		if (refusal.IsEmpty())
			return true;

		LF_Debug.Warn("CharacterCreation", string.Format("%1 from player %2 refused: not arriving (already playing, dead, downed, cuffed or jailed)", what, playerId));
		LF_NotificationManagerComponent.NotifyPlayer(playerId, "#EL-ID_Information", refusal, 5.0, LF_ENotificationType.WARNING);
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: sends the rules quiz to an account that has not passed it. Used by the flow and by
	//! the two answers a client could send to get past it (register, pick a character).
	//! \return true when the quiz was sent and the caller must stop.
	protected bool GateQuiz(int playerId, string accountUid)
	{
		if (!LF_Quiz.Required(accountUid))
			return false;

		LF_QuizFlow.Offer(playerId);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void ContinueFlow(int playerId)
	{
		// Last line of defence: the poll can outlive a disconnect or a spawn, and no caller may
		// restart the flow for a player the server is not arriving (#2413).
		if (!RefuseFlowRequest(playerId).IsEmpty())
		{
			LF_Debug.Warn("CharacterCreation", string.Format("flow for player %1 dropped: arrival already over", playerId));
			return;
		}

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
		// on the same UID the spawn logic uses. "" means it is still on its way (LF_PlayerIdentity):
		// the account must not open under any other key, or the signed form looks it up under the
		// real identity and finds nothing (playtest build 4).
		string playerUid = LF_Utils.GetAccountUID(playerId);
		if (playerUid.IsEmpty())
		{
			WaitForIdentity(playerId, playerController);
			return;
		}

		m_IdentityWaiting.RemoveItem(playerId);
		m_IdentityNoticeSent.RemoveItem(playerId);

		LF_PlayerAccount account = accountManager.GetAccount(playerUid);
		if (!account)
		{
			account = LF_PlayerAccount.Create(playerUid);
			accountManager.AddAccount(account);
		}

		LF_Debug.Info("CharacterCreation", "flow start for player " + playerId);

		// Workbench "LF Fast Play" only: no quiz, no form, straight into town (LF_WorkbenchPlay).
		if (LF_WorkbenchPlay.IsFastPlay())
		{
			FastPlay(playerId, playerController, account);
			return;
		}

		// The rules orientation (#915) comes first, once per account: nobody registers or spawns
		// before passing it. Its Continue button re-enters here.
		if (GateQuiz(playerId, playerUid))
			return;

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
	//! No platform identity yet: the spawn splash stays up, the player is told the server is
	//! connecting, and the flow asks again. A listen host keys the player by name after
	//! LF_PlayerIdentity.LISTEN_WAIT_MS, so only a dedicated server whose online service never
	//! identifies the player reaches IDENTITY_GIVE_UP_MS: the form then opens with the reason on it,
	//! and signing it asks again (ServerCreateCharacter), so the player is never left without a way on.
	protected void WaitForIdentity(int playerId, PlayerController playerController)
	{
		int waitedMs = LF_PlayerIdentity.WaitedMs(playerId, System.GetTickCount());
		if (waitedMs < IDENTITY_GIVE_UP_MS)
		{
			if (!m_IdentityWaiting.Contains(playerId))
			{
				m_IdentityWaiting.Insert(playerId);
				LF_Debug.Info("CharacterCreation", "waiting for the platform identity of player " + playerId + " before opening their account");
			}

			// Usually it comes within a second (0.8 s on a launcher solo host); a longer wait is said.
			if (waitedMs >= IDENTITY_NOTICE_MS && !m_IdentityNoticeSent.Contains(playerId))
			{
				m_IdentityNoticeSent.Insert(playerId);
				LF_NotificationManagerComponent.NotifyPlayer(playerId, "#EL-ID_Information", "#LF-CC_Wait_Identity", 8.0, LF_ENotificationType.INFO);
			}

			GetGame().GetCallqueue().CallLater(ContinueFlow, IDENTITY_POLL_MS, false, playerId);
			return;
		}

		m_IdentityWaiting.RemoveItem(playerId);
		m_IdentityNoticeSent.RemoveItem(playerId);
		LF_Debug.Error("CharacterCreation", string.Format("player %1 has no platform identity after %2 s, so no account can open; the registration form asks them to sign again", playerId, waitedMs / 1000));
		ShowRegistrationForm(playerController, "#LF-CC_Error_NoIdentity");
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

		string names = CharacterNames(account);
		bool canRegister = account.GetCharacterCount() < LF_CharacterConfig.MaxPerAccount();
		m_PickOffered.Insert(controller.GetPlayerId());
		LF_Debug.Info("CharacterCreation", string.Format("character pick sent to player %1: %2 character(s), new slot %3", controller.GetPlayerId(), account.GetCharacterCount(), canRegister));
		controller.LF_ShowCharacterSelect(names, canRegister);
	}

	//------------------------------------------------------------------------------------------------
	//! Server body of RpcAsk_LF_SelectCharacter. The pick is re-checked here: the server must have
	//! offered one, the slot must exist, and nobody switches character while alive or on duty.
	void ServerSelectCharacter(int playerId, int slot)
	{
		SCR_PlayerController controller = FindController(playerId);
		if (!controller)
			return;

		if (GateQuiz(playerId, LF_Utils.GetAccountUID(playerId)))
			return;

		// A pick from the switch sheet (the player is still in their body) is a switch.
		if (m_SwitchOffered.Contains(playerId))
		{
			ServerSwitchPick(controller, playerId, slot);
			return;
		}

		bool offered = m_PickOffered.Contains(playerId);
		LF_PlayerAccount account = FindAccount(LF_Utils.GetAccountUID(playerId));
		string refusal = LF_CharacterCreationRules.RefusePick(account, slot, ControlsLivingBody(controller), offered);
		if (!refusal.IsEmpty())
		{
			LF_Debug.Warn("CharacterCreation", string.Format("character pick refused for player %1 (slot %2, offered %3): %4", playerId, slot, offered, refusal));
			// An unasked pick (a second answer, or a dead player mid-session) gets nothing back.
			if (!offered)
				return;

			m_PickOffered.RemoveItem(playerId);
			LF_NotificationManagerComponent.NotifyPlayer(playerId, "#EL-ID_Information", refusal, 5.0, LF_ENotificationType.WARNING);
			if (account && LF_CharacterCreationRules.OffersCharacterPick(account, ControlsLivingBody(controller)))
				ShowCharacterSelect(controller, account);
			return;
		}

		m_PickOffered.RemoveItem(playerId);
		if (slot == LF_CharacterSelectSheet.SLOT_NEW)
		{
			m_RegisterOffered.Insert(playerId);
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

		// The form only ever opens during arrival; an answer from anyone else (a dead or arrested
		// player replaying it) must not reach the continue-with-character fallback below (#2413).
		if (!AllowFlowRequest(playerId, "form R-89"))
			return;

		string accountUid = LF_Utils.GetAccountUID(playerId);
		if (GateQuiz(playerId, accountUid))
			return;

		LF_PlayerAccount account = FindAccount(accountUid);
		if (!account)
		{
			// Never a dead end. No identity yet: the form comes back saying so. An identity with no
			// account behind it (the flow gave up waiting before it arrived): the player has never
			// registered under it, so the account opens now.
			if (accountUid.IsEmpty())
			{
				LF_Debug.Error("CharacterCreation", "form R-89 of player " + playerId + " not registered: still no platform identity; the form asks them to sign again");
				ShowRegistrationForm(controller, "#LF-CC_Error_NoIdentity");
				return;
			}

			LF_Debug.Warn("CharacterCreation", string.Format("player %1 signed form R-89 with no account open under %2; opening it now", playerId, accountUid));
			account = LF_PlayerAccountManager.GetOrCreate(accountUid);
		}

		// The form was open although the account already has a character (it opened before the
		// identity was confirmed): back into the flow, which spawns that character or offers the pick.
		if (account.HasCharacters() && !m_RegisterOffered.Contains(playerId) && !ControlsLivingBody(controller))
		{
			LF_Debug.Warn("CharacterCreation", "player " + playerId + " signed form R-89 but their account already has a character; continuing with it");
			ContinueFlow(playerId);
			return;
		}

		string refusal = LF_CharacterCreationRules.RefuseCharacter(account, firstName, lastName, age);
		// A further character (#331) is registered only from the pick's "new resident" line, never
		// from inside a living body.
		string furtherRefusal = LF_CharacterCreationRules.RefuseFurtherCharacter(account, ControlsLivingBody(controller), m_RegisterOffered.Contains(playerId));
		if (refusal.IsEmpty())
			refusal = furtherRefusal;

		if (!refusal.IsEmpty())
		{
			LF_Debug.Warn("CharacterCreation", string.Format("character refused for player %1: %2", playerId, refusal));
			// The form comes back with the reason written on it, unless the account has no free
			// slot (a double submit), none is loaded yet, or no further character is asked for.
			if (account && account.GetCharacterCount() < LF_CharacterConfig.MaxPerAccount() && furtherRefusal.IsEmpty())
				ShowRegistrationForm(controller, refusal);
			else
				LF_NotificationManagerComponent.NotifyPlayer(playerId, "#EL-ID_Information", refusal, 5.0, LF_ENotificationType.WARNING);
			return;
		}

		LF_PlayerCharacter character = LF_PlayerCharacter.Create(DEFAULT_CHARACTER_PREFAB, firstName.Trim(), lastName.Trim(), age);
		account.AddCharacter(character, true);
		// Its next spawn is its first (#1964); the spawn logic reads and clears the mark.
		LF_FirstSpawn.Mark(character.GetId());
		m_RegisterOffered.RemoveItem(playerId);
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
	//! Workbench LF Fast Play: the account gets the dev character (once), its starting bank, a spawn
	//! and an ID card. The rules quiz is skipped, not passed: nothing about it is recorded. Reached only
	//! when LF_WorkbenchPlay.IsFastPlay(), which a non-Workbench build never returns.
	protected void FastPlay(int playerId, PlayerController playerController, notnull LF_PlayerAccount account)
	{
		if (!account.HasCharacters())
		{
			LF_PlayerCharacter character = LF_PlayerCharacter.Create(DEFAULT_CHARACTER_PREFAB, LF_WorkbenchPlay.DEV_FIRST_NAME, LF_WorkbenchPlay.DEV_LAST_NAME, LF_WorkbenchPlay.DEV_AGE);
			account.AddCharacter(character, true);
			LF_FirstSpawn.Mark(character.GetId());
			LF_PlayerAccountManager.GetInstance().SaveAndReleaseAccount(account);
			LF_AccountReplication.Publish(account);
			OpenStartingBank(account.GetActiveCharacterKey());
			LF_Debug.Info(LF_WorkbenchPlay.FEATURE, "fast play: dev character registered for player " + playerId);
		}

		SpawnPlayer(playerId, playerController);
		// The hand-over completes after this frame; the card goes on the body once it is controlled.
		GetGame().GetCallqueue().CallLater(GiveFastPlayCard, FAST_PLAY_CARD_DELAY_MS, false, playerId);
	}

	//------------------------------------------------------------------------------------------------
	protected void GiveFastPlayCard(int playerId)
	{
		IEntity body = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		LF_PlayerAccount account = FindAccount(LF_Utils.GetAccountUID(playerId));
		if (!body || !account)
		{
			LF_Debug.Error(LF_WorkbenchPlay.FEATURE, "fast play: player " + playerId + " has no body or account a second after the spawn; no ID card");
			return;
		}

		bool handed = LF_QuestRewards.HandIdCard(body, account);
		LF_Debug.Info(LF_WorkbenchPlay.FEATURE, string.Format("fast play: player %1 is in town (new ID card %2)", playerId, handed));
	}

	//------------------------------------------------------------------------------------------------
	//! Playtest (LF_SpawnLogic.m_bGrantTestCars): a new character owns a garaged and an impounded car.
	protected void GrantTestCars(int playerId, string playerUid)
	{
		LF_SpawnLogic spawnLogic = LF_SpawnLogic.GetInstance();
		if (!spawnLogic || !spawnLogic.GrantsTestCars())
			return;

		IEntity body = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		LF_VehicleLotComponent lot = LF_VehicleLotComponent.FindTestCarLot();

		if (!body || !lot)
		{
			LF_Debug.Error("CharacterCreation", string.Format("test cars not granted to player %1: no body or no lot flagged m_bTestCarLot", playerId));
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

		PayStartingBank(playerUid, spawnLogic.GetStartingBank());
	}

	//------------------------------------------------------------------------------------------------
	//! Pays `amount` into the account's bank once: only an empty account gets it, so a returning
	//! character keeps its balance. Public for the pack test that proves a fresh account's figure.
	//! \return The amount paid, 0 when nothing was.
	static int PayStartingBank(string playerUid, int amount)
	{
		LF_BankAccount bank = LF_ATMManager.GetOrCreate(playerUid);
		if (amount <= 0 || !bank || bank.GetBalance() > 0)
			return 0;

		bank.Deposit(amount, "starting-bank");
		LF_Debug.Info("CharacterCreation", string.Format("starting bank %1 paid to %2", amount, playerUid));
		return amount;
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
		// Arrival ends here: from now on nothing re-enters the flow, whatever happens to the body.
		CloseFlow(playerId);
		spawnLogic.SpawnPlayer_S(playerId);

		// NOTE: the survival HUD is deliberately not opened here. Opened via OpenMenu it is a
		// full-screen menu that captures input and traps the player; it needs to be integrated as
		// a proper HUD element before it can come back.
	}
};

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_CharacterCreationManager's statics (LF_StaticReset finds it by reflection).
class LF_CharacterCreationManager_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_CharacterCreationManager";
	}

	override void Apply()
	{
		LF_CharacterCreationManager.ResetStatics();
	}
}
