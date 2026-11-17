// red-proof: 2026-11-17 `tools/cli test --no-build --red LF_Test_RaceRacerIndex,...`: RED-OK racing/racer-index, with the synthetic population not installed "each racer costs one uid compare a tick (expected 160, got 0)" and "a racer gone offline is scanned for once (expected 17, got 16)" failed. Green with the fix (--only, 8 passed).
// tier: WORLD
//! #2404: the race tick finds each racer's body through the racer's cached player id, checked with
//! one uid compare, not by scanning every player's uid per racer every 250 ms. A synthetic
//! population of 128 players stands in for the PlayerManager (LF_RaceRunner.SetPlayers). A racer
//! whose player switched character is scanned for once, then not again before RESCAN_MS.
class LF_Test_RaceRacerIndex : LF_Test
{
	static const int PLAYERS = 128;
	static const int RACERS = 16;
	static const int TICKS = 10;

	override string GetName()
	{
		return "racing/racer-index";
	}

	//------------------------------------------------------------------------------------------------
	static LF_RaceCourseRecord BuildCourse(string id)
	{
		LF_RaceCourseRecord course = new LF_RaceCourseRecord();
		course.m_sId = id;
		course.m_aCheckpoints = {"cp_index_a", "cp_index_b"};
		course.m_iLaps = 1;
		course.m_iEntryFee = 0;
		course.m_iMinEntrants = 2;
		course.m_iLobbySeconds = 600;
		course.m_aPayoutPercent = {100};
		return course;
	}

	override void Run(LF_TestContext ctx)
	{
		LF_RaceRunner.ResetStatics();
		IEntity body = LF_TestCharacterFixture.Spawn(ctx);
		if (!body)
			return;

		LF_TestRacePlayers players = LF_TestRacePlayers.Create(PLAYERS, "racer-index-", body);
		if (!RedProof())
			LF_RaceRunner.SetPlayers(players);

		// The racers are the last 16 player ids: a scan walks almost the whole server to find each.
		LF_RaceCourseRecord course = BuildCourse("test_racer_index");
		for (int r = 0; r < RACERS; r++)
		{
			string uid = players.UidAt(PLAYERS - r);
			ctx.True(LF_RaceRunner.Join(course, uid, body) == LF_ERaceJoin.JOINED, "racer " + uid + " joins");
		}
		ctx.True(LF_RaceRunner.CloseLobby(course.m_sId), "16 racers start the race");

		// The start notified every racer, which found each one once.
		int scans = LF_RaceRunner.GetScanCount();
		players.m_iUidLookups = 0;
		for (int t = 0; t < TICKS; t++)
			LF_RaceRunner.Tick();

		ctx.Equal(scans, LF_RaceRunner.GetScanCount(), "ticks with every racer online scan no players");
		ctx.Equal(RACERS * TICKS, players.m_iUidLookups, "each racer costs one uid compare a tick");

		// A racer whose player switched character: one scan finds nobody, the next ticks do not scan again.
		players.SetUid(PLAYERS, "racer-index-someone-else");
		players.m_iUidLookups = 0;
		for (int u = 0; u < 3; u++)
			LF_RaceRunner.Tick();

		ctx.Equal(scans + 1, LF_RaceRunner.GetScanCount(), "a racer gone offline is scanned for once, not every tick");
		ctx.True(players.m_iUidLookups < RACERS * 3 + PLAYERS + 3, "the offline racer's scan is paid once");

		LF_RaceRunner.AbandonAll("test racer index");
		LF_RaceRunner.SetPlayers(null);
		LF_RaceRunner.ResetStatics();
		LF_TestCharacterFixture.Delete(body);
	}
}

//------------------------------------------------------------------------------------------------
//! A synthetic PlayerManager for the race runner: players 1..N with uid prefix + id, every one
//! standing in the same fixture body. Counts the uid lookups.
class LF_TestRacePlayers : LF_RacePlayers
{
	ref array<string> m_aUids = {};
	EntityID m_BodyId = EntityID.INVALID;
	int m_iUidLookups;

	//------------------------------------------------------------------------------------------------
	static LF_TestRacePlayers Create(int count, string prefix, IEntity body)
	{
		LF_TestRacePlayers players = new LF_TestRacePlayers();
		for (int i = 1; i <= count; i++)
			players.m_aUids.Insert(prefix + i.ToString());

		if (body)
			players.m_BodyId = body.GetID();

		return players;
	}

	//------------------------------------------------------------------------------------------------
	string UidAt(int playerId)
	{
		if (playerId < 1 || playerId > m_aUids.Count())
			return "";

		return m_aUids[playerId - 1];
	}

	//------------------------------------------------------------------------------------------------
	void SetUid(int playerId, string uid)
	{
		if (playerId >= 1 && playerId <= m_aUids.Count())
			m_aUids[playerId - 1] = uid;
	}

	//------------------------------------------------------------------------------------------------
	override bool Available()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void GetPlayers(notnull array<int> outIds)
	{
		outIds.Clear();
		for (int i = 1; i <= m_aUids.Count(); i++)
			outIds.Insert(i);
	}

	//------------------------------------------------------------------------------------------------
	override string UidOf(int playerId)
	{
		m_iUidLookups++;
		return UidAt(playerId);
	}

	//------------------------------------------------------------------------------------------------
	override IEntity ControlledBy(int playerId)
	{
		if (m_BodyId == EntityID.INVALID || !GetGame().GetWorld())
			return null;

		return GetGame().GetWorld().FindEntityByID(m_BodyId);
	}

	//------------------------------------------------------------------------------------------------
	override int PlayerOf(IEntity body)
	{
		return 0;
	}
}
