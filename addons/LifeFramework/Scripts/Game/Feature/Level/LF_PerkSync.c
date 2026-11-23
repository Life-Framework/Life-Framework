//------------------------------------------------------------------------------------------------
//! The owner's perk sync (#4525). The trade levels and perk picks live on the server, so a remote
//! client could not shorten its own gather hold (swift_hands), its cuff bar (fast_cuff) or label the
//! labour office counter rows without opening F2 first. The server sends each player their own
//! character's levels in the counter trades and every pick they hold, to their own body only
//! (RplRcver.Owner on LF_PlayerJobComponent, never a broadcast): at spawn, again a moment later in
//! case the body's owner was not set yet, after a pick or a change (LF_TradePerks.Pick) and after a
//! level rise in a counter trade. A client writes the picks into its own LF_TradePerks store, so the
//! perk effects read the same store on every machine. Nothing here moves money: the server reads its
//! own stores for every pick, change and payout.
//!
//! Wire: rows split by a newline, fields by "|": `U|<uid>` first, then `L|<trade>|<level>` and
//! `P|<trade>|<atLevel>|<perk>`. No field holds "|" (LF_TradePerks refuses it in a uid or trade).
class LF_PerkSync
{
	static const string FEATURE = "PerkSync";
	static const int RESEND_MS = 3000; // lf-fixed: second send after a spawn, once the body has its owner

	//! Client: the character the last sync was for, and its levels in the counter trades.
	protected static string s_sUid;
	protected static ref map<string, int> s_mLevels;
	//! Server: how many syncs were sent (tests, logs).
	protected static int s_iSent;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_sUid = string.Empty;
		s_mLevels = null;
		s_iSent = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the wire for one character. Empty for no character.
	static string Encode(string uid)
	{
		if (uid.IsEmpty() || uid.Contains("|"))
			return string.Empty;

		string wire = "U|" + uid;
		array<string> trades = {};
		LF_PerkPick.Trades(trades);
		foreach (string trade : trades)
		{
			int level = LF_JobLevels.LevelOf(uid, trade);
			if (level > 0)
				wire += "\nL|" + trade + "|" + level.ToString();
		}

		foreach (LF_TradePerkRow row : LF_TradePerks.RowsOf(uid))
		{
			wire += "\nP|" + row.m_sTrade + "|" + row.m_iAtLevel.ToString() + "|" + row.m_sPerk;
		}

		return wire;
	}

