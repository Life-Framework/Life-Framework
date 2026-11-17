// red-proof: 2026-11-17, cli test --no-build --red LF_Test_JobMapMarks: RED-OK (RedProof() skips the fare accept and the
//   completion, so the driver and passenger rows, the waypoints and the clearing fail). Green without it.
//! Records the waypoints LF_JobMapMarks sends: "mark:<uid>:<id>@<x>,<z>" and "clear:<uid>:<id>".
class LF_JobMarkTestWaypoints : LF_JobWaypointSink
{
	ref array<string> m_aSent = {};

	override void Mark(string uid, string id, string label, int x, int z)
	{
		m_aSent.Insert(string.Format("mark:%1:%2@%3,%4", uid, id, x, z));
	}

	override void Clear(string uid, string id)
	{
		m_aSent.Insert(string.Format("clear:%1:%2", uid, id));
	}

	bool Got(string line)
	{
		return m_aSent.Find(line) != -1;
	}
}

//------------------------------------------------------------------------------------------------
//! Records what LF_MapShares sends: "add:<player>:<key>", "remove:<player>:<key>".
class LF_JobMarkTestSink : LF_MapShareSink
{
	ref array<string> m_aSent = {};

	override void Add(int playerId, LF_MapShareRow row)
	{
		m_aSent.Insert(string.Format("add:%1:%2", playerId, row.m_sKey));
	}

	override void Remove(int playerId, string key)
	{
		m_aSent.Insert(string.Format("remove:%1:%2", playerId, key));
	}

	bool Got(string line)
	{
		return m_aSent.Find(line) != -1;
	}
}

