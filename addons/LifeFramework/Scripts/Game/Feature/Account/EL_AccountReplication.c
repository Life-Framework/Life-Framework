//------------------------------------------------------------------------------------------------
//! Player-facing account fields packed into one int, so a character can carry them as a single
//! simple-type RplProp. Faction, duty, wanted level, and police rank are what client-side action
//! offers and menus need. Nothing else of the account leaves the server.
//!
//! Layout: bit 0 has-account, bit 1 on-duty, bits 2-4 faction, bits 5-7 wanted, bits 8-15 rank,
//! bit 16 on the police roster (whitelisted; the duty desk offers Clock in).
//! Values outside a field's range are clamped, never wrapped into a neighbour field.
//------------------------------------------------------------------------------------------------
class EL_AccountStateBits
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
	static int FromAccount(EL_PlayerAccount account)
	{
		if (!account)
			return 0;

		bool onRoster = EL_WhitelistManager.IsWhitelistedForFaction(account.GetPersistentId(), EL_EFactionType.POLICE);
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
	static EL_Faction GetFaction(int state)
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
		return HasAccount(state) && GetFaction(state) == EL_Faction.POLICE;
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
		return HasAccount(state) && GetFaction(state) == EL_Faction.CIVILIAN;
	}
}

//------------------------------------------------------------------------------------------------
//! Server -> client bridge for the account cache. EL_PlayerAccountManager lives on the server
//! only, so client-side CanBePerformed / CanBeShown / menus read the replicated copy instead:
//!   - the packed state above on each player's character (SCR_ChimeraCharacter RplProp), and
//!   - the on-duty police count on the game mode (EL_GameModeRoleplay RplProp).
//! Both are written on the server only and bumped only when the value changes. They are plain
//! RplProps, so a late joiner receives the current values with the entity snapshot.
//! The server stays authoritative: every PerformAction / RPC re-checks the real account.
//------------------------------------------------------------------------------------------------
class EL_AccountReplication
{
	//! Seconds between the server's self-healing pass over every connected player.
	static const int REFRESH_INTERVAL_MS = 10000;

	//------------------------------------------------------------------------------------------------
	//! Server: an account field changed. Pushes the packed state to that player's character
	//! and refreshes the police count. No connected player for the account is a no-op.
	static void Publish(EL_PlayerAccount account)
	{
		if (!account || !Replication.IsServer() || !GetGame())
			return;

		string uid = account.GetPersistentId();
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (uid.IsEmpty() || !playerManager)
			return;

		array<int> playerIds = {};
		playerManager.GetPlayers(playerIds);
		foreach (int playerId : playerIds)
		{
			if (EL_Utils.GetPlayerUID(playerId) != uid)
				continue;

			SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(playerManager.GetPlayerControlledEntity(playerId));
			if (character)
				character.EL_SetAccountState(EL_AccountStateBits.FromAccount(account));
		}

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

		EL_PlayerAccount account;
		EL_PlayerAccountManager accountManager = EL_PlayerAccountManager.GetInstance();
		string uid = EL_Utils.GetPlayerUID(playerId);
		if (accountManager && !uid.IsEmpty())
			account = accountManager.GetAccount(uid);

		character.EL_SetAccountState(EL_AccountStateBits.FromAccount(account));
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

		array<int> playerIds = {};
		playerManager.GetPlayers(playerIds);
		foreach (int playerId : playerIds)
		{
			PublishForPlayer(playerId, playerManager.GetPlayerControlledEntity(playerId));
		}

		RefreshPoliceOnDuty();
	}

	//------------------------------------------------------------------------------------------------
	static void RefreshPoliceOnDuty()
	{
		if (!Replication.IsServer() || !GetGame())
			return;

		EL_GameModeRoleplay gameMode = EL_GameModeRoleplay.Cast(GetGame().GetGameMode());
		if (gameMode)
			gameMode.EL_SetPoliceOnDuty(CountPoliceOnDuty());
	}

	//------------------------------------------------------------------------------------------------
	//! Server-authoritative count of connected on-duty police.
	static int CountPoliceOnDuty()
	{
		if (!GetGame())
			return 0;

		PlayerManager playerManager = GetGame().GetPlayerManager();
		EL_PlayerAccountManager accountManager = EL_PlayerAccountManager.GetInstance();
		if (!playerManager || !accountManager)
			return 0;

		int count = 0;
		array<int> playerIds = {};
		playerManager.GetPlayers(playerIds);
		foreach (int playerId : playerIds)
		{
			EL_PlayerAccount account = accountManager.GetAccount(EL_Utils.GetPlayerUID(playerId));
			if (EL_AccountStateBits.IsOnDutyPolice(EL_AccountStateBits.FromAccount(account)))
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
			EL_PlayerAccountManager accountManager = EL_PlayerAccountManager.GetInstance();
			string uid = EL_Utils.GetPlayerUid(character);
			if (!accountManager || uid.IsEmpty())
				return 0;

			return EL_AccountStateBits.FromAccount(accountManager.GetAccount(uid));
		}

		SCR_ChimeraCharacter chimera = SCR_ChimeraCharacter.Cast(character);
		if (!chimera)
			return 0;

		return chimera.EL_GetAccountState();
	}

	//------------------------------------------------------------------------------------------------
	//! Offer-side police count. Server counts live; a client reads the game mode's replicated count.
	static int GetPoliceOnDutyCount()
	{
		if (Replication.IsServer())
			return CountPoliceOnDuty();

		if (!GetGame())
			return 0;

		EL_GameModeRoleplay gameMode = EL_GameModeRoleplay.Cast(GetGame().GetGameMode());
		if (!gameMode)
			return 0;

		return gameMode.EL_GetPoliceOnDuty();
	}
}

//------------------------------------------------------------------------------------------------
//! Replicated copy of the player's account state. Written on the server only.
modded class SCR_ChimeraCharacter
{
	[RplProp()]
	protected int m_iEL_AccountState;

	//------------------------------------------------------------------------------------------------
	void EL_SetAccountState(int state)
	{
		if (!Replication.IsServer())
			return;

		if (state == m_iEL_AccountState)
			return;

		m_iEL_AccountState = state;
		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	int EL_GetAccountState()
	{
		return m_iEL_AccountState;
	}
}
