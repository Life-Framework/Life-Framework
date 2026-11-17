//------------------------------------------------------------------------------------------------
//! One shared map mark (everon-map.md section 3, "Shared marks"). Server side; the wire carries its
//! plain fields only (LF_MapShareComponent), never the audience or the expiry. A row may also carry an
//! area: a circle of m_iAreaRadius metres around its position, drawn in the LF_MapAreaStyle m_iAreaStyle
//! (radius 0 is no area). The client registers it in LF_MapAreas beside the mark (LF_MapShareAreas).
class LF_MapShareRow : Managed
{
	//! The owning system's stable key: dispatch:<callId>, org:<orgId>:<n>, taxi:<fareId>, event:<eventId>.
	string m_sKey;
	LF_EMapShareAudience m_eAudience;
	//! Character id (PLAYER), service (ROLE: police, medic), org id (ORG); unused for EVERYONE.
	string m_sAudienceId;
	//! World x and z in metres.
	int m_iX;
	int m_iZ;
	//! Indexes into the shared-mark config.
	int m_iIcon;
	int m_iColour;
	//! A localization key or a short player text, at most TEXT_CAP characters.
	string m_sText;
	//! World seconds; 0 never expires.
	float m_fExpires;
	//! Optional area around the position: radius in metres (0 none) and an LF_MapAreaStyle id.
	int m_iAreaRadius;
	int m_iAreaStyle;

	//------------------------------------------------------------------------------------------------
	static LF_MapShareRow Make(string key, LF_EMapShareAudience audience, string audienceId, int x, int z, int icon, int colour, string text, float expires)
	{
		LF_MapShareRow row = new LF_MapShareRow();
		row.m_sKey = key;
		row.m_eAudience = audience;
		row.m_sAudienceId = audienceId;
		row.m_iX = x;
		row.m_iZ = z;
		row.m_iIcon = icon;
		row.m_iColour = colour;
		row.m_sText = text;
		row.m_fExpires = expires;
		return row;
	}

	//------------------------------------------------------------------------------------------------
	//! Gives the row an area (a circle around its position). eturn the row, so it chains off Make.
	LF_MapShareRow WithArea(int radius, LF_MapAreaStyle style)
	{
		m_iAreaRadius = radius;
		m_iAreaStyle = style;
		return this;
	}
}

//------------------------------------------------------------------------------------------------
//! Where LF_MapShares sends a row. The game sink is LF_MapShareRplSink (an owner RPC per recipient);
//! tests install a recording sink.
class LF_MapShareSink : Managed
{
	void Add(int playerId, LF_MapShareRow row) {}
	void Remove(int playerId, string key) {}
	//! Drop every shared mark the player's client holds (before a full resend on spawn).
	void Clear(int playerId) {}
}

//------------------------------------------------------------------------------------------------
//! Who is on the server and what they belong to. The game facts read the account and org managers;
//! tests install fixture facts.
class LF_MapShareFacts : Managed
{
	void Players(notnull array<int> outIds) {}

	//! Null when the player is not known yet (no account): such a player receives nothing.
	LF_MapShareMember Of(int playerId)
	{
		return null;
	}
}

//------------------------------------------------------------------------------------------------
class LF_MapShareGameFacts : LF_MapShareFacts
{
	//------------------------------------------------------------------------------------------------
	override void Players(notnull array<int> outIds)
	{
		outIds.Clear();
		PlayerManager players = GetGame().GetPlayerManager();
		if (players)
			players.GetPlayers(outIds);
	}

	//------------------------------------------------------------------------------------------------
	override LF_MapShareMember Of(int playerId)
	{
		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		if (!accounts)
			return null;

		LF_PlayerAccount account = accounts.GetAccount(playerId);
		if (!account)
			return null;

		string uid = account.GetPersistentId();
		LF_PlayerCharacter character = account.GetActiveCharacter();
		if (character)
			uid = character.GetId();

		array<string> roles = {};
		if (account.IsOnDuty() && account.GetFaction() == LF_Faction.POLICE)
			roles.Insert(LF_MapShares.ROLE_POLICE);

		if (LF_MedicDuty.IsOnDuty(account))
			roles.Insert(LF_MapShares.ROLE_MEDIC);

		string orgId;
		LF_Organization org = LF_OrganizationManager.GetByCharacter(uid);
		if (org)
			orgId = org.GetId();

		return LF_MapShareMember.Make(playerId, uid, roles, orgId);
	}
}

