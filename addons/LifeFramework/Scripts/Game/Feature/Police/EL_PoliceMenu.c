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
						string playerUid = EL_Utils.GetPlayerUID(pc.GetControlledEntity());
						EL_PlayerAccount account = accountManager.GetAccount(playerUid);
						if (account && account.GetWantedLevel() > 0)
						{
							EL_WantedPlayerInfo info = new EL_WantedPlayerInfo();
							info.m_sPlayerUid = playerUid;
							info.m_sPlayerName = account.GetActiveCharacter().GetFullName();
							info.m_iWantedLevel = account.GetWantedLevel();
							m_aWantedPlayers.Insert(info);
						}
					}
				}
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	void ArrestPlayer(string playerUid)
	{
		if (!Replication.IsServer())
			return;

		// Find player and arrest
		EL_PlayerControllerManagerCompat playerManager = EL_PlayerControllerManagerCompat.GetInstance();
		if (playerManager)
		{
			array<int> playerIds = {};
			playerManager.GetAllPlayerIds(playerIds);
			
			foreach (int playerId : playerIds)
			{
				PlayerController pc = playerManager.GetPlayerController(playerId);
				if (pc && EL_Utils.GetPlayerUID(pc.GetControlledEntity()) == playerUid)
				{
					// Teleport to jail or something
					vector jailPos = "0 0 0"; // Set jail position
					pc.GetControlledEntity().SetOrigin(jailPos);
					
					// Reset wanted level
					EL_PlayerAccountManager accountManager = EL_PlayerAccountManager.GetInstance();
					EL_PlayerAccount account = accountManager.GetAccount(playerUid);
					if (account)
					{
						account.SetWantedLevel(0);
						accountManager.SaveAndReleaseAccount(account);
					}
					
					EL_Utils.Notify("#EL-Player_Arrested", "#EL-Police_Title", 3.0);
					break;
				}
			}
		}
		RefreshWantedList();
	}

	//------------------------------------------------------------------------------------------------
	void FinePlayer(string playerUid, int amount)
	{
		if (!Replication.IsServer())
			return;

		// Find player
		EL_PlayerControllerManagerCompat playerManager = EL_PlayerControllerManagerCompat.GetInstance();
		if (playerManager)
		{
			array<int> playerIds = {};
			playerManager.GetAllPlayerIds(playerIds);
			
			foreach (int playerId : playerIds)
			{
				PlayerController pc = playerManager.GetPlayerController(playerId);
				if (pc && EL_Utils.GetPlayerUID(pc.GetControlledEntity()) == playerUid)
				{
					IEntity target = pc.GetControlledEntity();
					if (EL_MoneyUtils.RemoveAmount(target, amount))
					{
						// Reduce wanted level, 1 per 1000
						int reduce = amount / 1000;
						EL_PlayerAccountManager accountManager = EL_PlayerAccountManager.GetInstance();
						EL_PlayerAccount account = accountManager.GetAccount(playerUid);
						if (account)
						{
							account.IncreaseWantedLevel(-reduce);
							accountManager.SaveAndReleaseAccount(account);
						}
						EL_Utils.Notify(string.Format("#EL-Fined %1!", amount), "#EL-Police_Title", 3.0);
					}
					else
					{
						EL_Utils.Notify("#EL-Fine_Failed", "#EL-Fine_Title", 3.0);
					}
					break;
				}
			}
		}
		RefreshWantedList();
	}
}

class EL_WantedPlayerInfo
{
	string m_sPlayerUid;
	string m_sPlayerName;
	int m_iWantedLevel;
};