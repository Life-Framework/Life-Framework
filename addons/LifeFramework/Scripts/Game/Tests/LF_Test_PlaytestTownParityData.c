// red-proof: RedProof() resolves no place (every lookup answers "not placed"), so each row below fails and the run names them all.
//! playtest-town/parity-data (TP-6b, #4246): under the playtest town's header the map and the welcome guide
//! point only at places the town has. Every map POI kind, every stop of every run and every guide step target in
//! the loaded config resolves to a placed place component, found through `LF_Places` (and the guide's own target
//! lookup), never by a coordinate. A row the town cannot place is a gap: add the place, or leave the row out of
//! the town's config.
//! Run it on the playtest town's mission header: `tools\cli test --no-build --only LF_Test_PlaytestTownParityData
//! --world <the town mission's resource name>` (see docs/features/map.md). A boot with no config overlays (the
//! grid, a bare DebugWorld) has no pack tables to check and the test passes without rows.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_PlaytestTownParityData : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "playtest-town/parity-data";
	}

	//------------------------------------------------------------------------------------------------
	//! How many places of this kind stand in the world.
	protected int CountOf(string kind)
	{
		if (RedProof())
			return 0;

		array<IEntity> found = {};
		return LF_Places.FindAll(kind, found);
	}

	//------------------------------------------------------------------------------------------------
	protected bool HasPlace(string kind, string id)
	{
		if (RedProof())
			return false;

		return LF_Places.Find(kind, id) != null;
	}

	//------------------------------------------------------------------------------------------------
	protected bool GuideFinds(string kind, string id)
	{
		if (RedProof())
			return false;

		vector position;
		return LF_ActivityTargets.Resolve(kind, id, vector.Zero, position);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckPois(LF_TestContext ctx)
	{
		LF_MapPoiConfig.Reload();
		LF_MapPoiConfig config = LF_MapPoiConfig.Get();
		ctx.NotNull(config, "the POI table loads");
		if (!config || !config.m_aKinds)
			return;

		foreach (LF_MapPoiKind row : config.m_aKinds)
		{
			// A slot row ("StoreCounter/hardware") only restyles the places of its base kind.
			if (!row || LF_MapPoiConfig.IsSlotRow(row))
				continue;

			ctx.True(CountOf(row.m_sKind) > 0, "map POI " + row.m_sKind + " has a place in the town");
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckRuns(LF_TestContext ctx)
	{
		LF_MapRunConfig.ResetStatics();
		LF_MapRunConfig runs = LF_MapRunConfig.Get();
		ctx.NotNull(runs, "the run table loads");
		if (!runs || !runs.m_aRuns)
			return;

		foreach (LF_MapRun run : runs.m_aRuns)
		{
			if (!run || !run.m_aStops)
				continue;

			// A stop with no id takes a place of its kind this run has not used: needs one place each.
			map<string, int> open = new map<string, int>();
			foreach (int i, LF_MapRunStop stop : run.m_aStops)
			{
				string where = "run " + run.m_sId + " step " + (i + 1).ToString() + " (" + stop.m_sKind + " " + stop.m_sId + ")";
				if (!stop.m_sId.IsEmpty())
				{
					ctx.True(HasPlace(stop.m_sKind, stop.m_sId), where + " is placed in the town");
					continue;
				}

				int had;
				open.Find(stop.m_sKind, had);
				open.Set(stop.m_sKind, had + 1);
				if (!stop.m_sNear.IsEmpty())
				{
					int colon = stop.m_sNear.IndexOf(":");
					if (colon > 0)
					{
						string nearKind = stop.m_sNear.Substring(0, colon);
						string nearId = stop.m_sNear.Substring(colon + 1, stop.m_sNear.Length() - colon - 1);
						ctx.True(HasPlace(nearKind, nearId), where + " anchors on " + stop.m_sNear + ", placed in the town");
					}
				}
			}

			foreach (string kind, int need : open)
			{
				ctx.True(CountOf(kind) >= need, "run " + run.m_sId + " needs " + need.ToString() + " place(s) of kind " + kind + " in the town");
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckGuide(LF_TestContext ctx)
	{
		LF_ActivityTable.ResetStatics();
		LF_ActivityConfig table = LF_ActivityTable.Get();
		ctx.NotNull(table, "the activity table loads");
		if (!table || !table.m_aActivities)
			return;

		foreach (LF_ActivityDef def : table.m_aActivities)
		{
			if (!def)
				continue;

			if (!def.m_sPlaceKind.IsEmpty())
				ctx.True(GuideFinds(def.m_sPlaceKind, def.m_sPlaceId), "guide " + def.m_sId + ": its place (" + def.m_sPlaceKind + " " + def.m_sPlaceId + ") is placed in the town");

			if (!def.m_aSteps)
				continue;

			foreach (LF_ActivityStep step : def.m_aSteps)
			{
				if (!step || step.m_sTargetKind.IsEmpty())
					continue;

				ctx.True(GuideFinds(step.m_sTargetKind, step.m_sTargetId), "guide " + def.m_sId + " step " + step.m_sStepId + ": target (" + step.m_sTargetKind + " " + step.m_sTargetId + ") is placed in the town");
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_MissionHeader header = LF_MissionHeader.GetCurrent();
		if (!header || !header.m_aLF_ConfigOverlays || header.m_aLF_ConfigOverlays.IsEmpty())
		{
			LF_Debug.Info("PlaytestTownParity", "no config overlays under this header: nothing pack-side to check, run it with --world on the playtest town's header");
			// The grid and a bare DebugWorld have no pack tables to check; a test with no assertion counts as a failure.
			ctx.Pass("no config overlays under this header: no pack tables to check");
			return;
		}

		CheckPois(ctx);
		CheckRuns(ctx);
		CheckGuide(ctx);

		LF_MapPoiConfig.Reload();
		LF_MapRunConfig.ResetStatics();
		LF_ActivityTable.ResetStatics();
	}
}