	//------------------------------------------------------------------------------------------------
	//! Client (and tests): takes a sync. The levels replace the last sync's; with `intoStore` the
	//! picks replace this character's rows in LF_TradePerks (a listen host passes false: its store is
	//! the server's own). False on a wire with no character.
	static bool Apply(string wire, bool intoStore)
	{
		array<string> lines = {};
		wire.Split("\n", lines, true);
		if (lines.IsEmpty() || !lines[0].StartsWith("U|"))
		{
			LF_Debug.Error(FEATURE, "a perk sync arrived with no character, ignored");
			return false;
		}

		string uid = lines[0].Substring(2, lines[0].Length() - 2); // lf-bounded: one short line
		if (uid.IsEmpty())
		{
			LF_Debug.Error(FEATURE, "a perk sync arrived with an empty character, ignored");
			return false;
		}

		map<string, int> levels = new map<string, int>();
		array<ref LF_TradePerkRow> rows = new array<ref LF_TradePerkRow>();
		for (int i = 1; i < lines.Count(); i++)
		{
			array<string> fields = {};
			lines[i].Split("|", fields, false);
			if (fields.Count() == 3 && fields[0] == "L")
				levels.Set(fields[1], fields[2].ToInt());
			else if (fields.Count() == 4 && fields[0] == "P")
				rows.Insert(LF_TradePerkRow.Create(uid, fields[1], fields[2].ToInt(), fields[3]));
			else
				LF_Debug.Warn(FEATURE, "a perk sync row is malformed and is skipped: " + lines[i]);
		}

		s_sUid = uid;
		s_mLevels = levels;
		int kept = 0;
		if (intoStore)
			kept = LF_TradePerks.ReplaceFor(uid, rows);

		LF_Debug.Info(FEATURE, string.Format("perk sync for %1: %2 level(s), %3 perk(s) (%4 into the store)", uid, levels.Count(), rows.Count(), kept));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: true once a sync for this character arrived.
	static bool Ready(string uid)
	{
		return !uid.IsEmpty() && uid == s_sUid && s_mLevels != null;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the character's level in a counter trade from the last sync; 0 when none.
	static int LevelOf(string uid, string trade)
	{
		if (!Ready(uid))
			return 0;

		int level;
		s_mLevels.Find(trade, level);
		return level;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: sends the character's sync to every live body of theirs. \return Bodies sent to.
	static int Push(string uid)
	{
		if (uid.IsEmpty() || !Replication.IsServer() || !GetGame())
			return 0;

		PlayerManager players = GetGame().GetPlayerManager();
		if (!players)
			return 0;

		array<int> playerIds = {};
		players.GetPlayers(playerIds);
		string wire;
		int sent = 0;
		foreach (int playerId : playerIds)
		{
			if (LF_Utils.GetPlayerUID(playerId) != uid)
				continue;

			if (wire.IsEmpty())
				wire = Encode(uid);

			if (SendTo(players.GetPlayerControlledEntity(playerId), wire))
				sent++;
		}

		return sent;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: sends one player's sync to their body (the spawn and its second send).
	static bool PushPlayer(int playerId)
	{
		if (playerId <= 0 || !Replication.IsServer() || !GetGame() || !GetGame().GetPlayerManager())
			return false;

		string uid = LF_Utils.GetPlayerUID(playerId);
		if (uid.IsEmpty())
			return false;

		return SendTo(GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId), Encode(uid));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: sends a character's sync to one body (the spawn hook: the body may not be registered as
	//! controlled yet, so it is passed in).
	static bool SendToBody(IEntity body, string uid)
	{
		if (!Replication.IsServer())
			return false;

		return SendTo(body, Encode(uid));
	}

	//------------------------------------------------------------------------------------------------
	protected static bool SendTo(IEntity body, string wire)
	{
		if (!body || wire.IsEmpty())
			return false;

		LF_PlayerJobComponent jobs = LF_Component<LF_PlayerJobComponent>.Find(body);
		if (!jobs)
			return false;

		jobs.LF_SendPerkSync(wire);
		s_iSent++;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static int SentCount()
	{
		return s_iSent;
	}

	//------------------------------------------------------------------------------------------------
	//! True for a trade whose level the sync carries.
	static bool IsCounterTrade(string job)
	{
		array<string> trades = {};
		LF_PerkPick.Trades(trades);
		return trades.Contains(job);
	}
}

//------------------------------------------------------------------------------------------------
//! Server: a level rise in a counter trade sends the owner a fresh sync (new counter rows).
class LF_PerkSyncLevels : LF_EventSubscriber
{
	protected static ref LF_PerkSyncLevels s_Subscriber;

	//------------------------------------------------------------------------------------------------
	static bool EnsureSubscribed()
	{
		if (!Replication.IsServer())
			return false;

		if (s_Subscriber)
			return true;

		s_Subscriber = new LF_PerkSyncLevels();
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
		if (changed && LF_PerkSync.IsCounterTrade(changed.m_sJob))
			LF_PerkSync.Push(changed.m_sPlayerUid);

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_PerkSync's statics (LF_StaticReset finds it by reflection).
class LF_PerkSync_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_PerkSync";
	}

	override void Apply()
	{
		LF_PerkSync.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_PerkSyncLevels's statics (LF_StaticReset finds it by reflection).
class LF_PerkSyncLevels_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_PerkSyncLevels";
	}

	override void Apply()
	{
		LF_PerkSyncLevels.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
//! The sync RPC on the player's job component: the server tells the owner, never another player.
modded class LF_PlayerJobComponent
{
	//------------------------------------------------------------------------------------------------
	//! Server: sends this body's owner their perk sync. Ids and numbers only, never an entity id.
	void LF_SendPerkSync(string wire)
	{
		if (!Replication.IsServer())
			return;

		Rpc(RpcDo_LF_PerkSync, wire);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_PerkSync(string wire)
	{
		// A listen host runs this inside its own Rpc: its store is the server's and stays as it is.
		LF_PerkSync.Apply(wire, !Replication.IsServer());
	}
}

//------------------------------------------------------------------------------------------------
//! Game start subscribes the level hook; a spawn sends the owner their sync, twice.
modded class LF_GameModeRoleplay
{
	//------------------------------------------------------------------------------------------------
	protected override void OnGameStart()
	{
		super.OnGameStart();
		LF_PerkSyncLevels.EnsureSubscribed();
	}

	//------------------------------------------------------------------------------------------------
	override void OnPlayerSpawned(int playerId, IEntity controlledEntity)
	{
		super.OnPlayerSpawned(playerId, controlledEntity);
		if (!Replication.IsServer())
			return;

		string uid = LF_Utils.GetPlayerUID(playerId);
		if (!uid.IsEmpty() && controlledEntity)
			LF_PerkSync.SendToBody(controlledEntity, uid);

		GetGame().GetCallqueue().CallLater(LF_PerkSync.PushPlayer, LF_PerkSync.RESEND_MS, false, playerId);
	}
}
