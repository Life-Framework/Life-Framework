// red-proof: 2026-11-17, cli test --no-build --red LF_Test_PerfMap: RED-OK (RedProof() lays out 20x the POIs and areas and fans out to 20x the recipients, so all three gates fail: map-frame-ms 1.95 over 1, map-rebuild-ms 108.9 over 8, map-share-fanout-ms 4.7 over 2). Green without it on lf-testbox, --tier perf: frame 0.067 ms, rebuild 3.55 ms, fan-out 0.15 ms.
//! Counts what LF_MapShares sends and does nothing else, so the fan-out measure times the share table and not a
//! recording sink's string formatting.
class LF_PerfMapSink : LF_MapShareSink
{
	int m_iAdds;
	int m_iRemoves;
	int m_iClears;

	override void Add(int playerId, LF_MapShareRow row)
	{
		m_iAdds++;
	}

	override void Remove(int playerId, string key)
	{
		m_iRemoves++;
	}

	override void Clear(int playerId)
	{
		m_iClears++;
	}
}

//------------------------------------------------------------------------------------------------
//! Synthetic players: ids 1..N, one in eight on duty as police, one in six in an org.
class LF_PerfMapFacts : LF_MapShareFacts
{
	ref array<int> m_aIds = {};
	ref map<int, ref LF_MapShareMember> m_mMembers = new map<int, ref LF_MapShareMember>();

	void Fill(int count)
	{
		m_aIds.Clear();
		m_mMembers.Clear();
		for (int i = 1; i <= count; i++)
		{
			array<string> roles = {};
			if ((i % 8) == 0)
				roles.Insert(LF_MapShares.ROLE_POLICE);

			string org;
			if ((i % 6) == 0)
				org = "perf-org-" + (i % 4).ToString();

			m_aIds.Insert(i);
			m_mMembers.Set(i, LF_MapShareMember.Make(i, "perf-map-" + i.ToString(), roles, org));
		}
	}

	override void Players(notnull array<int> outIds)
	{
		outIds.Copy(m_aIds);
	}

	override LF_MapShareMember Of(int playerId)
	{
		return m_mMembers.Get(playerId);
	}
}

//------------------------------------------------------------------------------------------------
// tier: PERF
//! perf/map-128 (#2246, everon-map.md card M18): the map cost gate. Runs only under `tools\cli test --tier perf`
//! (`--only` runs it too, with fewer repeats). Three budgets, each on the cheapest of several timed repeats:
//!
//!  - per frame, map open (`FRAME_BUDGET_MS`, 1 ms): what a pan or zoom frame costs in script, 400 POIs drawn at the
//!    solo zoom (400 icons) and 40 areas. POIs are projected to the screen and culled (the widget layer's
//!    `Reposition`); the areas are one canvas that the map moves, so a pan frame re-tessellates nothing.
//!  - per zoom end (`REBUILD_BUDGET_MS`, 8 ms; measured 3.5 ms): the cluster bucketing of the 400 POIs plus the re-tessellation of the 40
//!    areas into draw commands (32-gon outlines, dashes, fills, labels). It runs once when a zoom ends, never per
//!    frame, so it has its own budget and does not weaken the frame gate.
//!  - share fan-out (`FANOUT_BUDGET_MS`, 2 ms): one EVERYONE share put to 128 synthetic players who already hold
//!    24 rows; the 40 puts of a repeat fill the 64-row cap.
//!
//! What it cannot show: the engine's widget calls (`FrameSlot` positions, `SetDrawCommands`) and the owner RPCs of
//! the real sink. The widget route was measured in the engine by the `-lfMapPlacesProbe` pass (everon-map.md,
//! #2232) at 0.23 to 0.30 ms per pan frame and 8.2 to 12.2 ms per rebuild, which is where the headroom in the frame
//! budget goes. Prints one `[LFDebug:Perf] BENCH <name> value=<n> budget=<n>` line per measure.
class LF_Test_PerfMap : LF_Test
{
	//! Sizes from the card: 400 POIs, 40 areas, 128 recipients.
	protected static const int POIS = 400;
	protected static const int AREAS = 40;
	protected static const int RECIPIENTS = 128;
	protected static const int HELD_ROWS = 24;	// rows a recipient holds before the measured puts; with FANOUT_PASSES new keys they reach the 64-row cap