//------------------------------------------------------------------------------------------------
// tier: LOGIC
//! jobs/job-map-marks (card M11, #2239): an accepted taxi fare puts its destination on the driver's and
//! the passenger's map and waypoint (nobody else's); a delivery stop does the same for its runner and a
//! new stop replaces the last; finishing the fare or the run clears the row and the waypoint. Three
//! fixture players, a recording share sink and a recording waypoint sink, no network.
class LF_Test_JobMapMarks : LF_Test
{
	protected static const int DRIVER = 1;
	protected static const int PASSENGER = 2;
	protected static const int BYSTANDER = 3;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "jobs/job-map-marks";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_MapShares.ResetStatics();
		LF_JobMapMarks.ResetStatics();
		RunMarks(ctx);
		LF_MapShares.ResetStatics();
		LF_JobMapMarks.ResetStatics();
		LF_MapShares.EnsureStarted();
		LF_JobMapMarks.Install();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunMarks(LF_TestContext ctx)
	{
		LF_JobMarkTestSink sink = new LF_JobMarkTestSink();
		LF_JobMarkTestWaypoints waypoints = new LF_JobMarkTestWaypoints();
		LF_Test_MapShareFacts facts = new LF_Test_MapShareFacts();
		facts.Set(DRIVER, "char-driver", "", "");
		facts.Set(PASSENGER, "char-passenger", "", "");
		facts.Set(BYSTANDER, "char-bystander", "", "");
		LF_MapShares.SetSink(sink);
		LF_MapShares.SetFacts(facts);
		LF_JobMapMarks.SetSink(waypoints);

		// A fare accepted: driver and passenger get the row and the waypoint, nobody else.
		int sent;
		if (!RedProof())
			sent = LF_JobMapMarks.OnFareAccepted("7", "char-driver", "char-passenger", Vector(100.4, 0, 200.6));

		ctx.Equal(2, sent, "the fare's destination goes to two players");
		ctx.True(sink.Got("add:1:taxi:7"), "the driver gets the fare row");
		ctx.True(sink.Got("add:2:taxi:7:p"), "the passenger gets the fare row");
		ctx.False(sink.Got("add:3:taxi:7") || sink.Got("add:3:taxi:7:p") || sink.Got("add:2:taxi:7") || sink.Got("add:1:taxi:7:p"), "each row goes to its own player only");
		ctx.True(waypoints.Got("mark:char-driver:taxi-fare@100,201"), "the driver gets the waypoint");
		ctx.True(waypoints.Got("mark:char-passenger:taxi-fare@100,201"), "the passenger gets the waypoint");
		ctx.Equal(2, LF_JobMapMarks.Count(), "two marks are tracked");

		// Calls that get no mark.
		ctx.Equal(0, LF_JobMapMarks.OnFareAccepted("8", "char-driver", "", vector.Zero), "a fare with no destination gets no mark");
		ctx.Equal(0, LF_JobMapMarks.OnFareAccepted("", "char-driver", "", Vector(1, 0, 1)), "a fare with no id gets no mark");
		ctx.EqualStr("", LF_JobMapMarks.OnStop("char-driver", "depot", vector.Zero), "a stop with no position gets no mark");
		ctx.Equal(2, LF_JobMapMarks.Count(), "refused fares and stops add nothing");

		// A delivery stop: a row and a waypoint for the runner; a new stop replaces it.
		string stopKey = LF_JobMapMarks.OnStop("char-bystander", "dropA", Vector(10, 0, 20));
		ctx.EqualStr("job:char-bystander:dropA", stopKey, "a stop is a job mark keyed by runner and stop");
		ctx.True(sink.Got("add:3:" + stopKey), "the runner gets the stop row");
		ctx.True(waypoints.Got("mark:char-bystander:job-stop@10,20"), "the runner gets the stop waypoint");
		ctx.Equal(3, LF_JobMapMarks.Count(), "the stop is tracked beside the fare");
		string nextKey = LF_JobMapMarks.OnStop("char-bystander", "dropB", Vector(30, 0, 40));
		ctx.False(LF_MapShares.Has(stopKey), "the next stop takes the last one's row away");
		ctx.True(LF_MapShares.Has(nextKey), "the next stop has its row");
		ctx.Equal(3, LF_JobMapMarks.Count(), "one stop per runner");

		// Done: the fare's completion clears both rows and both waypoints; the runner's stop is untouched.
		sink.m_aSent.Clear();
		waypoints.m_aSent.Clear();
		int closed;
		if (!RedProof())
			closed = LF_JobMapMarks.OnJobCompleted(LF_OnJobCompleted.Create("char-driver", LF_EJobType.TAXI, 80));

		ctx.Equal(2, closed, "completing the fare closes the driver's and the passenger's marks");
		ctx.True(sink.Got("remove:1:taxi:7") && sink.Got("remove:2:taxi:7:p"), "both fare rows leave their maps");
		ctx.True(waypoints.Got("clear:char-driver:taxi-fare") && waypoints.Got("clear:char-passenger:taxi-fare"), "both fare waypoints clear");
		ctx.False(LF_MapShares.Has("taxi:7") || LF_MapShares.Has("taxi:7:p"), "no fare row stays in the share table");
		ctx.True(LF_MapShares.Has(nextKey), "another player's stop is untouched");

		// Done: the runner's completion clears the stop row and waypoint.
		sink.m_aSent.Clear();
		waypoints.m_aSent.Clear();
		ctx.Equal(1, LF_JobMapMarks.OnJobCompleted(LF_OnJobCompleted.Create("char-bystander", LF_EJobType.PARCEL, 40)), "completing the run closes the stop");
		ctx.True(sink.Got("remove:3:" + nextKey), "the stop row leaves the runner's map");
		ctx.True(waypoints.Got("clear:char-bystander:job-stop"), "the stop waypoint clears");
		ctx.Equal(0, LF_JobMapMarks.Count(), "nothing is tracked after both jobs are done");
		ctx.Equal(0, LF_JobMapMarks.OnJobCompleted(LF_OnJobCompleted.Create("char-bystander", LF_EJobType.PARCEL, 40)), "a second completion closes nothing");

		// A fare ended by id (cancelled) clears too; a run that ended without a run is stale only when bound.
		LF_JobMapMarks.OnFareAccepted("9", "char-driver", "", Vector(5, 0, 5));
		waypoints.m_aSent.Clear();
		ctx.Equal(1, LF_JobMapMarks.OnFareDone("9"), "a cancelled fare closes its mark");
		ctx.True(waypoints.Got("clear:char-driver:taxi-fare"), "the cancelled fare's waypoint clears");
		ctx.True(LF_JobMapMarks.IsStale(true, false), "a run-bound stop with no run is stale (abandoned)");
		ctx.False(LF_JobMapMarks.IsStale(true, true), "a run-bound stop with a run stays");
		ctx.False(LF_JobMapMarks.IsStale(false, false), "a stop never bound to a run stays");

		// Expiry: the safety net removes a mark whose owner never finished.
		LF_JobMapMarks.OnFareAccepted("10", "char-driver", "", Vector(6, 0, 6));
		ctx.Equal(1, LF_MapShares.Expire(LF_MapShares.Now() + LF_JobMapMarks.EXPIRY_SECONDS + 1), "an unfinished fare mark expires");
		ctx.Equal(0, LF_JobMapMarks.Count(), "the expired mark is forgotten");
	}
}
