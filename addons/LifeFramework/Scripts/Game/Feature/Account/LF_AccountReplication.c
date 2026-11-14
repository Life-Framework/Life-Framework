//------------------------------------------------------------------------------------------------
//! Player-facing account fields packed into one int, so a character can carry them as a single
//! simple-type RplProp. Faction, duty, wanted level, and police rank are what client-side action
//! offers and menus need. Nothing else of the account leaves the server.
//!
//! Layout: bit 0 has-account, bit 1 on-duty, bits 2-4 faction, bits 5-7 wanted, bits 8-15 rank,
//! bit 16 on the police roster (whitelisted; the duty desk offers Clock in).
//! Values outside a field's range are clamped, never wrapped into a neighbour field.
//------------------------------------------------------------------------------------------------
class LF_AccountStateBits
{
	static const int HAS_ACCOUNT = 1;
	static const int ON_DUTY = 2;
	static const int FACTION_SHIFT = 2;
	static const int FACTION_MASK = 7;
	static const int WANTED_SHIFT = 5;
	static const int WANTED_MASK = 7;
	static const int RANK_SHIFT = 8;
	static const int RANK_MASK = 255;
	static const int ON_ROSTER = 1 << 16;

	//------------------------------------------------------------------------------------------------
	//! \return The packed state. No account is 0, which reads back as a civilian, off duty, not wanted.
	static int Encode(bool hasAccount, int faction, bool onDuty, int wantedLevel, int policeRank, bool onRoster = false)
	{
		if (!hasAccount)
			return 0;

		int state = HAS_ACCOUNT;
		if (onDuty)
			state = state | ON_DUTY;

		state = state | (Math.ClampInt(faction, 0, FACTION_MASK) << FACTION_SHIFT);
		state = state | (Math.ClampInt(wantedLevel, 0, WANTED_MASK) << WANTED_SHIFT);
		state = state | (Math.ClampInt(policeRank, 0, RANK_MASK) << RANK_SHIFT);
		if (onRoster)
			state = state | ON_ROSTER;

		return state;
	}

	//------------------------------------------------------------------------------------------------
	static int FromAccount(LF_PlayerAccount account)
	{
		if (!account)
			return 0;

		bool onRoster = LF_WhitelistManager.IsWhitelistedForFaction(account.GetPersistentId(), LF_EFactionType.POLICE);
		return Encode(true, account.GetFaction(), account.IsOnDuty(), account.GetWantedLevel(), account.GetPoliceRank(), onRoster);
	}

