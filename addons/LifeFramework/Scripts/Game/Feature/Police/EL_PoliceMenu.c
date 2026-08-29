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
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_EL_PoliceArrest(string targetPlayerUid)
	{
		IEntity officer = this;
		if (!officer)
			return;

		int officerPlayerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(officer);
		if (!EL_PoliceUtils.IsOnDutyOfficer(EL_PoliceUtils.GetAccountForEntity(officer)))
		{
			Print(string.Format("[EL_PoliceMenu] Arrest denied: %1 is not an on-duty officer", officerPlayerId), LogLevel.WARNING);
			return;
		}

		EL_PlayerAccount targetAccount;
		IEntity target = EL_PoliceUtils.FindPlayerEntityByUid(targetPlayerUid, targetAccount);
		if (!target || !targetAccount || targetAccount.GetWantedLevel() <= 0)
		{
			Print(string.Format("[EL_PoliceMenu] Arrest denied: target %1 is not wanted", targetPlayerUid), LogLevel.WARNING);
			return;
		}

		target.SetOrigin(EL_PoliceUtils.GetJailPosition());

		targetAccount.SetWantedLevel(0);
		EL_PlayerAccountManager.GetInstance().SaveAndReleaseAccount(targetAccount);

		int targetPlayerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(target);
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
		if (!EL_PoliceUtils.IsOnDutyOfficer(EL_PoliceUtils.GetAccountForEntity(officer)))
		{
			Print(string.Format("[EL_PoliceMenu] Fine denied: %1 is not an on-duty officer", officerPlayerId), LogLevel.WARNING);
			return;
		}

		if (!EL_PoliceUtils.IsSaneFineAmount(amount))
		{
			Print(string.Format("[EL_PoliceMenu] Fine denied: amount %1 out of range", amount), LogLevel.WARNING);
			return;
		}

		EL_PlayerAccount targetAccount;
		IEntity target = EL_PoliceUtils.FindPlayerEntityByUid(targetPlayerUid, targetAccount);
		if (!target || !targetAccount || targetAccount.GetWantedLevel() <= 0)
		{
			Print(string.Format("[EL_PoliceMenu] Fine denied: target %1 is not wanted", targetPlayerUid), LogLevel.WARNING);
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
};

//------------------------------------------------------------------------------------------------
//! Pure police-domain helpers. Server-side validation and fine math live here so the EL_Test
//! suite can exercise them without a world.
class EL_PoliceUtils
{
	static const int MAX_FINE = 5000;

	//------------------------------------------------------------------------------------------------
	//! \return Position arrested players are teleported to (near the DebugWorld police desk).
	static vector GetJailPosition()
	{
		return "127 1 137";
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
		return amount / 1000;
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