//------------------------------------------------------------------------------------------------
//! Sends each row by owner RPC on the recipient's controller (LF_MapShareComponent). Nobody outside
//! the audience gets a byte; nothing is broadcast.
class LF_MapShareRplSink : LF_MapShareSink
{
	//------------------------------------------------------------------------------------------------
	override void Add(int playerId, LF_MapShareRow row)
	{
		LF_MapShareComponent channel = LF_MapShareComponent.OfPlayer(playerId);
		if (channel)
			channel.ServerAdd(row);
	}

	//------------------------------------------------------------------------------------------------
	override void Remove(int playerId, string key)
	{
		LF_MapShareComponent channel = LF_MapShareComponent.OfPlayer(playerId);
		if (channel)
			channel.ServerRemove(key);
	}

	//------------------------------------------------------------------------------------------------
	override void Clear(int playerId)
	{
		LF_MapShareComponent channel = LF_MapShareComponent.OfPlayer(playerId);
		if (channel)
			channel.ServerClear();
	}
}

//------------------------------------------------------------------------------------------------
//! Bus subscriber: a player spawned (first join, respawn, restart load): resend their full set.
class LF_MapShareSpawnSubscriber : LF_EventSubscriber
{
	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnPlayerSpawnedEvent spawned = LF_OnPlayerSpawnedEvent.Cast(busEvent);
		if (!spawned)
			return false;

		LF_MapShares.OnSpawned(spawned.m_iPlayerId);
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The server's table of shared map marks (card M9, everon-map.md section 3). Owning systems (dispatch,
//! taxi, org marks, events) Put and Remove rows by key; this class resolves each row's audience and
//! sends it to exactly those players by owner RPC, never broadcast.
//! - Membership changes (on or off duty, joining or leaving an org) are picked up by Refresh(playerId),
//!   which owners may call at once, and by the sweep every SWEEP_MS for every player: the diff adds the
//!   rows a player now belongs to and removes the ones they left.
//! - A spawn (late join or respawn) clears the client's shared marks and resends the full set.
//! - Rows with an expiry are removed from everyone at that time.
//! No persistence: owners re-issue their rows (org marks are saved by M12 under their own payload).
class LF_MapShares
{
	static const string FEATURE = "Map";
	static const string ROLE_POLICE = "police";
	static const string ROLE_MEDIC = "medic";
	//! Most rows one player holds (and so one send carries).
	static const int SEND_CAP = 64; // lf-fixed: protocol cap from everon-map.md section 3, not a server tunable
	static const int TEXT_CAP = 32; // lf-fixed: wire limit of a shared-mark text (everon-map.md section 3)
	static const int SWEEP_MS = 5000; // lf-fixed: internal sweep cadence; expiry granularity, not gameplay

	protected static ref map<string, ref LF_MapShareRow> s_mRows;
	//! Keys in the order they were put, so sends are deterministic.
	protected static ref array<string> s_aOrder;
	//! What each player's client holds now, by player id.
	protected static ref map<int, ref array<string>> s_mDelivered;
	protected static ref LF_MapShareSink s_Sink;
	protected static ref LF_MapShareFacts s_Facts;
	protected static ref LF_MapShareSpawnSubscriber s_Subscriber;
	protected static bool s_bSweeping;

