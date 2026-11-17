// red-proof: observed 2026-11-17 with LF_PromoteCandidates.List reading only BindOfficer bodies, as before the fix (log test-20261117-224005): FAIL "the patrol officer standing by is found among the connected players (expected promote-players-patrol, got empty)". Restored: green.
// tier: LOGIC
//! police/promote-finds-players (#2416): the station promote action looks for the lower-rank officer
//! among the connected players, not only bodies a test bound with BindOfficer. Synthetic players
//! stand in for the player manager.
class LF_Test_PromoteFindsPlayers : LF_Test
{
	protected static const string PROMOTER = "promote-players-senior";
	protected static const string TARGET = "promote-players-patrol";
	protected static const string FAR = "promote-players-far";
	protected static const string CIVILIAN = "promote-players-civilian";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "police/promote-finds-players";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		int top = LF_PoliceRanks.TopRank();
		ctx.True(top > LF_PoliceRanks.PATROL, "the ladder has a rank above patrol");

		LF_PlayerAccount promoter = Police(PROMOTER, top);
		LF_PlayerAccount target = Police(TARGET, LF_PoliceRanks.PATROL);
		Police(FAR, LF_PoliceRanks.PATROL);
		LF_PlayerAccount civilian = LF_PlayerAccountManager.GetOrCreate(CIVILIAN);
		civilian.SetFaction(LF_Faction.CIVILIAN);

		vector station = "500 0 500";
		LF_Test_PromotePlayers players = new LF_Test_PromotePlayers();
		players.Add(PROMOTER, station);
		players.Add(CIVILIAN, "501 0 500");
		players.Add(FAR, "540 0 500");
		players.Add(TARGET, "502 0 501");

		string found = LF_PoliceRanks.FindLowerRankNear(PROMOTER, station, LF_PoliceRanks.PROMOTE_RADIUS, players);
		ctx.EqualStr(TARGET, found, "the patrol officer standing by is found among the connected players");
		ctx.EqualStr("", LF_PoliceRanks.FindLowerRankNear(PROMOTER, "900 0 900", LF_PoliceRanks.PROMOTE_RADIUS, players), "nobody is found with no officer in reach");
		ctx.EqualStr("", LF_PoliceRanks.FindLowerRankNear(TARGET, "502 0 501", LF_PoliceRanks.PROMOTE_RADIUS, players), "a patrol officer finds no one below them");

		ctx.True(LF_PoliceRanks.Promote(PROMOTER, found), "the senior promotes the officer found");
		ctx.Equal(LF_PoliceRanks.PATROL + 1, target.GetPoliceRank(), "the patrol officer moves up one rank");

		target.SetPoliceRank(LF_PoliceRanks.PATROL);
		promoter.SetOnDuty(false);
		target.SetOnDuty(false);
	}

	//------------------------------------------------------------------------------------------------
	protected LF_PlayerAccount Police(string uid, int rank)
	{
		LF_PlayerAccount account = LF_PlayerAccountManager.GetOrCreate(uid);
		account.SetFaction(LF_Faction.POLICE);
		account.SetPoliceRank(rank);
		account.SetOnDuty(true);
		return account;
	}
}

//------------------------------------------------------------------------------------------------
//! Synthetic connected players for LF_Test_PromoteFindsPlayers.
class LF_Test_PromotePlayers : LF_PromoteCandidates
{
	protected ref array<string> m_aUids = {};
	protected ref array<vector> m_aOrigins = {};

	//------------------------------------------------------------------------------------------------
	void Add(string uid, vector origin)
	{
		m_aUids.Insert(uid);
		m_aOrigins.Insert(origin);
	}

	//------------------------------------------------------------------------------------------------
	override protected void ListPlayers(notnull array<string> uids, notnull array<vector> origins)
	{
		uids.InsertAll(m_aUids);
		origins.InsertAll(m_aOrigins);
	}
}