	//! The budgets, milliseconds.
	protected static const float FRAME_BUDGET_MS = 1.0;
	protected static const float REBUILD_BUDGET_MS = 8.0;
	protected static const float FANOUT_BUDGET_MS = 2.0;

	//! Map view the layout runs against: a 1080p screen, solo zoom (zoom 4 is 800 m wide; 1920 px / 800 m = 2.4).
	protected static const float VIEW_W = 1920;
	protected static const float VIEW_H = 1080;
	protected static const float PPU = 2.4;
	protected static const float ZOOM = 5;
	protected static const float CULL_MARGIN = 64;

	protected static const int REPEATS = 5;
	protected static const int PERF_REPEATS = 15;
	protected static const int FRAME_PASSES = 2000;
	protected static const int REBUILD_PASSES = 20;
	protected static const int FANOUT_PASSES = 40;

	protected bool m_bMuted;
	protected int m_iRepeats = REPEATS;

	override string GetName()
	{
		return "perf/map-128";
	}

	//------------------------------------------------------------------------------------------------
	protected void Say(string message)
	{
		LF_Debug.SetAllEnabled(true);
		LF_Debug.Info(LF_Perf.FEATURE, message);
		LF_Debug.SetAllEnabled(!m_bMuted);
	}

	//------------------------------------------------------------------------------------------------
	protected void Mute(bool muted)
	{
		m_bMuted = muted;
		LF_Debug.SetAllEnabled(!muted);
	}

	//------------------------------------------------------------------------------------------------
	//! One measure against its budget: prints the BENCH line and asserts.
	protected void Check(LF_TestContext ctx, string name, float ms, float budgetMs)
	{
		Say(string.Format("BENCH %1 value=%2 budget=%3 unit=ms", name, ms, budgetMs));
		ctx.True(ms <= budgetMs, string.Format("perf %1 %2 ms over budget %3 ms", name, ms, budgetMs));
	}

	//------------------------------------------------------------------------------------------------
	protected void Report(string name, float ms)
	{
		Say(string.Format("BENCH %1 value=%2 budget=none unit=ms", name, ms));
	}