	//------------------------------------------------------------------------------------------------
	//! Server: subscribe to spawns and start the expiry and membership sweep. Safe to call again.
	static bool EnsureStarted()
	{
		if (!Replication.IsServer())
			return false;

		if (!s_Subscriber)
		{
			s_Subscriber = new LF_MapShareSpawnSubscriber();
			LF_EventBus.Subscribe(LF_OnPlayerSpawnedEvent, s_Subscriber);
		}

		if (!s_bSweeping && GetGame())
		{
			GetGame().GetCallqueue().CallLater(LF_MapShares.Sweep, SWEEP_MS, true);
			s_bSweeping = true;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		if (s_Subscriber)
			LF_EventBus.Unsubscribe(LF_OnPlayerSpawnedEvent, s_Subscriber);

		if (s_bSweeping && GetGame())
			GetGame().GetCallqueue().Remove(LF_MapShares.Sweep);

		s_Subscriber = null;
		s_bSweeping = false;
		s_mRows = null;
		s_aOrder = null;
		s_mDelivered = null;
		s_Sink = null;
		s_Facts = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: replace where rows go and who is on. Null restores the game's.
	static void SetSink(LF_MapShareSink sink)
	{
		s_Sink = sink;
	}

	//------------------------------------------------------------------------------------------------
	static void SetFacts(LF_MapShareFacts facts)
	{
		s_Facts = facts;
	}

	//------------------------------------------------------------------------------------------------
	//! World seconds, the clock row expiries are in.
	static float Now()
	{
		if (!GetGame() || !GetGame().GetWorld())
			return 0;

		return GetGame().GetWorld().GetWorldTime() / 1000;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: add a row, or replace the row with the same key, and send it to its audience. Players
	//! who held the key but are outside the new audience lose it. \return false when refused.
	static bool Put(LF_MapShareRow row)
	{
		if (!Replication.IsServer())
		{
			LF_Debug.Error(FEATURE, "share put refused: not the server");
			return false;
		}

		if (!row || row.m_sKey.IsEmpty())
		{
			LF_Debug.Error(FEATURE, "share put refused: no row or no key");
			return false;
		}

		if (row.m_eAudience == LF_EMapShareAudience.NONE)
		{
			LF_Debug.Warn(FEATURE, "share put refused: " + row.m_sKey + " has no audience");
			return false;
		}

		if (row.m_sText.Length() > TEXT_CAP)
			row.m_sText = row.m_sText.Substring(0, TEXT_CAP);

		if (row.m_iAreaRadius != 0 && !LF_MapShareRules.AreaValid(row.m_iAreaRadius, row.m_iAreaStyle))
		{
			LF_Debug.Warn(FEATURE, string.Format("share %1 area dropped: radius %2 style %3 is not drawable", row.m_sKey, row.m_iAreaRadius, row.m_iAreaStyle));
			row.m_iAreaRadius = 0;
			row.m_iAreaStyle = 0;
		}

		EnsureTables();
		if (!s_mRows.Contains(row.m_sKey))
			s_aOrder.Insert(row.m_sKey);

		s_mRows.Set(row.m_sKey, row);

		LF_MapShareFacts facts = Facts();
		LF_MapShareSink sink = Sink();
		array<int> players = {};
		facts.Players(players);
		int sent;
		foreach (int playerId : players)
		{
			array<string> delivered = DeliveredOf(playerId);
			bool had = delivered.Find(row.m_sKey) != -1;
			if (LF_MapShareRules.InAudience(row.m_eAudience, row.m_sAudienceId, facts.Of(playerId)))
			{
				if (!had && delivered.Count() >= SEND_CAP)
				{
					LF_Debug.Warn(FEATURE, string.Format("share %1 not sent to player %2: they hold %3 rows", row.m_sKey, playerId, SEND_CAP));
					continue;
				}

				sink.Add(playerId, row);
				if (!had)
					delivered.Insert(row.m_sKey);

				sent++;
			}
			else if (had)
			{
				sink.Remove(playerId, row.m_sKey);
				delivered.RemoveItemOrdered(row.m_sKey);
			}
		}

		LF_Debug.Info(FEATURE, string.Format("share %1 put (audience %2 %3): sent to %4 player(s)", row.m_sKey, typename.EnumToString(LF_EMapShareAudience, row.m_eAudience), row.m_sAudienceId, sent));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: remove a row from the table and from every client that holds it. \return false when unknown.
	static bool Remove(string key)
	{
		if (!s_mRows || !s_mRows.Contains(key))
			return false;

		s_mRows.Remove(key);
		s_aOrder.RemoveItemOrdered(key);

		LF_MapShareSink sink = Sink();
		int removed;
		foreach (int playerId, array<string> delivered : s_mDelivered)
		{
			if (delivered.Find(key) == -1)
				continue;

			delivered.RemoveItemOrdered(key);
			sink.Remove(playerId, key);
			removed++;
		}

		LF_Debug.Info(FEATURE, string.Format("share %1 removed from %2 player(s)", key, removed));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a player's membership may have changed. Sends the rows they now belong to and removes
	//! the ones they left. Owners call it at once (going on duty, joining an org); the sweep catches the rest.
	static void Refresh(int playerId)
	{
		EnsureTables();
		array<string> wanted = {};
		WantedFor(playerId, wanted);
		array<string> delivered = DeliveredOf(playerId);
		array<string> adds = {};
		array<string> removes = {};
		LF_MapShareRules.Diff(delivered, wanted, adds, removes);
		if (adds.IsEmpty() && removes.IsEmpty())
			return;

		LF_MapShareSink sink = Sink();
		foreach (string key : removes)
		{
			sink.Remove(playerId, key);
		}

		foreach (string key : adds)
		{
			sink.Add(playerId, s_mRows.Get(key));
		}

		delivered.Copy(wanted);
		LF_Debug.Info(FEATURE, string.Format("player %1 shares changed: +%2 -%3", playerId, adds.Count(), removes.Count()));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a player spawned (late join or respawn). Their client drops its shared marks and gets the
	//! full set for their audiences.
	static void OnSpawned(int playerId)
	{
		EnsureTables();
		s_mDelivered.Remove(playerId);
		Sink().Clear(playerId);
		Refresh(playerId);
		LF_Debug.Info(FEATURE, string.Format("player %1 spawned: resent %2 share(s)", playerId, DeliveredCount(playerId)));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: remove every row expired at now (world seconds). \return how many went.
	static int Expire(float now)
	{
		if (!s_aOrder)
			return 0;

		array<string> gone = {};
		foreach (string key : s_aOrder)
		{
			LF_MapShareRow row = s_mRows.Get(key);
			if (row && LF_MapShareRules.Expired(row.m_fExpires, now))
				gone.Insert(key);
		}

		foreach (string key : gone)
		{
			Remove(key);
		}

		return gone.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Server, every SWEEP_MS: expire rows, forget players who left, refresh everyone's membership.
	static void Sweep()
	{
		if (!Replication.IsServer())
			return;

		Expire(Now());
		RefreshAll();
	}

	//------------------------------------------------------------------------------------------------
	static void RefreshAll()
	{
		EnsureTables();
		array<int> players = {};
		Facts().Players(players);

		array<int> gone = {};
		foreach (int playerId, array<string> delivered : s_mDelivered)
		{
			if (players.Find(playerId) == -1)
				gone.Insert(playerId);
		}

		foreach (int playerId : gone)
		{
			s_mDelivered.Remove(playerId);
		}

		foreach (int playerId : players)
		{
			Refresh(playerId);
		}
	}

	//------------------------------------------------------------------------------------------------
	static int Count()
	{
		if (!s_mRows)
			return 0;

		return s_mRows.Count();
	}

	//------------------------------------------------------------------------------------------------
	static bool Has(string key)
	{
		return s_mRows && s_mRows.Contains(key);
	}

	//------------------------------------------------------------------------------------------------
	static int DeliveredCount(int playerId)
	{
		if (!s_mDelivered || !s_mDelivered.Contains(playerId))
			return 0;

		return s_mDelivered.Get(playerId).Count();
	}

	//------------------------------------------------------------------------------------------------
	static bool IsDelivered(int playerId, string key)
	{
		if (!s_mDelivered || !s_mDelivered.Contains(playerId))
			return false;

		return s_mDelivered.Get(playerId).Find(key) != -1;
	}

	//------------------------------------------------------------------------------------------------
	//! The keys a player should hold now, in put order, at most SEND_CAP.
	protected static void WantedFor(int playerId, notnull array<string> wanted)
	{
		wanted.Clear();
		LF_MapShareMember member = Facts().Of(playerId);
		if (!member)
			return;

		foreach (string key : s_aOrder)
		{
			LF_MapShareRow row = s_mRows.Get(key);
			if (!row || !LF_MapShareRules.InAudience(row.m_eAudience, row.m_sAudienceId, member))
				continue;

			if (wanted.Count() >= SEND_CAP)
			{
				LF_Debug.Warn(FEATURE, string.Format("player %1 is in more than %2 shares: the rest wait", playerId, SEND_CAP));
				return;
			}

			wanted.Insert(key);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static array<string> DeliveredOf(int playerId)
	{
		array<string> delivered = s_mDelivered.Get(playerId);
		if (!delivered)
		{
			delivered = {};
			s_mDelivered.Set(playerId, delivered);
		}

		return delivered;
	}

	//------------------------------------------------------------------------------------------------
	protected static void EnsureTables()
	{
		if (!s_mRows)
			s_mRows = new map<string, ref LF_MapShareRow>();

		if (!s_aOrder)
			s_aOrder = {};

		if (!s_mDelivered)
			s_mDelivered = new map<int, ref array<string>>();
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_MapShareSink Sink()
	{
		if (!s_Sink)
			s_Sink = new LF_MapShareRplSink();

		return s_Sink;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_MapShareFacts Facts()
	{
		if (!s_Facts)
			s_Facts = new LF_MapShareGameFacts();

		return s_Facts;
	}
}

//------------------------------------------------------------------------------------------------
//! Self-start: the share table listens from the world's start, on the server only.
modded class LF_GameModeRoleplay
{
	//------------------------------------------------------------------------------------------------
	protected override void OnGameStart()
	{
		super.OnGameStart();
		LF_MapShares.EnsureStarted();
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_MapShares' statics (registered at script load; Core/LF_StaticReset.c).
class LF_MapShares_StaticResetHook : LF_StaticResetHook
{
	protected static bool s_bRegistered = LF_StaticReset.Register(new LF_MapShares_StaticResetHook());

	override string Owner()
	{
		return "LF_MapShares";
	}

	override void Apply()
	{
		LF_MapShares.ResetStatics();
	}
}
