// red-proof: 2026-11-17, cli test --no-build --red LF_Test_EventMapShare: RED-OK events/map-share (RedProof() skips LF_EventMapShare.Install, so no row or area is ever put: the start, row, area and expiry checks fail). Green without it.
// tier: WORLD
//! Event places on the map (#2243, card M15). The test event is bound to the target of the scene in
//! grid cell `event-scene` (scene `grid_event_scene`, found through LF_Places by component) with a 10 m
//! radius and driven through the real shell. Starting it puts an `EVERYONE` row `event:test_ems` in
//! LF_MapShares (sent to a civilian, an on-duty officer and an org member alike) and an EVENT-style
//! circle in LF_MapAreas; a phase change renews it with the phase's radius and does not duplicate it; a
//! late joiner's spawn resends it; the event's end removes the row from everyone and the area; a
//! second run's row leaves everyone at its safety expiry. An event with no place, or whose marker is
//! not placed, shows nothing.
class LF_Test_EventMapShare : LF_Test
{
	static const string SCENE = "grid_event_scene";
	static const string EVENT = "test_ems";
	static const string KEY = "event:test_ems";
	protected static const int CIVILIAN = 1;
	protected static const int OFFICER = 2;
	protected static const int MEMBER = 3;
	protected static const int LATE = 4;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "events/map-share";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_MapShares.ResetStatics();
		LF_MapAreas.ResetStatics();
		LF_EventMapShare.ResetStatics();
		RunShare(ctx);
		LF_MapShares.ResetStatics();
		LF_MapAreas.ResetStatics();
		LF_EventMapShare.ResetStatics();
		LF_MapShares.EnsureStarted();
		LF_EventMapShare.Install();
	}

	//------------------------------------------------------------------------------------------------
	protected static float RadiusOfArea(LF_MapArea area, vector centre)
	{
		float dx = area.m_aOutline[0][0] - centre[0];
		float dz = area.m_aOutline[0][2] - centre[2];
		return Math.Sqrt(dx * dx + dz * dz);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunShare(LF_TestContext ctx)
	{
		vector centre;
		ctx.True(LF_Places.FindOrigin(LF_EventWaypointComponent.KIND_TARGET, SCENE, centre), "the grid cell's target is placed");

		LF_Test_MapShareSink sink = new LF_Test_MapShareSink();
		LF_Test_MapShareFacts facts = new LF_Test_MapShareFacts();
		facts.Set(CIVILIAN, "char-civilian", "", "");
		facts.Set(OFFICER, "char-officer", LF_MapShares.ROLE_POLICE, "");
		facts.Set(MEMBER, "char-member", "", "crew");
		LF_MapShares.SetSink(sink);
		LF_MapShares.SetFacts(facts);
		if (!RedProof())
			LF_EventMapShare.Install();

		// Stop the live ticker and swap in the test event; other modules' registrations stay.
		LF_EventShell.Stop();
		LF_EventShell.Unregister("test_ems_here");
		LF_EventShell.Unregister("test_ems_never");
		LF_EventShell.Unregister("test_ems_empty");
		ctx.True(LF_EventShell.RegisterCondition("test_ems_here", LF_EventPrecondition.MakePlayers(LF_EEventConditionKind.PLAYERS_NEARBY, 1)), "the nearby condition registers");
		ctx.True(LF_EventShell.RegisterCondition("test_ems_never", LF_EventPrecondition.MakePlayers(LF_EEventConditionKind.PLAYERS_ONLINE, 9999)), "a condition that never holds registers");
		ctx.True(LF_EventShell.RegisterAbort("test_ems_empty", LF_EventAbort.MakePlayers(LF_EEventAbortKind.NEARBY_BELOW, 1)), "the empty-place abort registers");

		array<ref LF_EventPhase> phases = {};
		array<string> prepModules = {"test_ems_here"};
		array<string> hotModules = {"test_ems_never", "test_ems_empty"};
		phases.Insert(LF_EventPhase.Make("prep", 0, 0, prepModules));
		phases.Insert(LF_EventPhase.Make("hot", 0, 0, hotModules));
		LF_EventsConfig config = new LF_EventsConfig();
		config.m_aEvents = {};
		config.m_aEvents.Insert(LF_EventDef.Make(EVENT, phases, 300));
		LF_EventShell.Use(config);
		LF_EventShell.ForceHour(12);
		LF_EventPlace.Unbind(EVENT);
		ctx.True(LF_EventPlace.Bind(EVENT, SCENE, 10), "the event binds to its scene with a 10 m radius");
		ctx.True(LF_EventPlace.BindPhaseRadius(EVENT, "hot", 20), "the hot phase has a wider ring");

		vector inside = centre + Vector(3, 0, 0);
		vector outside = centre + Vector(30, 0, 0);
		array<vector> positions = {};
		positions.Insert(inside);
		LF_EventPlace.UseTestPositions(positions);

		// Idle: nothing on the map.
		ctx.False(LF_EventMapShare.IsOpen(EVENT), "an event that has not started is not on the map");
		ctx.Equal(0, LF_MapShares.Count(), "no share before the event starts");

		// Start: an EVERYONE row, sent to every player whatever they belong to, and the area.
		LF_EventShell.Tick(10);
		ctx.EqualStr("prep", LF_EventShell.PhaseOf(EVENT), "with a player near the event starts");
		ctx.True(LF_EventMapShare.IsOpen(EVENT), "the running event is on the map");
		ctx.True(LF_MapShares.Has(KEY), "its row is in the share table");
		ctx.Equal(1, LF_MapShares.Count(), "one row for the event");
		ctx.True(sink.Got("add:1:" + KEY), "a civilian gets the event row");
		ctx.True(sink.Got("add:2:" + KEY), "an officer on duty gets it too");
		ctx.True(sink.Got("add:3:" + KEY), "an org member gets it too");
		LF_MapArea area = LF_MapAreas.Find(KEY);
		ctx.True(area != null, "its place is a map area");
		if (area)
		{
			ctx.True(area.m_eStyle == LF_MapAreaStyle.EVENT, "the area has the event style");
			ctx.Equal(LF_MapAreas.DEFAULT_SEGMENTS, area.m_aOutline.Count(), "a circle of 32 sides");
			ctx.True(Math.AbsFloat(RadiusOfArea(area, centre) - 10) < 0.01, "the area has the prep radius");
		}

		// Advance: the same row (not a second one), the area follows the hot radius.
		LF_EventShell.Tick(11);
		ctx.EqualStr("hot", LF_EventShell.PhaseOf(EVENT), "prep advances to hot");
		ctx.Equal(1, LF_MapShares.Count(), "a phase change replaces the row, it does not add another");
		area = LF_MapAreas.Find(KEY);
		ctx.True(area != null, "the area stays through the phase change");
		if (area)
			ctx.True(Math.AbsFloat(RadiusOfArea(area, centre) - 20) < 0.01, "the area takes the hot phase's radius");

		// A late joiner spawns while the event runs: their client is cleared and sent the row.
		facts.Set(LATE, "char-late", "", "");
		LF_MapShares.OnSpawned(LATE);
		ctx.True(sink.Got("add:4:" + KEY), "a late joiner is sent the running event's row");

		// End: the row leaves everyone, the area goes.
		positions.Clear();
		positions.Insert(outside);
		LF_EventPlace.UseTestPositions(positions);
		LF_EventShell.Tick(13);
		ctx.EqualStr("", LF_EventShell.PhaseOf(EVENT), "an empty place ends the event");
		ctx.False(LF_EventMapShare.IsOpen(EVENT), "the ended event is off the map");
		ctx.False(LF_MapShares.Has(KEY), "its row leaves the share table");
		ctx.Equal(0, LF_MapShares.Count(), "no share is left");
		ctx.True(sink.Got("remove:1:" + KEY), "the civilian loses it");
		ctx.True(sink.Got("remove:2:" + KEY), "the officer loses it");
		ctx.True(sink.Got("remove:3:" + KEY), "the org member loses it");
		ctx.True(sink.Got("remove:4:" + KEY), "the late joiner loses it");
		ctx.True(LF_MapAreas.Find(KEY) == null, "its area is gone");

		// The timer: a second run's row stays until its safety expiry, then leaves everyone.
		positions.Clear();
		positions.Insert(inside);
		LF_EventPlace.UseTestPositions(positions);
		LF_EventShell.Tick(400);
		ctx.True(LF_MapShares.Has(KEY), "a second run puts the row again after the cooldown");
		ctx.Equal(0, LF_MapShares.Expire(LF_MapShares.Now() + LF_EventMapShare.EXPIRY_SECONDS - 5), "the row stays before its expiry");
		ctx.True(LF_MapShares.Has(KEY), "still on the map");
		ctx.Equal(1, LF_MapShares.Expire(LF_MapShares.Now() + LF_EventMapShare.EXPIRY_SECONDS + 5), "the row leaves at its expiry");
		ctx.False(LF_MapShares.Has(KEY), "an expired row is off the map");

		// No place, or a marker that is not placed: nothing is shown.
		ctx.False(LF_EventMapShare.Open("test_ems_noplace", "prep"), "an event with no place shows nothing");
		LF_EventPlace.Bind("test_ems_nomarker", "no_such_scene", 10);
		ctx.False(LF_EventMapShare.Open("test_ems_nomarker", "prep"), "a scene with no placed target shows nothing");
		ctx.False(LF_EventMapShare.IsOpen("test_ems_noplace"), "and is not on the map");
		ctx.Equal(0, LF_MapShares.Count(), "no share for either");
		ctx.False(LF_EventMapShare.Close("test_ems_noplace"), "closing an event that was never shown is a no-op");

		LF_EventPlace.Unbind(EVENT);
		LF_EventPlace.Unbind("test_ems_nomarker");
		LF_EventPlace.ClearTestPositions();
		LF_EventShell.Unregister("test_ems_here");
		LF_EventShell.Unregister("test_ems_never");
		LF_EventShell.Unregister("test_ems_empty");
		LF_EventShell.ForceHour(-1);
		LF_EventShell.Use(LF_EventShell.LoadConfig());
		LF_EventShell.StartIfServer();
	}
}