	//------------------------------------------------------------------------------------------------
	//! POIs scattered over the island at fixed offsets, so every run lays out the same points.
	protected void FillPois(int count, notnull array<ref LF_MapPoi> into)
	{
		for (int i = 0; i < count; i++)
		{
			LF_MapPoi poi = new LF_MapPoi();
			poi.m_sId = "perf-poi-" + i.ToString();
			poi.m_sKind = "PerfKind";
			poi.m_sCategory = "civic";
			poi.m_vPos = Vector(Math.Mod(i * 7919.0, 12800), 0, Math.Mod(i * 104729.0, 12800));
			poi.m_eState = LF_EMapPoiState.FULL;
			into.Insert(poi);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! 40 circle areas of the six styles in an 8 by 5 grid, all inside the view, so every one draws (the worst case:
	//! an area off the canvas returns early).
	protected void FillAreas(int count, notnull array<ref LF_MapArea> into)
	{
		float cell = (VIEW_W / PPU) / 8;
		for (int i = 0; i < count; i++)
		{
			LF_MapArea area = new LF_MapArea();
			area.m_sName = "perf-area-" + i.ToString();
			area.m_eStyle = i % 6;
			area.m_sLabel = "Perf area " + i.ToString();
			int gridRow = Math.Floor(i / 8.0);
			gridRow = gridRow % 5;
			int gridCol = i % 8;
			float centreX = cell * (gridCol + 0.5);
			float centreZ = cell * (gridRow + 0.5);
			vector centre = Vector(centreX, 0, centreZ);
			LF_MapAreas.CircleOutline(centre, cell * 0.4, LF_MapAreas.DEFAULT_SEGMENTS, area.m_aOutline);
			into.Insert(area);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The widget layer's per-frame work minus the widget calls: project each group to the screen and cull it.
	protected static int Reposition(notnull array<ref LF_MapClusterGroup> groups, float originX, float originZ)
	{
		int shown;
		foreach (LF_MapClusterGroup group : groups)
		{
			float x = (group.m_fX - originX) * PPU;
			float y = VIEW_H - (group.m_fZ - originZ) * PPU;
			if (x > -CULL_MARGIN && y > -CULL_MARGIN && x < VIEW_W + CULL_MARGIN && y < VIEW_H + CULL_MARGIN)
				shown++;
		}

		return shown;
	}

	//------------------------------------------------------------------------------------------------
	//! The area layer's zoom-end work minus the canvas call: project every vertex, build the draw records.
	protected static int Tessellate(notnull array<ref LF_MapArea> areas, notnull array<ref LF_MapAreaCmd> into)
	{
		into.Clear();
		foreach (LF_MapArea area : areas)
		{
			array<vector> screen = {};
			foreach (vector world : area.m_aOutline)
			{
				screen.Insert(Vector(world[0] * PPU, VIEW_H - world[2] * PPU, 0));
			}

			LF_MapAreasDraw.Build(area, screen, PPU, VIEW_W, VIEW_H, "", area.m_sLabel, into);
		}

		return into.Count();
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		m_iRepeats = REPEATS;
		if (LF_TestManager.PerfRun())
			m_iRepeats = PERF_REPEATS;

		// The red proof plants a cost: every measured unit does this many times its work.
		int load = 1;
		if (RedProof())
		{
			load = 20;
			m_iRepeats = 2;
		}

		RunLayout(ctx, load);
		RunFanout(ctx, load);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunLayout(LF_TestContext ctx, int load)
	{
		array<ref LF_MapPoi> pois = {};
		FillPois(POIS * load, pois);
		array<ref LF_MapArea> areas = {};
		FillAreas(AREAS * load, areas);
		ctx.Equal(POIS * load, pois.Count(), "the POI set is built");
		ctx.Equal(AREAS * load, areas.Count(), "the area set is built");

		array<ref LF_MapClusterGroup> groups = {};
		LF_MapClusterRules.BuildFromPois(pois, ZOOM, groups);
		ctx.Equal(POIS * load, groups.Count(), "at the solo zoom every POI is its own icon (the worst case)");

		array<ref LF_MapAreaCmd> commands = {};
		int drawn = Tessellate(areas, commands);
		ctx.True(drawn >= AREAS * load * 2, "every area draws at least its outline and a fill or label");

		Mute(true);
		int t;
		int shown;
		int rep;
		int pass;
		float best = -1;
		float ms;

		// Per frame: reposition the 400 icons. The origin moves a little each pass, as a pan does.
		// GetTickCount(t) is whole milliseconds: take the batch total and divide in float.
		best = -1;
		for (rep = 0; rep < m_iRepeats; rep++)
		{
			t = System.GetTickCount();
			for (pass = 0; pass < FRAME_PASSES; pass++)
			{
				shown += Reposition(groups, pass * 0.5, pass * 0.25);
			}

			int frameTicks = System.GetTickCount(t);
			ms = (frameTicks * 1.0) / FRAME_PASSES;
			if (best < 0 || ms < best)
				best = ms;
		}

		Mute(false);
		ctx.True(shown > 0, "some icons are on screen");
		Check(ctx, "map-frame-ms", best, FRAME_BUDGET_MS);
		Mute(true);

		// Per zoom end: cluster the POIs again and re-tessellate the areas.
		float bestCluster = -1;
		float bestAreas = -1;
		float bestTotal = -1;
		for (rep = 0; rep < m_iRepeats; rep++)
		{
			t = System.GetTickCount();
			for (pass = 0; pass < REBUILD_PASSES; pass++)
			{
				groups.Clear();
				LF_MapClusterRules.BuildFromPois(pois, ZOOM, groups);
			}

			float clusterMs = (System.GetTickCount(t) * 1.0) / REBUILD_PASSES;
			t = System.GetTickCount();
			for (pass = 0; pass < REBUILD_PASSES; pass++)
			{
				Tessellate(areas, commands);
			}

			float areaMs = (System.GetTickCount(t) * 1.0) / REBUILD_PASSES;
			if (bestCluster < 0 || clusterMs < bestCluster)
				bestCluster = clusterMs;

			if (bestAreas < 0 || areaMs < bestAreas)
				bestAreas = areaMs;

			if (bestTotal < 0 || clusterMs + areaMs < bestTotal)
				bestTotal = clusterMs + areaMs;
		}

		Mute(false);
		Report("map-rebuild-clusters-ms", bestCluster);
		Report("map-rebuild-areas-ms", bestAreas);
		Check(ctx, "map-rebuild-ms", bestTotal, REBUILD_BUDGET_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunFanout(LF_TestContext ctx, int load)
	{
		LF_MapShares.ResetStatics();
		LF_PerfMapSink sink = new LF_PerfMapSink();
		LF_PerfMapFacts facts = new LF_PerfMapFacts();
		facts.Fill(RECIPIENTS * load);
		LF_MapShares.SetSink(sink);
		LF_MapShares.SetFacts(facts);

		Mute(true);
		int row;
		for (row = 0; row < HELD_ROWS; row++)
		{
			LF_MapShares.Put(LF_MapShareRow.Make("perf-held-" + row.ToString(), LF_EMapShareAudience.EVERYONE, "", row, row, 0, 0, "held", 0));
		}

		Mute(false);
		ctx.Equal(HELD_ROWS, LF_MapShares.Count(), "the held rows are in the table");
		ctx.Equal(HELD_ROWS, LF_MapShares.DeliveredCount(1), "a recipient holds them all");
		ctx.Equal(HELD_ROWS * RECIPIENTS * load, sink.m_iAdds, "every held row reached every recipient");

		array<ref LF_MapShareRow> measured = {};
		for (int made = 0; made < FANOUT_PASSES; made++)
		{
			measured.Insert(LF_MapShareRow.Make("perf-put-" + made.ToString(), LF_EMapShareAudience.EVERYONE, "", 100, 200, 0, 0, "measured", 0));
		}

		Mute(true);
		float best = -1;
		for (int rep = 0; rep < m_iRepeats; rep++)
		{
			int adds = sink.m_iAdds;
			int t = System.GetTickCount();
			for (int pass = 0; pass < FANOUT_PASSES; pass++)
			{
				LF_MapShares.Put(measured[pass]);
			}

			float ms = (System.GetTickCount(t) * 1.0) / FANOUT_PASSES;
			if (best < 0 || ms < best)
				best = ms;

			ctx.Equal(adds + FANOUT_PASSES * RECIPIENTS * load, sink.m_iAdds, "each put reached every recipient");
			for (int gone = 0; gone < FANOUT_PASSES; gone++)
			{
				LF_MapShares.Remove("perf-put-" + gone.ToString());
			}
		}

		// The sweep's steady state: nothing changed, every recipient is checked against every row.
		float bestSweep = -1;
		for (int sweep = 0; sweep < m_iRepeats; sweep++)
		{
			int sweepStart = System.GetTickCount();
			LF_MapShares.RefreshAll();
			float sweepMs = System.GetTickCount(sweepStart);
			if (bestSweep < 0 || sweepMs < bestSweep)
				bestSweep = sweepMs;
		}

		Mute(false);
		Check(ctx, "map-share-fanout-ms", best, FANOUT_BUDGET_MS);
		Report("map-share-sweep-ms", bestSweep);

		LF_MapShares.ResetStatics();
		LF_MapShares.EnsureStarted();
	}
}