	//------------------------------------------------------------------------------------------------
	static bool HasAccount(int state)
	{
		return (state & HAS_ACCOUNT) != 0;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsOnDuty(int state)
	{
		return (state & ON_DUTY) != 0;
	}

	//------------------------------------------------------------------------------------------------
	static LF_Faction GetFaction(int state)
	{
		return (state >> FACTION_SHIFT) & FACTION_MASK;
	}

	//------------------------------------------------------------------------------------------------
	static int GetWantedLevel(int state)
	{
		return (state >> WANTED_SHIFT) & WANTED_MASK;
	}

	//------------------------------------------------------------------------------------------------
	static int GetPoliceRank(int state)
	{
		return (state >> RANK_SHIFT) & RANK_MASK;
	}

	//------------------------------------------------------------------------------------------------
	//! Whitelisted for police. Only the duty desk offer reads it; the server re-checks the file.
	static bool IsOnRoster(int state)
	{
		return HasAccount(state) && (state & ON_ROSTER) != 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Clocked in as police. The faction is the duty role, so POLICE means on duty.
	static bool IsPolice(int state)
	{
		return HasAccount(state) && GetFaction(state) == LF_Faction.POLICE;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsOnDutyPolice(int state)
	{
		return IsPolice(state) && IsOnDuty(state);
	}

	//------------------------------------------------------------------------------------------------
	//! Civilian gate for crime offers. A client without an account in the state is refused too:
	//! the server refuses a robbery without an account, so the offer must not promise one.
	static bool IsCivilian(int state)
	{
		return HasAccount(state) && GetFaction(state) == LF_Faction.CIVILIAN;
	}
}

//------------------------------------------------------------------------------------------------
//! Server: which connected player(s) play an account uid, for LF_AccountReplication.Publish.
//! Publish used to walk every player per account change, and refresh the police count (another
//! walk) each time: D changes in a frame cost 2*D*N identity reads, 32768 at 128 players (#716).
//! Here a cached owner is checked with one read; a miss walks the players once and caches every
//! owner, and further misses in the same frame (offline accounts) cost nothing. EndFrame allows
//! the next walk. A frame of D changes now costs at most N + D reads.
//------------------------------------------------------------------------------------------------
class LF_AccountOwnerIndex
{
	protected ref map<string, ref array<int>> m_mOwners = new map<string, ref array<int>>();
	protected int m_iScanFrame = -1;
	protected bool m_bScanValid;
	protected int m_iUidReads;
	protected int m_iScans;

	//------------------------------------------------------------------------------------------------
	//! Fills owners with the player ids playing uid (normally one, empty when nobody does).
	//! Returns false when the answer is "nobody" taken from a walk earlier this frame, so a player
	//! whose identity resolved since then could be missed; the caller may ask again next frame.
	bool Resolve(string uid, notnull array<int> owners)
	{
		owners.Clear();
		if (uid.IsEmpty())
			return true;

		array<int> cached = m_mOwners.Get(uid);
		if (cached && IsCurrent(uid, cached))
		{
			owners.Copy(cached);
			return true;
		}

		if (m_bScanValid && m_iScanFrame == CurrentFrame())
			return false;

		Scan();
		cached = m_mOwners.Get(uid);
		if (cached)
			owners.Copy(cached);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The next miss may walk the players again. A new frame does this by itself.
	void EndFrame()
	{
		m_bScanValid = false;
	}

	//------------------------------------------------------------------------------------------------
	int GetUidReads()
	{
		return m_iUidReads;
	}

	//------------------------------------------------------------------------------------------------
	int GetScans()
	{
		return m_iScans;
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsCurrent(string uid, array<int> cached)
	{
		if (cached.IsEmpty())
			return false;

		foreach (int playerId : cached)
		{
			if (ReadUid(playerId) != uid)
				return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void Scan()
	{
		m_bScanValid = true;
		m_iScanFrame = CurrentFrame();
		m_iScans++;
		LF_Perf.Count("Account.ownerScan");
		m_mOwners.Clear();

		array<int> playerIds = {};
		ListPlayers(playerIds);
		foreach (int playerId : playerIds)
		{
			string uid = ReadUid(playerId);
			if (uid.IsEmpty())
				continue;

			array<int> owners = m_mOwners.Get(uid);
			if (!owners)
			{
				owners = {};
				m_mOwners.Set(uid, owners);
			}

			owners.Insert(playerId);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected string ReadUid(int playerId)
	{
		m_iUidReads++;
		return UidOf(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Overridden by the cost test with synthetic players.
	protected void ListPlayers(notnull array<int> playerIds)
	{
		if (!GetGame() || !GetGame().GetPlayerManager())
			return;

		GetGame().GetPlayerManager().GetPlayers(playerIds);
	}

	//------------------------------------------------------------------------------------------------
	protected int CurrentFrame()
	{
		if (!GetGame() || !GetGame().GetWorld())
			return -1;

		return GetGame().GetWorld().GetFrameNumber();
	}

	//------------------------------------------------------------------------------------------------
	protected string UidOf(int playerId)
	{
		return LF_Utils.GetAccountUID(playerId);
	}
}

//------------------------------------------------------------------------------------------------
//! Server -> client bridge for the account cache. LF_PlayerAccountManager lives on the server
//! only, so client-side CanBePerformed / CanBeShown / menus read the replicated copy instead:
//!   - the packed state above on each player's character (SCR_ChimeraCharacter RplProp), and
//!   - the on-duty police count on the game mode (LF_GameModeRoleplay RplProp).
//! Both are written on the server only and bumped only when the value changes. They are plain
//! RplProps, so a late joiner receives the current values with the entity snapshot.
//! The server stays authoritative: every PerformAction / RPC re-checks the real account.
//------------------------------------------------------------------------------------------------
class LF_AccountReplication
{
	//! Seconds between the server's self-healing pass over every connected player.
	static const int REFRESH_INTERVAL_MS = 10000;

	protected static ref LF_AccountOwnerIndex s_OwnerIndex = new LF_AccountOwnerIndex();
	protected static bool s_bFlushQueued;
	protected static int s_iFlushFrame;
	//! Accounts whose owner lookup missed on a walk made earlier in the same frame: retried once.
	protected static ref array<ref LF_PlayerAccount> s_aRetry = {};

	//------------------------------------------------------------------------------------------------
	//! Server: an account field changed. Pushes the packed state to the character of the player
	//! playing that account, at once, and queues one police-count refresh for the end of the frame.
	//! No connected player for the account is a no-op. The owner comes from LF_AccountOwnerIndex,
	//! so a burst of changes in one frame (load, payday) walks the players at most once (#716).
	static void Publish(LF_PlayerAccount account)
	{
		if (!account || !Replication.IsServer() || !GetGame())
			return;

		string uid = account.GetPersistentId();
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (uid.IsEmpty() || !playerManager)
			return;

		LF_Perf.Count("Account.publish");
		if (!WriteOwners(account, uid, playerManager) && s_aRetry.Find(account) < 0)
			s_aRetry.Insert(account);

		QueueFlush();
	}

	//------------------------------------------------------------------------------------------------
	//! Returns false when the owner lookup was not fresh (see LF_AccountOwnerIndex.Resolve).
	protected static bool WriteOwners(LF_PlayerAccount account, string uid, PlayerManager playerManager)
	{
		array<int> owners = {};
		bool fresh = s_OwnerIndex.Resolve(uid, owners);
		foreach (int playerId : owners)
		{
			SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(playerManager.GetPlayerControlledEntity(playerId));
			if (character)
			{
				character.LF_SetAccountState(LF_AccountStateBits.FromAccount(account));
				character.LF_SetCharacterKey(account.GetActiveCharacterKey());
			}
		}

		return fresh;
	}

	//------------------------------------------------------------------------------------------------
	//! One end-of-frame pass however many accounts changed: the police count is counted once and
	//! the owner index may walk the players again next frame.
	protected static void QueueFlush()
	{
		if (!GetGame() || !GetGame().GetCallqueue())
			return;

		int frame = -1;
		if (GetGame().GetWorld())
			frame = GetGame().GetWorld().GetFrameNumber();

		// A queued flush runs next frame; one still pending two frames on was dropped (world change).
		if (s_bFlushQueued && frame >= s_iFlushFrame && frame - s_iFlushFrame < 2)
			return;

		s_bFlushQueued = true;
		s_iFlushFrame = frame;
		GetGame().GetCallqueue().CallLater(Flush, 0);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Flush()
	{
		s_bFlushQueued = false;
		s_OwnerIndex.EndFrame();

		// One more lookup for the misses, on a fresh walk; a second miss means offline.
		PlayerManager playerManager;
		if (GetGame())
			playerManager = GetGame().GetPlayerManager();
		if (playerManager)
		{
			foreach (LF_PlayerAccount account : s_aRetry)
			{
				if (account)
					WriteOwners(account, account.GetPersistentId(), playerManager);
			}
		}
		s_aRetry.Clear();

		RefreshPoliceOnDuty();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: write the account state onto a freshly controlled character. A character with no
	//! account yet gets 0 (no account) until the creation flow publishes one.
	static void PublishForPlayer(int playerId, IEntity controlled)
	{
		if (!Replication.IsServer() || playerId <= 0)
			return;

		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(controlled);
		if (!character)
			return;

		LF_PlayerAccount account;
		LF_PlayerAccountManager accountManager = LF_PlayerAccountManager.GetInstance();
		string uid = LF_Utils.GetAccountUID(playerId);
		if (accountManager && !uid.IsEmpty())
			account = accountManager.GetAccount(uid);

		character.LF_SetAccountState(LF_AccountStateBits.FromAccount(account));
		if (account)
			character.LF_SetCharacterKey(account.GetActiveCharacterKey());
		// Organization copy for the offer side (M1-17), on the same spawn hook and periodic pass.
		LF_OrgReplication.Publish(character);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: periodic pass. Re-publishes every connected player's character and the police
	//! count, so a missed hook (respawn path, disconnect) converges within one interval.
	static void RefreshAll()
	{
		if (!Replication.IsServer() || !GetGame())
			return;

		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return;

		int perfToken = LF_Perf.Begin();
		s_OwnerIndex.EndFrame();
		array<int> playerIds = {};
		playerManager.GetPlayers(playerIds);
		foreach (int playerId : playerIds)
		{
			PublishForPlayer(playerId, playerManager.GetPlayerControlledEntity(playerId));
		}

		RefreshPoliceOnDuty();
		LF_Perf.End("Account.refreshAll", perfToken);
	}

	//------------------------------------------------------------------------------------------------
	static void RefreshPoliceOnDuty()
	{
		if (!Replication.IsServer() || !GetGame())
			return;

		LF_GameModeRoleplay gameMode = LF_GameModeRoleplay.Cast(GetGame().GetGameMode());
		if (gameMode)
			gameMode.LF_SetPoliceOnDuty(CountPoliceOnDuty());
	}

	//------------------------------------------------------------------------------------------------
	//! Server-authoritative count of connected on-duty police.
	static int CountPoliceOnDuty()
	{
		if (!GetGame())
			return 0;

		PlayerManager playerManager = GetGame().GetPlayerManager();
		LF_PlayerAccountManager accountManager = LF_PlayerAccountManager.GetInstance();
		if (!playerManager || !accountManager)
			return 0;

		int count = 0;
		array<int> playerIds = {};
		playerManager.GetPlayers(playerIds);
		foreach (int playerId : playerIds)
		{
			LF_PlayerAccount account = accountManager.GetAccount(LF_Utils.GetAccountUID(playerId));
			if (LF_AccountStateBits.IsOnDutyPolice(LF_AccountStateBits.FromAccount(account)))
				count++;
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! Offer-side read for any character. The server (dedicated or listen host) answers from the
	//! account cache; a client answers from the replicated copy on the character.
	static int GetState(IEntity character)
	{
		if (!character)
			return 0;

		if (Replication.IsServer())
		{
			LF_PlayerAccountManager accountManager = LF_PlayerAccountManager.GetInstance();
			string uid = LF_Utils.GetPlayerUid(character);
			if (!accountManager || uid.IsEmpty())
				return 0;

			return LF_AccountStateBits.FromAccount(accountManager.GetAccount(uid));
		}

		SCR_ChimeraCharacter chimera = SCR_ChimeraCharacter.Cast(character);
		if (!chimera)
			return 0;

		return chimera.LF_GetAccountState();
	}

	//------------------------------------------------------------------------------------------------
	//! Offer-side police count. Server counts live; a client reads the game mode's replicated count.
	static int GetPoliceOnDutyCount()
	{
		if (Replication.IsServer())
			return CountPoliceOnDuty();

		if (!GetGame())
			return 0;

		LF_GameModeRoleplay gameMode = LF_GameModeRoleplay.Cast(GetGame().GetGameMode());
		if (!gameMode)
			return 0;

		return gameMode.LF_GetPoliceOnDuty();
	}
}

//------------------------------------------------------------------------------------------------
//! Replicated copy of the player's account state. Written on the server only.
modded class SCR_ChimeraCharacter
{
	[RplProp()]
	protected int m_iLF_AccountState;

	//! A uid or character id: a short string, set once per body (LF_SetCharacterKey).
	[RplProp()]
	protected string m_sLF_CharacterKey;

	//------------------------------------------------------------------------------------------------
	void LF_SetAccountState(int state)
	{
		if (!Replication.IsServer())
			return;

		if (state == m_iLF_AccountState)
			return;

		m_iLF_AccountState = state;
		LF_Perf.Count("Replication.accountState");
		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	int LF_GetAccountState()
	{
		return m_iLF_AccountState;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the record key of the account character this body plays (#331), so a client's
	//! LF_Utils.GetPlayerUID matches the key the server saves that character's records under.
	void LF_SetCharacterKey(string characterKey)
	{
		if (!Replication.IsServer())
			return;

		if (characterKey == m_sLF_CharacterKey)
			return;

		m_sLF_CharacterKey = characterKey;
		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	string LF_GetCharacterKey()
	{
		return m_sLF_CharacterKey;
	}
}
