// red-proof: 2026-11-17, cli test --no-build --red LF_Test_JobRunMarks: RED-OK (RedProof() skips the run's stop, the respawn
//   resend and the abandon, so the row, the waypoint, the resend and the clearing fail). Green without it.

//------------------------------------------------------------------------------------------------
// tier: LOGIC
//! jobs/job-run-marks (#2239): a delivery run's stop goes on the runner's map and waypoint (the nearest
//! drop-off, none for a taxi run or when no drop is placed), a next stop replaces it, a respawn sends
//! the waypoint again, and abandoning or completing the run clears the row and the waypoint. Real
//! LF_CivilianJob runs, a recording share sink and a recording waypoint sink, no network.
class LF_Test_JobRunMarks : LF_Test
{
	protected static const int RUNNER = 1;
	protected static const string UID = "run-marks-courier";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "jobs/job-run-marks";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_MapShares.ResetStatics();
		LF_JobMapMarks.ResetStatics();
		RunMarks(ctx);
		LF_CivilianJob.AbandonRun(UID);
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
		facts.Set(RUNNER, UID, "", "");
		LF_MapShares.SetSink(sink);
		LF_MapShares.SetFacts(facts);
		LF_JobMapMarks.SetSink(waypoints);
		LF_JobMapMarks.Install();

		array<string> ids = {"dropFar", "dropNear"};
		array<vector> positions = {Vector(500, 0, 500), Vector(10.4, 0, 20.6)};
		array<string> none = {};
		array<vector> nowhere = {};
		ctx.Equal(1, LF_JobMapMarks.NearestIndex(positions, vector.Zero), "the nearer drop is picked");
		ctx.Equal(-1, LF_JobMapMarks.NearestIndex(nowhere, vector.Zero), "no drop gives no pick");

		ctx.True(LF_CivilianJob.SignUp(UID, LF_EJobType.PARCEL), "the courier signs up");
		ctx.True(LF_CivilianJob.StartRun(UID, LF_EJobType.PARCEL), "the run starts");

		// No drop placed: no mark, and the run carries on without one.
		ctx.EqualStr("", LF_JobMapMarks.StopForRun(UID, LF_EJobType.PARCEL, vector.Zero, none, nowhere), "no drop placed gives no stop mark");
		ctx.Equal(0, LF_JobMapMarks.Count(), "nothing is tracked without a drop");

		// A run starts: its stop goes to the runner only.
		string key;
		if (!RedProof())
			key = LF_JobMapMarks.StopForRun(UID, LF_EJobType.PARCEL, vector.Zero, ids, positions);

		ctx.EqualStr("job:" + UID + ":dropNear", key, "the run's stop is its nearest drop");
		ctx.True(sink.Got("add:1:" + key), "the runner gets the stop row");
		ctx.True(waypoints.Got("mark:" + UID + ":job-stop@10,21"), "the runner gets the stop waypoint");
		ctx.Equal(1, LF_JobMapMarks.Count(), "the stop is tracked");

		// The next stop replaces the last: one stop per runner.
		string next = LF_JobMapMarks.OnStop(UID, "dropFar", positions[0]);
		ctx.False(LF_MapShares.Has(key), "the next stop takes the last row away");
		ctx.True(LF_MapShares.Has(next), "the next stop has its row");
		ctx.Equal(1, LF_JobMapMarks.Count(), "still one stop");
		LF_JobMapMarks.StopForRun(UID, LF_EJobType.PARCEL, vector.Zero, ids, positions);

		// A respawn: the new client gets the waypoint again, and only its own player's.
		waypoints.m_aSent.Clear();
		int resent;
		if (!RedProof())
			resent = LF_JobMapMarks.ResendTo(UID);

		ctx.Equal(1, resent, "a respawn resends the stop waypoint");
		ctx.True(waypoints.Got("mark:" + UID + ":job-stop@10,21"), "the resent waypoint is the current stop");
		ctx.Equal(0, LF_JobMapMarks.ResendTo("somebody-else"), "another player's respawn resends nothing of this run");
		ctx.Equal(0, LF_JobMapMarks.ResendTo(""), "an unknown player resends nothing");

		// A taxi run gets no stop mark (no fare system yet).
		ctx.EqualStr("", LF_JobMapMarks.StopForRun("run-marks-taxi", LF_EJobType.TAXI, vector.Zero, ids, positions), "a taxi run gets no stop mark");
		ctx.Equal(1, LF_JobMapMarks.Count(), "the taxi run added nothing");

		// Abandoned: the row and the waypoint go at once.
		sink.m_aSent.Clear();
		waypoints.m_aSent.Clear();
		if (!RedProof())
			ctx.True(LF_CivilianJob.AbandonRun(UID), "the run is abandoned");

		ctx.Equal(0, LF_JobMapMarks.Count(), "abandoning the run clears its stop");
		ctx.True(sink.Got("remove:1:" + key), "the stop row leaves the runner's map");
		ctx.True(waypoints.Got("clear:" + UID + ":job-stop"), "the stop waypoint clears");

		// Completed: the paid run clears its stop too.
		ctx.True(LF_CivilianJob.StartRun(UID, LF_EJobType.PARCEL), "a second run starts");
		key = LF_JobMapMarks.StopForRun(UID, LF_EJobType.PARCEL, vector.Zero, ids, positions);
		ctx.True(LF_MapShares.Has(key), "the second run has its stop");
		waypoints.m_aSent.Clear();
		int paid;
		if (!RedProof())
			paid = LF_CivilianJob.CompleteRun(UID);

		ctx.Equal(LF_CivilianJob.PAY_PARCEL, paid, "the run pays");
		ctx.False(LF_MapShares.Has(key), "completing the run clears the stop row");
		ctx.True(waypoints.Got("clear:" + UID + ":job-stop"), "completing the run clears the waypoint");
		ctx.Equal(0, LF_JobMapMarks.Count(), "nothing is left");
	}
}
