// red-proof: 2026-11-17, cli test --no-build --red LF_Test_DispatchMapMarks: RED-OK (RedProof() skips the take, so
//   "the take finds the call's mark", the taken colour and the officer-only send fail). Green without it.
//! Records what LF_MapShares sends: "add:<player>:<key>", "remove:<player>:<key>", and each add's
//! colour as "<player>:<key>:<colour>".
class LF_DispatchMarkTestSink : LF_MapShareSink
{
	ref array<string> m_aSent = {};
	ref array<string> m_aColours = {};

	override void Add(int playerId, LF_MapShareRow row)
	{
		m_aSent.Insert(string.Format("add:%1:%2", playerId, row.m_sKey));
		m_aColours.Insert(string.Format("%1:%2:%3", playerId, row.m_sKey, row.m_iColour));
	}

	override void Remove(int playerId, string key)
	{
		m_aSent.Insert(string.Format("remove:%1:%2", playerId, key));
	}

	bool Got(string line)
	{
		return m_aSent.Find(line) != -1;
	}

	bool GotColour(int playerId, string key, int colour)
	{
		return m_aColours.Find(string.Format("%1:%2:%3", playerId, key, colour)) != -1;
	}
}

//------------------------------------------------------------------------------------------------
// tier: LOGIC
//! comms/dispatch-map-marks (card M10, #2238, D-M10): a raised police call becomes a ROLE police shared
//! mark for the on-duty officer only; taking it turns it solid (the taken colour); the world reset
//! closes it and the board expiry removes another. A medic call reaches the medic only; a taxi call
//! and a call with no position get no mark. Four fixture players and a recording sink, no network.
class LF_Test_DispatchMapMarks : LF_Test
{
	protected static const int OFFICER = 1;
	protected static const int MEDIC = 2;
	protected static const int CIVILIAN = 3;
	protected static const int OFF_DUTY = 4;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "comms/dispatch-map-marks";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_MapShares.ResetStatics();
		LF_ServiceCallMapMarks.ResetStatics();
		RunMarks(ctx);
		LF_MapShares.ResetStatics();
		LF_ServiceCallMapMarks.ResetStatics();
		LF_MapShares.EnsureStarted();
		LF_ServiceCallMapMarks.Install();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunMarks(LF_TestContext ctx)
	{
		LF_DispatchMarkTestSink sink = new LF_DispatchMarkTestSink();
		LF_Test_MapShareFacts facts = new LF_Test_MapShareFacts();
		facts.Set(OFFICER, "char-officer", LF_MapShares.ROLE_POLICE, "");
		facts.Set(MEDIC, "char-medic", LF_MapShares.ROLE_MEDIC, "");
		facts.Set(CIVILIAN, "char-civilian", "", "");
		facts.Set(OFF_DUTY, "char-offduty", "", "");
		LF_MapShares.SetSink(sink);
		LF_MapShares.SetFacts(facts);

		// Raise: the police audience row appears for the on-duty officer only.
		LF_OnServiceCall police = LF_OnServiceCall.Create("caller-1", "police", "Test Square", "shots fired", "board", Vector(100.4, 0, 200.6), null);
		string key = LF_ServiceCallMapMarks.OnCall(police);
		ctx.EqualStr("dispatch:1", key, "a raised police call becomes dispatch mark 1");
		ctx.True(LF_MapShares.Has(key), "the share table holds the mark");
		ctx.True(sink.GotColour(OFFICER, key, LF_ServiceCallMapMarks.COLOUR_POLICE_OPEN), "the on-duty officer gets it in the open colour");
		ctx.False(sink.Got("add:2:" + key), "the medic never gets a police mark (D-M10)");
		ctx.False(sink.Got("add:3:" + key) || sink.Got("add:4:" + key), "a civilian and an off-duty officer get nothing");

		// Calls that get no mark.
		ctx.EqualStr("", LF_ServiceCallMapMarks.OnCall(LF_OnServiceCall.Create("caller-2", "taxi", "Test Square", "fare", "board", Vector(1, 0, 1), null)), "a taxi call gets no dispatch mark");
		ctx.EqualStr("", LF_ServiceCallMapMarks.OnCall(LF_OnServiceCall.Create("caller-3", "police", "Nowhere", "x", "board", vector.Zero, null)), "a call with no position gets no mark");
		ctx.Equal(1, LF_ServiceCallMapMarks.Count(), "only the police call is marked");

		// Take: the mark turns solid.
		sink.m_aSent.Clear();
		LF_OnServiceCallTaken taken = LF_OnServiceCallTaken.Create("caller-1", "police", "char-officer", "Test Square", "shots fired", 1);
		string takenKey;
		if (!RedProof())
			takenKey = LF_ServiceCallMapMarks.OnTaken(taken);

		ctx.EqualStr(key, takenKey, "the take finds the call's mark");
		ctx.True(sink.GotColour(OFFICER, key, LF_ServiceCallMapMarks.COLOUR_POLICE_TAKEN), "the officer's mark turns the taken colour");
		ctx.Equal(1, sink.m_aSent.Count(), "the taken mark goes to the officer only");
		ctx.EqualStr("", LF_ServiceCallMapMarks.OnTaken(LF_OnServiceCallTaken.Create("caller-9", "police", "char-officer", "Elsewhere", "y", 1)), "a take of an unmarked call changes nothing");

		// A second identical call stays open while a second officer joins the first one.
		string twin = LF_ServiceCallMapMarks.OnCall(police);
		sink.m_aSent.Clear();
		ctx.EqualStr(key, LF_ServiceCallMapMarks.OnTaken(LF_OnServiceCallTaken.Create("caller-1", "police", "char-other", "Test Square", "shots fired", 2)), "a second taker joins the taken call, not its open twin");
		ctx.Equal(0, sink.m_aSent.Count(), "joining a taken call sends nothing");
		ctx.EqualStr(twin, LF_ServiceCallMapMarks.OnTaken(taken), "the next first taker takes the open twin");

		// Close: the world reset clears the calls and the marks go.
		sink.m_aSent.Clear();
		LF_WorldReset_DispatchMarks step = new LF_WorldReset_DispatchMarks();
		ctx.Equal(2, step.Run(), "the world reset closes both marks");
		ctx.False(LF_MapShares.Has(key), "the closed mark leaves the share table");
		ctx.True(sink.Got("remove:1:" + key), "the closed mark leaves the officer's map");
		ctx.Equal(0, LF_ServiceCallMapMarks.Count(), "no dispatch mark is tracked after the reset");

		// A medic call reaches the medic only, and expires with the board.
		sink.m_aSent.Clear();
		string medicKey = LF_ServiceCallMapMarks.OnCall(LF_OnServiceCall.Create("caller-4", "medic", "Test Farm", "hurt", "board", Vector(10, 0, 20), null));
		ctx.True(sink.GotColour(MEDIC, medicKey, LF_ServiceCallMapMarks.COLOUR_MEDIC_OPEN), "the on-duty medic gets the medic call");
		ctx.Equal(1, sink.m_aSent.Count(), "nobody else gets the medic call");
		float after = LF_MapShares.Now() + LF_ServiceCallsConfig.BoardExpirySeconds() + 1;
		ctx.Equal(1, LF_MapShares.Expire(after), "the medic mark expires with the board");
		ctx.True(sink.Got("remove:2:" + medicKey), "the expired mark leaves the medic's map");
		ctx.Equal(0, LF_ServiceCallMapMarks.Count(), "the expired mark is forgotten");
	}
}
