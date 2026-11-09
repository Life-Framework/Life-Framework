//------------------------------------------------------------------------------------------------
//! Client->server bridge for police menu actions. Hosted on the officer's character: a client
//! can only RPC on entities it owns, and the character is the one entity the officer owns.
//! The server validates every request (faction, duty, wanted state, amount) and performs the
//! authoritative action; replies go through LF_NotificationManagerComponent.
modded class SCR_ChimeraCharacter
{
	//------------------------------------------------------------------------------------------------
	//! Ask the server to arrest the player with the given UID.
	//! \param targetPlayerUid Platform-stable UID of the wanted player.
	void LF_AskPoliceArrest(string targetPlayerUid)
	{
		Rpc(RpcAsk_LF_PoliceArrest, targetPlayerUid);
	}

	//------------------------------------------------------------------------------------------------
	//! Ask the server to promote this officer. The server always refuses: rank is not a client field.
	void LF_AskPoliceSelfPromote()
	{
		if (Replication.IsServer())
			LF_PoliceRanks.RequestSelfPromotion(LF_Utils.GetPlayerUid(this));
		else
			Rpc(RpcAsk_LF_PoliceSelfPromote);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_PoliceArrest(string targetPlayerUid)
	{
		IEntity officer = this;
		if (!officer)
			return;

		int officerPlayerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(officer);
		LF_PlayerAccount officerAccount = LF_PoliceUtils.GetAccountForEntity(officer);
		if (!LF_PoliceUtils.IsOnDutyOfficer(officerAccount))
		{
			LF_Debug.Error("Jail", string.Format("arrest refused: %1 is not an on-duty officer", officerPlayerId));
			return;
		}

		if (!LF_PoliceRanks.AuthorizeArrest(officerAccount))
		{
			if (officerPlayerId > 0)
				LF_NotificationManagerComponent.NotifyPlayer(officerPlayerId, "#EL-Police_Title", "#EL-Rank_ArrestDenied", 3.0, LF_ENotificationType.WARNING);
			return;
		}

		string officerUid = LF_Utils.GetPlayerUID(officer);
		LF_PlayerAccount targetAccount;
		IEntity target = LF_PoliceUtils.FindPlayerEntityByUid(targetPlayerUid, targetAccount);
		if (!LF_PoliceUtils.IsTargetInReach(officer, target))
		{
			// The menu lists every wanted player on the server. The arrest is a hands-on act:
			// no connected body, or a body beyond reach, is not arrestable from the menu.
			LF_Debug.Warn("Jail", string.Format("arrest refused: %1 is not within %2 m of officer %3", targetPlayerUid, LF_PoliceUtils.POLICE_REACH, officerUid));
			if (officerPlayerId > 0)
				LF_NotificationManagerComponent.NotifyPlayer(officerPlayerId, "#EL-Police_Title", "#EL-Police_TooFar", 3.0, LF_ENotificationType.WARNING);
			return;
		}

		if (!LF_Jail.Arrest(officerUid, targetPlayerUid, target))
			return;

		int targetPlayerId = 0;
		if (target)
			targetPlayerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(target);
		LF_NotificationManagerComponent.NotifyPlayer(officerPlayerId, "#EL-Police_Title", "#EL-Player_Arrested", 3.0, LF_ENotificationType.POLICE_ALERT);
		if (targetPlayerId > 0)
			LF_NotificationManagerComponent.NotifyPlayer(targetPlayerId, "#EL-Police_Title", "#EL-Player_Arrested", 3.0, LF_ENotificationType.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_PoliceSelfPromote()
	{
		LF_PoliceRanks.RequestSelfPromotion(LF_Utils.GetPlayerUid(this));
	}
};

//------------------------------------------------------------------------------------------------
//! Pure police-domain helpers. Server-side validation lives here so the LF_Test
//! suite can exercise them without a world.
class LF_PoliceUtils
{
	//! The largest fine a senior officer may write on a ticket (LF_PoliceRanks.FineCap).
	static const int MAX_FINE = 5000;

	//! How close the officer must stand to arrest or fine from the police menu. Matches the
	//! in-world arrest action's range. There is no restraint state in the framework or in
	//! vanilla Reforger, so reach is the hands-on requirement.
	static const float POLICE_REACH = 5.0;

	//------------------------------------------------------------------------------------------------
	//! \param officerPos Officer's world position.
	//! \param targetPos Target's world position.
	//! \return true when the two are within POLICE_REACH.
	static bool IsWithinReach(vector officerPos, vector targetPos)
	{
		return vector.Distance(officerPos, targetPos) <= POLICE_REACH;
	}

	//------------------------------------------------------------------------------------------------
	//! \return true when both bodies exist and stand within POLICE_REACH. A missing body is out of reach.
	static bool IsTargetInReach(IEntity officer, IEntity target)
	{
		if (!officer || !target)
			return false;

		return IsWithinReach(officer.GetOrigin(), target.GetOrigin());
	}

	//------------------------------------------------------------------------------------------------
	//! \return Sentence length in seconds. Wanted level times secondsPerWanted, clamped to maxSeconds.
	//! A non-positive wanted level or step is zero. A max of zero is the zero-duration test config.
	static int SentenceSeconds(int wantedLevel, int secondsPerWanted, int maxSeconds)
	{
		if (wantedLevel <= 0)
			return 0;

		if (secondsPerWanted <= 0)
			return 0;

		if (maxSeconds < 0)
			maxSeconds = 0;

		int raw = wantedLevel * secondsPerWanted;
		if (raw > maxSeconds)
			return maxSeconds;

		return raw;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Origin of the placed jail cell. A missing cell logs and returns the zero vector.
	static vector GetJailPosition()
	{
		if (!LF_Jail.HasCell())
		{
			LF_Debug.Error("Jail", "jail position missing: no cell is placed");
			return "0 0 0";
		}

		return LF_Jail.GetCellOrigin();
	}

	//------------------------------------------------------------------------------------------------
	//! \param account Account to check, may be null.
	//! \return true when the account belongs to an on-duty police officer.
	static bool IsOnDutyOfficer(LF_PlayerAccount account)
	{
		if (!account)
			return false;
		if (account.GetFaction() != LF_Faction.POLICE)
			return false;
		return account.IsOnDuty();
	}

	//------------------------------------------------------------------------------------------------
	//! \param entity Player character.
	//! \return The cached account for the character, or null.
	static LF_PlayerAccount GetAccountForEntity(IEntity entity)
	{
		if (!entity)
			return null;

		LF_PlayerAccountManager accountManager = LF_PlayerAccountManager.GetInstance();
		if (!accountManager)
			return null;

		return accountManager.GetAccount(LF_Utils.GetPlayerUID(entity));
	}

	//------------------------------------------------------------------------------------------------
	//! \param playerUid Platform-stable UID to match.
	//! \param outAccount The wanted player's cached account, or null when not found.
	//! \return The connected player entity matching the UID, or null.
	static IEntity FindPlayerEntityByUid(string playerUid, out LF_PlayerAccount outAccount)
	{
		outAccount = null;
		if (playerUid.IsEmpty())
			return null;

		LF_PlayerControllerManagerCompat playerManager = LF_PlayerControllerManagerCompat.GetInstance();
		if (!playerManager)
			return null;

		array<int> playerIds = {};
		playerManager.GetAllPlayerIds(playerIds);

		foreach (int playerId : playerIds)
		{
			PlayerController pc = playerManager.GetPlayerController(playerId);
			if (!pc)
				continue;

			IEntity entity = pc.GetControlledEntity();
			if (entity && LF_Utils.GetPlayerUID(entity) == playerUid)
			{
				outAccount = GetAccountForEntity(entity);
				return entity;
			}
		}

		return null;
	}
};

//------------------------------------------------------------------------------------------------
class LF_PoliceMenu : ChimeraMenuBase
{
	protected PlayerController m_PlayerController;

	// UI input only: Enter, Esc and the pad, polled every frame (LF_UIMenuInput).
	protected Widget m_wCloseButton;
	protected ref LF_UIMenuInput m_Input = new LF_UIMenuInput();

	//------------------------------------------------------------------------------------------------
	override void OnMenuOpen()
	{
		super.OnMenuOpen();

		Widget root = GetRootWidget();
		if (!root)
		{
			LF_Debug.Error("Police", "police menu opened without a root widget");
			return;
		}

		m_wCloseButton = root.FindAnyWidget("CloseButton");
		ScriptInvoker closeClick = LF_UIMenuInput.ClickInvoker(m_wCloseButton);
		if (closeClick)
			closeClick.Insert(OnClosePressed);
		else
			LF_Debug.Error("Police", "police menu close button missing or not a button; Esc still closes");

		array<Widget> focus = {};
		focus.Insert(root.FindAnyWidget("SearchButton"));
		focus.Insert(root.FindAnyWidget("ArrestButton"));
		focus.Insert(m_wCloseButton);
		m_Input.SetFocusOrder(focus);
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuUpdate(float tDelta)
	{
		super.OnMenuUpdate(tDelta);

		LF_EUIMenuInput pressed = m_Input.Tick();
		if (pressed == LF_EUIMenuInput.BACK)
		{
			OnClosePressed();
			return;
		}

		if (pressed == LF_EUIMenuInput.SELECT && m_Input.GetSelected() == m_wCloseButton)
			OnClosePressed();
	}

	//------------------------------------------------------------------------------------------------
	override bool OnClick(Widget w, int x, int y, int button)
	{
		if (m_wCloseButton && m_Input.RingEntry(w) == m_wCloseButton)
		{
			OnClosePressed();
			return true;
		}

		return super.OnClick(w, x, y, button);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnClosePressed()
	{
		if (m_Input.Ready("Close"))
			Close();
	}

	//------------------------------------------------------------------------------------------------
	void SetPlayerController(PlayerController playerController)
	{
		m_PlayerController = playerController;
	}

	//------------------------------------------------------------------------------------------------
	//! Client-side entry: routes the arrest request to the server via the officer's character.
	void ArrestPlayer(string playerUid)
	{
		SCR_ChimeraCharacter officer = GetOfficerCharacter();
		if (officer)
			officer.LF_AskPoliceArrest(playerUid);
	}

	//------------------------------------------------------------------------------------------------
	protected SCR_ChimeraCharacter GetOfficerCharacter()
	{
		if (!m_PlayerController)
			return null;

		IEntity entity = m_PlayerController.GetControlledEntity();
		return SCR_ChimeraCharacter.Cast(entity);
	}
};
