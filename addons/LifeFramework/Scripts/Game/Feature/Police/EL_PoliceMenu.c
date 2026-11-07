//------------------------------------------------------------------------------------------------
//! Client->server bridge for police menu actions. Hosted on the officer's character: a client
//! can only RPC on entities it owns, and the character is the one entity the officer owns.
//! The server validates every request (faction, duty, wanted state, amount) and performs the
//! authoritative action; replies go through EL_NotificationManagerComponent.
modded class SCR_ChimeraCharacter
{
	//------------------------------------------------------------------------------------------------
	//! Ask the server to arrest the player with the given UID.
	//! \param targetPlayerUid Platform-stable UID of the wanted player.
	void EL_AskPoliceArrest(string targetPlayerUid)
	{
		Rpc(RpcAsk_EL_PoliceArrest, targetPlayerUid);
	}

	//------------------------------------------------------------------------------------------------
	//! Ask the server to fine the player with the given UID.
	//! \param targetPlayerUid Platform-stable UID of the wanted player.
	//! \param amount Fine amount in cash.
	void EL_AskPoliceFine(string targetPlayerUid, int amount)
	{
		Rpc(RpcAsk_EL_PoliceFine, targetPlayerUid, amount);
	}

	//------------------------------------------------------------------------------------------------
	//! Ask the server to promote this officer. The server always refuses: rank is not a client field.
	void EL_AskPoliceSelfPromote()
	{
		if (Replication.IsServer())
			LF_PoliceRanks.RequestSelfPromotion(EL_Utils.GetPlayerUid(this));
		else
			Rpc(RpcAsk_EL_PoliceSelfPromote);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_EL_PoliceArrest(string targetPlayerUid)
	{
		IEntity officer = this;
		if (!officer)
			return;

		int officerPlayerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(officer);
		EL_PlayerAccount officerAccount = EL_PoliceUtils.GetAccountForEntity(officer);
		if (!EL_PoliceUtils.IsOnDutyOfficer(officerAccount))
		{
			EL_Debug.Error("Jail", string.Format("arrest refused: %1 is not an on-duty officer", officerPlayerId));
			return;
		}

		if (!LF_PoliceRanks.AuthorizeArrest(officerAccount))
		{
			if (officerPlayerId > 0)
				EL_NotificationManagerComponent.NotifyPlayer(officerPlayerId, "#EL-Police_Title", "#EL-Rank_ArrestDenied", 3.0, EL_ENotificationType.WARNING);
			return;
		}

		string officerUid = EL_Utils.GetPlayerUID(officer);
		EL_PlayerAccount targetAccount;
		IEntity target = EL_PoliceUtils.FindPlayerEntityByUid(targetPlayerUid, targetAccount);
		if (!EL_PoliceUtils.IsTargetInReach(officer, target))
		{
			// The menu lists every wanted player on the server. The arrest is a hands-on act:
			// no connected body, or a body beyond reach, is not arrestable from the menu.
			EL_Debug.Warn("Jail", string.Format("arrest refused: %1 is not within %2 m of officer %3", targetPlayerUid, EL_PoliceUtils.POLICE_REACH, officerUid));
			if (officerPlayerId > 0)
				EL_NotificationManagerComponent.NotifyPlayer(officerPlayerId, "#EL-Police_Title", "#EL-Police_TooFar", 3.0, EL_ENotificationType.WARNING);
			return;
		}

		if (!LF_Jail.Arrest(officerUid, targetPlayerUid, target))
			return;

		int targetPlayerId = 0;
		if (target)
			targetPlayerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(target);
		EL_NotificationManagerComponent.NotifyPlayer(officerPlayerId, "#EL-Police_Title", "#EL-Player_Arrested", 3.0, EL_ENotificationType.POLICE_ALERT);
		if (targetPlayerId > 0)
			EL_NotificationManagerComponent.NotifyPlayer(targetPlayerId, "#EL-Police_Title", "#EL-Player_Arrested", 3.0, EL_ENotificationType.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_EL_PoliceFine(string targetPlayerUid, int amount)
	{
		IEntity officer = this;
		if (!officer)
			return;

		int officerPlayerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(officer);
		EL_PlayerAccount officerAccount = EL_PoliceUtils.GetAccountForEntity(officer);
		if (!EL_PoliceUtils.IsOnDutyOfficer(officerAccount))
		{
			EL_Debug.Warn("Police", string.Format("fine refused: %1 is not an on-duty officer", officerPlayerId));
			return;
		}

		if (!LF_PoliceRanks.AuthorizeFine(officerAccount, amount))
		{
			if (officerPlayerId > 0)
				EL_NotificationManagerComponent.NotifyPlayer(officerPlayerId, "#EL-Police_Title", "#EL-Rank_FineDenied", 3.0, EL_ENotificationType.WARNING);
			return;
		}

		if (!EL_PoliceUtils.IsSaneFineAmount(amount))
		{
			EL_Debug.Warn("Police", string.Format("fine refused: amount %1 out of range", amount));
			return;
		}

		if (!EL_PoliceUtils.FineClearsWanted(amount))
		{
			// A fine that clears no wanted level only drains cash, and the target stays wanted,
			// so it could be repeated until the target is broke.
			EL_Debug.Warn("Police", string.Format("fine refused: amount %1 clears no wanted level (minimum %2)", amount, EL_PoliceUtils.WANTED_STEP));
			if (officerPlayerId > 0)
				EL_NotificationManagerComponent.NotifyPlayer(officerPlayerId, "#EL-Fine_Title", "#EL-Fine_TooSmall", 3.0, EL_ENotificationType.WARNING);
			return;
		}

		EL_PlayerAccount targetAccount;
		IEntity target = EL_PoliceUtils.FindPlayerEntityByUid(targetPlayerUid, targetAccount);
		if (!target || !targetAccount || targetAccount.GetWantedLevel() <= 0)
		{
			EL_Debug.Warn("Police", string.Format("fine refused: target %1 is not wanted", targetPlayerUid));
			return;
		}

		if (!EL_PoliceUtils.IsTargetInReach(officer, target))
		{
			EL_Debug.Warn("Police", string.Format("fine refused: %1 is not within %2 m of the officer", targetPlayerUid, EL_PoliceUtils.POLICE_REACH));
			if (officerPlayerId > 0)
				EL_NotificationManagerComponent.NotifyPlayer(officerPlayerId, "#EL-Fine_Title", "#EL-Police_TooFar", 3.0, EL_ENotificationType.WARNING);
			return;
		}

		int targetPlayerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(target);
		string amountText = amount.ToString();

		int removed = EL_MoneyUtils.RemoveAmount(target, amount);
		if (removed != amount)
		{
			// Partial removals are not a fine: return the removed part and fail.
			if (removed > 0)
				EL_MoneyUtils.AddAmount(target, removed);
			EL_NotificationManagerComponent.NotifyPlayer(officerPlayerId, "#EL-Fine_Title", "#EL-Fine_Failed", 3.0, EL_ENotificationType.WARNING);
			return;
		}

		targetAccount.IncreaseWantedLevel(-EL_PoliceUtils.GetWantedReduction(amount));
		EL_PlayerAccountManager.GetInstance().SaveAndReleaseAccount(targetAccount);

		EL_NotificationManagerComponent.NotifyPlayer(officerPlayerId, "#EL-Police_Title", "#EL-Fined", 3.0, EL_ENotificationType.SUCCESS, amountText);
		if (targetPlayerId > 0)
			EL_NotificationManagerComponent.NotifyPlayer(targetPlayerId, "#EL-Police_Title", "#EL-Fined", 3.0, EL_ENotificationType.WARNING, amountText);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_EL_PoliceSelfPromote()
	{
		LF_PoliceRanks.RequestSelfPromotion(EL_Utils.GetPlayerUid(this));
	}
};

//------------------------------------------------------------------------------------------------
//! Pure police-domain helpers. Server-side validation and fine math live here so the EL_Test
//! suite can exercise them without a world.
class EL_PoliceUtils
{
	static const int MAX_FINE = 5000;

	//! Cash per wanted level a fine clears. A fine below this clears nothing and is refused.
	static const int WANTED_STEP = 1000;

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
	//! \param amount Fine amount in cash.
	//! \return true when the fine clears at least one wanted level.
	static bool FineClearsWanted(int amount)
	{
		return GetWantedReduction(amount) > 0;
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
			EL_Debug.Error("Jail", "jail position missing: no cell is placed");
			return "0 0 0";
		}

		return LF_Jail.GetCellOrigin();
	}

	//------------------------------------------------------------------------------------------------
	//! \param account Account to check, may be null.
	//! \return true when the account belongs to an on-duty police officer.
	static bool IsOnDutyOfficer(EL_PlayerAccount account)
	{
		if (!account)
			return false;
		if (account.GetFaction() != EL_Faction.POLICE)
			return false;
		return account.IsOnDuty();
	}

	//------------------------------------------------------------------------------------------------
	//! \param amount Fine amount in cash.
	//! \return Wanted levels the fine removes (1 per 1000 cash).
	static int GetWantedReduction(int amount)
	{
		return amount / WANTED_STEP;
	}

	//------------------------------------------------------------------------------------------------
	//! \param amount Fine amount in cash.
	//! \return true when the amount is above zero and within the maximum fine.
	static bool IsSaneFineAmount(int amount)
	{
		if (amount <= 0)
			return false;
		return amount <= MAX_FINE;
	}

	//------------------------------------------------------------------------------------------------
	//! \param entity Player character.
	//! \return The cached account for the character, or null.
	static EL_PlayerAccount GetAccountForEntity(IEntity entity)
	{
		if (!entity)
			return null;

		EL_PlayerAccountManager accountManager = EL_PlayerAccountManager.GetInstance();
		if (!accountManager)
			return null;

		return accountManager.GetAccount(EL_Utils.GetPlayerUID(entity));
	}

	//------------------------------------------------------------------------------------------------
	//! \param playerUid Platform-stable UID to match.
	//! \param outAccount The wanted player's cached account, or null when not found.
	//! \return The connected player entity matching the UID, or null.
	static IEntity FindPlayerEntityByUid(string playerUid, out EL_PlayerAccount outAccount)
	{
		outAccount = null;
		if (playerUid.IsEmpty())
			return null;

		EL_PlayerControllerManagerCompat playerManager = EL_PlayerControllerManagerCompat.GetInstance();
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
			if (entity && EL_Utils.GetPlayerUID(entity) == playerUid)
			{
				outAccount = GetAccountForEntity(entity);
				return entity;
			}
		}

		return null;
	}
};

//------------------------------------------------------------------------------------------------
class EL_PoliceMenu : ChimeraMenuBase
{
	protected PlayerController m_PlayerController;
	protected ref array<ref EL_WantedPlayerInfo> m_aWantedPlayers;

	//------------------------------------------------------------------------------------------------
	void SetPlayerController(PlayerController playerController)
	{
		m_PlayerController = playerController;
		RefreshWantedList();
	}

	//------------------------------------------------------------------------------------------------
	protected void RefreshWantedList()
	{
		m_aWantedPlayers = {};
		EL_PlayerControllerManagerCompat playerManager = EL_PlayerControllerManagerCompat.GetInstance();
		if (playerManager)
		{
			array<int> playerIds = {};
			playerManager.GetAllPlayerIds(playerIds);
			
			EL_PlayerAccountManager accountManager = EL_PlayerAccountManager.GetInstance();
			if (accountManager)
			{
				foreach (int playerId : playerIds)
				{
					PlayerController pc = playerManager.GetPlayerController(playerId);
					if (pc)
					{
						IEntity entity = pc.GetControlledEntity();
						string playerUid = EL_Utils.GetPlayerUID(entity);
						EL_PlayerAccount account = accountManager.GetAccount(playerUid);
						if (account && account.GetWantedLevel() > 0)
						{
							EL_WantedPlayerInfo info = new EL_WantedPlayerInfo();
							info.m_sPlayerUid = playerUid;

							EL_PlayerCharacter character = account.GetActiveCharacter();
							if (character)
								info.m_sPlayerName = character.GetFullName();
							else
								info.m_sPlayerName = EL_Utils.GetCharacterName(entity);

							info.m_iWantedLevel = account.GetWantedLevel();
							m_aWantedPlayers.Insert(info);
						}
					}
				}
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Client-side entry: routes the arrest request to the server via the officer's character.
	void ArrestPlayer(string playerUid)
	{
		SCR_ChimeraCharacter officer = GetOfficerCharacter();
		if (officer)
			officer.EL_AskPoliceArrest(playerUid);
	}

	//------------------------------------------------------------------------------------------------
	//! Client-side entry: routes the fine request to the server via the officer's character.
	void FinePlayer(string playerUid, int amount)
	{
		SCR_ChimeraCharacter officer = GetOfficerCharacter();
		if (officer)
			officer.EL_AskPoliceFine(playerUid, amount);
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

//------------------------------------------------------------------------------------------------
class EL_WantedPlayerInfo
{
	string m_sPlayerUid;
	string m_sPlayerName;
	int m_iWantedLevel;
};