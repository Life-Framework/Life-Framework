// LF_LevelUpCard.c: the level-up moment (#4404). When a trade level rises the server tells the player's
// own client (an Owner RPC on LF_PlayerJobComponent) and the client shows a small HUD card for a few
// seconds: the trade, the new level, its tier word and "Opens: <unlock>". One card for every trade,
// police and medic included. The text is built here, pure, so a LOGIC test drives it
// (LF_Test_LevelUpCard); the card widget is LF_LevelUpCardView.

//------------------------------------------------------------------------------------------------
//! What one level-up card says, as keys and numbers. Plain values, nothing from the world.
class LF_LevelUpCardLines
{
	string m_sJob;
	int m_iLevel;
	string m_sTradeName;
	string m_sTierKey;
	//! The unlock text key of the new level, empty when no row names one (the card shows no "Opens:" line).
	string m_sUnlockKey;

	//------------------------------------------------------------------------------------------------
	//! \return "Level 7", printed.
	string LevelLine()
	{
		return LF_Terms.Text("#LF-PlayerMenu_SkillLevel", m_iLevel.ToString());
	}

	//------------------------------------------------------------------------------------------------
	//! \return The printed tier word, "Journeyman".
	string TierWord()
	{
		return LF_Terms.Text(m_sTierKey);
	}

	//------------------------------------------------------------------------------------------------
	//! \return "Opens: Ore buyer at the depot", or empty when this level opens nothing. A milestone row
	//! (#4869) opens nothing: its text is shown as it is, with no "Opens:".
	string OpensLine()
	{
		if (m_sUnlockKey.IsEmpty())
			return string.Empty;

		if (LF_TradeUnlocks.IsMilestoneKey(m_sUnlockKey))
			return LF_Terms.Text(m_sUnlockKey);

		return LF_Terms.Text("#LF-LevelUpCard_Opens", LF_Terms.Text(m_sUnlockKey));
	}
}

//------------------------------------------------------------------------------------------------
class LF_LevelUpCard
{
	static const string FEATURE = "Level";
	//! The sound slot the sting plays (the pack fills it; WK-W3_2 makes the sound).
	static const string SOUND_SLOT = "level_up";

	//------------------------------------------------------------------------------------------------
	//! \return The card for a trade that went from levelBefore to level, or null when the level did not
	//! rise (a change of 0 levels builds nothing). The unlock is the one of the new level; the tier word
	//! is the F2 card's (LF_PlayerMenuTrade.TierKeyFor).
	static LF_LevelUpCardLines Build(string job, int levelBefore, int level)
	{
		if (level <= levelBefore || level < 1)
			return null;

		LF_LevelUpCardLines lines = new LF_LevelUpCardLines();
		lines.m_sJob = job;
		lines.m_iLevel = level;
		lines.m_sTradeName = LF_PlayerMenuSheet.SkillName(job);
		lines.m_sTierKey = LF_PlayerMenuTrade.TierKeyFor(level);
		lines.m_sUnlockKey = LF_TradeUnlocks.KeyFor(job, level);
		return lines;
	}
}

//------------------------------------------------------------------------------------------------
//! Server: hears every LF_OnJobLevelChanged and sends the player the card.
class LF_LevelUpCardServer : LF_EventSubscriber
{
	protected static ref LF_LevelUpCardServer s_Subscriber;

	//------------------------------------------------------------------------------------------------
	static bool EnsureSubscribed()
	{
		if (!Replication.IsServer())
			return false;

		if (s_Subscriber && LF_EventBus.SubscriberCount(LF_OnJobLevelChanged) > 0)
			return true;

		s_Subscriber = new LF_LevelUpCardServer();
		return LF_EventBus.Subscribe(LF_OnJobLevelChanged, s_Subscriber);
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_Subscriber = null;
	}

	//------------------------------------------------------------------------------------------------
	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnJobLevelChanged changed = LF_OnJobLevelChanged.Cast(busEvent);
		if (!changed || changed.m_iLevel <= changed.m_iLevelBefore)
			return true;

		int sent = Send(changed.m_sPlayerUid, changed.m_sJob, changed.m_iLevelBefore, changed.m_iLevel);
		LF_Debug.Info(LF_LevelUpCard.FEATURE, string.Format("level-up card %1 %2>%3 for %4 sent to %5 live player(s)", changed.m_sJob, changed.m_iLevelBefore, changed.m_iLevel, changed.m_sPlayerUid, sent));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return How many connected players of this character took the card.
	static int Send(string playerUid, string job, int levelBefore, int level)
	{
		if (playerUid.IsEmpty() || !GetGame())
			return 0;

		PlayerManager players = GetGame().GetPlayerManager();
		if (!players)
			return 0;

		array<int> playerIds = {};
		players.GetPlayers(playerIds);
		int sent = 0;
		foreach (int playerId : playerIds)
		{
			if (LF_Utils.GetPlayerUID(playerId) != playerUid)
				continue;

			LF_PlayerJobComponent jobs = LF_Component<LF_PlayerJobComponent>.Find(players.GetPlayerControlledEntity(playerId));
			if (!jobs)
				continue;

			jobs.LF_SendLevelUp(job, levelBefore, level);
			sent++;
		}

		return sent;
	}
}

//------------------------------------------------------------------------------------------------
class LF_LevelUpCardServer_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_LevelUpCardServer";
	}

	override void Apply()
	{
		LF_LevelUpCardServer.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
//! The card RPC on the player's job component: the server tells the owner, the owner shows it.
modded class LF_PlayerJobComponent
{
	//------------------------------------------------------------------------------------------------
	//! Server: tells this body's owner a trade level rose. Job id and levels only, never an entity id.
	void LF_SendLevelUp(string job, int levelBefore, int level)
	{
		if (!Replication.IsServer())
			return;

		Rpc(RpcDo_LF_LevelUp, job, levelBefore, level);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_LevelUp(string job, int levelBefore, int level)
	{
		LF_LevelUpCardView.Show(job, levelBefore, level);
	}
}

//------------------------------------------------------------------------------------------------
//! The game start subscribes the card on the server.
modded class LF_GameModeRoleplay
{
	//------------------------------------------------------------------------------------------------
	protected override void OnGameStart()
	{
		super.OnGameStart();
		LF_LevelUpCardServer.EnsureSubscribed();
	}
}
