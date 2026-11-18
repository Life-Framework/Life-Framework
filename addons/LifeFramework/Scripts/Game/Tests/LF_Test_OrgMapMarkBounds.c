// red-proof: 2026-11-18, cli test --no-build --only LF_Test_OrgMapMarkBounds on the unfixed Add: FAIL, 10 assertions (icon and colour at list size and all four out-of-map edges accepted, got serials 1-8); green with the fix.
//! map/org-mark-bounds (#2513, card M12 follow-up): an org mark is refused when its icon or colour index
//! is past the placed-marker config's lists (LF_MapMarkerConfig.conf: an index off the end draws a white
//! circle on every member's map) or its position is outside the map, not only outside the int16 wire
//! range. The limits are read here from the engine's own marker manager and the world's bound box (the dedicated server has no map entity), independent of
//! the code under test. An in-range mark is still added.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_OrgMapMarkBounds : LF_Test
{
	protected static const string FOUNDER = "orgmarkbounds-founder";
	protected static const int P_FOUNDER = 1;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "map/org-mark-bounds";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		SCR_MapMarkerManagerComponent manager = SCR_MapMarkerManagerComponent.GetInstance();
		ctx.NotNull(manager, "the game mode carries the marker manager");
		if (!manager || !manager.GetMarkerConfig())
			return;

		SCR_MapMarkerEntryPlaced placed = SCR_MapMarkerEntryPlaced.Cast(manager.GetMarkerConfig().GetMarkerEntryConfigByType(SCR_EMapMarkerType.PLACED_CUSTOM));
		ctx.NotNull(placed, "the marker config has a placed marker entry");
		if (!placed || !placed.GetIconEntries() || !placed.GetColorEntries())
			return;

		int iconCount = placed.GetIconEntries().Count();
		int colourCount = placed.GetColorEntries().Count();
		ctx.True(iconCount > 0 && colourCount > 0, "the placed marker lists are not empty");

		vector mins, maxs;
		GetGame().GetWorld().GetBoundBox(mins, maxs);
		int minX = Math.Floor(mins[0]);
		int minZ = Math.Floor(mins[2]);
		int maxX = Math.Ceil(maxs[0]);
		int maxZ = Math.Ceil(maxs[2]);
		ctx.True(maxX > minX && maxZ > minZ, "the world has an extent");
		int midX = (minX + maxX) / 2;
		int midZ = (minZ + maxZ) / 2;

		array<ref LF_OrgMapMarkRow> before = LF_OrgMapMarks.ExportAll();
		array<ref LF_OrganizationRecord> orgsBefore = LF_OrganizationManager.ExportAll();
		array<ref LF_OrganizationRankRow> ranksBefore = LF_OrganizationManager.ExportRanks();
		LF_OrgMapMarks.ApplyAll(null);
		LF_OrgMapMarks.ResetStatics();
		LF_MapShares.ResetStatics();
		LF_OrganizationManager.Reset();

		LF_Test_MapShareSink sink = new LF_Test_MapShareSink();
		LF_Test_MapShareFacts facts = new LF_Test_MapShareFacts();
		LF_MapShares.SetSink(sink);
		LF_MapShares.SetFacts(facts);

		LF_Organization org = LF_OrganizationManager.Create(FOUNDER, "Bounds Crew");
		ctx.NotNull(org, "the organization is founded");
		if (!org)
		{
			Restore(before, orgsBefore, ranksBefore);
			return;
		}

		string orgId = org.GetId();
		facts.Set(P_FOUNDER, FOUNDER, "", orgId);
		ctx.True(org.Allows(FOUNDER, LF_OrgMapMarks.RIGHT), "the founder holds the territory right");

		float now = 100;
		ctx.EqualStr("", LF_OrgMapMarks.Add(FOUNDER, midX, midZ, iconCount, 0, "", now), "an icon index equal to the icon list size is refused");
		now += 10;
		ctx.EqualStr("", LF_OrgMapMarks.Add(FOUNDER, midX, midZ, 0, colourCount, "", now), "a colour index equal to the colour list size is refused");
		now += 10;
		ctx.EqualStr("", LF_OrgMapMarks.Add(FOUNDER, maxX + 1, midZ, 0, 0, "", now), "x past the map extent is refused");
		now += 10;
		ctx.EqualStr("", LF_OrgMapMarks.Add(FOUNDER, minX - 1, midZ, 0, 0, "", now), "x before the map extent is refused");
		now += 10;
		ctx.EqualStr("", LF_OrgMapMarks.Add(FOUNDER, midX, maxZ + 1, 0, 0, "", now), "z past the map extent is refused");
		now += 10;
		ctx.EqualStr("", LF_OrgMapMarks.Add(FOUNDER, midX, minZ - 1, 0, 0, "", now), "z before the map extent is refused");
		ctx.Equal(0, LF_OrgMapMarks.CountOf(orgId), "the refusals added nothing");

		now += 10;
		string last = LF_OrgMapMarks.Add(FOUNDER, midX, midZ, iconCount - 1, colourCount - 1, "last entries", now);
		ctx.EqualStr(LF_OrgMapMarks.KeyOf(orgId, 1), last, "the last icon and colour entries are accepted");
		now += 10;
		string edge = LF_OrgMapMarks.Add(FOUNDER, maxX, maxZ, 0, 0, "far corner", now);
		ctx.EqualStr(LF_OrgMapMarks.KeyOf(orgId, 2), edge, "a mark on the map's far edge is accepted");
		ctx.Equal(2, LF_OrgMapMarks.CountOf(orgId), "the in-range marks are kept");

		Restore(before, orgsBefore, ranksBefore);
	}

	//------------------------------------------------------------------------------------------------
	protected void Restore(array<ref LF_OrgMapMarkRow> before, array<ref LF_OrganizationRecord> orgsBefore, array<ref LF_OrganizationRankRow> ranksBefore)
	{
		LF_OrganizationManager.Reset();
		LF_OrganizationManager.ApplyAll(orgsBefore);
		LF_OrganizationManager.ApplyRanks(ranksBefore);
		LF_MapShares.ResetStatics();
		LF_OrgMapMarks.ResetStatics();
		LF_MapShares.EnsureStarted();
		LF_OrgMapMarks.ApplyAll(before);
	}
}
