// red-proof: 2026-11-17, cli test --no-build --red LF_Test_MapShareAreas: RED-OK map/share-areas (RedProof() makes the test sink draw areas but never drop one on a remove or a clear, so the remove, the expiry and the moved-audience checks fail). Green without it.
// tier: LOGIC
//! A share row's area on the client (#2243 follow-up, everon-map.md sections 2 and 3). The test sink stands
//! in for the owner RPC and calls what LF_MapShareComponent calls on the client: LF_MapShareAreas.Show for
//! an add, Hide for a remove, HideAll for the clear before a resend. A row with a radius and a style
//! becomes a local LF_MapAreas circle named after its key, in that style and at that radius; a row without
//! one (and a row whose area is not drawable) draws none; a replaced row replaces its area; a remove, an
//! expiry, a spawn's clear-and-resend and a row moved out of the audience leave no stale circle.
class LF_MapShareAreaTestSink : LF_Test_MapShareSink
{
	//! Red proof: the client draws areas but never drops one (a remove, a clear and a replace leave them).
	bool m_bKeepStale;

	override void Add(int playerId, LF_MapShareRow row)
	{
		super.Add(playerId, row);
		LF_MapShareAreas.Show(row.m_sKey, row.m_iX, row.m_iZ, row.m_iAreaRadius, row.m_iAreaStyle, row.m_sText);
	}

	override void Remove(int playerId, string key)
	{
		super.Remove(playerId, key);
		if (!m_bKeepStale)
			LF_MapShareAreas.Hide(key);
	}

	override void Clear(int playerId)
	{
		super.Clear(playerId);
		if (!m_bKeepStale)
			LF_MapShareAreas.HideAll();
	}
}

//------------------------------------------------------------------------------------------------
class LF_Test_MapShareAreas : LF_Test
{
	protected static const int PLAYER = 1;
	protected static const string KEY = "event:fair";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "map/share-areas";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_MapShares.ResetStatics();
		LF_MapAreas.ResetStatics();
		LF_MapShareAreas.ResetStatics();
		RunAreas(ctx);
		LF_MapShares.ResetStatics();
		LF_MapAreas.ResetStatics();
		LF_MapShareAreas.ResetStatics();
		LF_MapShares.EnsureStarted();
	}

	//------------------------------------------------------------------------------------------------
	protected static float RadiusOfArea(LF_MapArea area, vector centre)
	{
		float dx = area.m_aOutline[0][0] - centre[0];
		float dz = area.m_aOutline[0][2] - centre[2];
		return Math.Sqrt(dx * dx + dz * dz);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunAreas(LF_TestContext ctx)
	{
		LF_MapShareAreaTestSink sink = new LF_MapShareAreaTestSink();
		sink.m_bKeepStale = RedProof();
		LF_Test_MapShareFacts facts = new LF_Test_MapShareFacts();
		facts.Set(PLAYER, "char-a", "", "");
		LF_MapShares.SetSink(sink);
		LF_MapShares.SetFacts(facts);
		vector centre = Vector(120, 0, 340);

		// A row with an area becomes a client circle in its style, at its radius, around its position.
		LF_MapShareRow row = LF_MapShareRow.Make(KEY, LF_EMapShareAudience.EVERYONE, "", 120, 340, 0, 0, "#LF-EventMap_Place", 0);
		row.WithArea(150, LF_MapAreaStyle.EVENT);
		ctx.True(LF_MapShares.Put(row), "a row with an area is put");
		LF_MapArea area = LF_MapAreas.Find(KEY);
		ctx.True(area != null, "the client draws an area for the row");
		ctx.True(LF_MapShareAreas.IsShown(KEY), "the client knows it drew the row's area");
		if (area)
		{
			ctx.True(area.m_eStyle == LF_MapAreaStyle.EVENT, "the area has the row's style");
			ctx.Equal(LF_MapAreas.DEFAULT_SEGMENTS, area.m_aOutline.Count(), "a circle of 32 sides");
			ctx.True(Math.AbsFloat(RadiusOfArea(area, centre) - 150) < 0.01, "the area has the row's radius");
			ctx.True(Math.AbsFloat(area.m_vCentroid[0] - 120) < 0.5 && Math.AbsFloat(area.m_vCentroid[2] - 340) < 0.5, "the area is centred on the row's position");
			ctx.EqualStr("#LF-EventMap_Place", LF_MapAreas.LabelKeyOf(area), "the area carries the row's text as its label");
		}

		// A row with no area draws none.
		LF_MapShares.Put(LF_MapShareRow.Make("taxi:1", LF_EMapShareAudience.EVERYONE, "", 5, 5, 0, 0, "", 0));
		ctx.True(LF_MapAreas.Find("taxi:1") == null, "a plain mark draws no area");
		ctx.False(LF_MapShareAreas.IsShown("taxi:1"), "and is not tracked as one");

		// An area the wire cannot carry is dropped at the server and never drawn: the mark still goes.
		LF_MapShareRow wide = LF_MapShareRow.Make("event:wide", LF_EMapShareAudience.EVERYONE, "", 5, 5, 0, 0, "", 0);
		wide.WithArea(LF_MapShareRules.AREA_RADIUS_MAX + 1, LF_MapAreaStyle.EVENT);
		ctx.True(LF_MapShares.Put(wide), "a row with an oversize area is still put");
		ctx.True(sink.Got("add:1:event:wide"), "its mark is sent");
		ctx.Equal(0, wide.m_iAreaRadius, "the server dropped the oversize area");
		ctx.True(LF_MapAreas.Find("event:wide") == null, "no area is drawn for it");
		LF_MapShareRow badStyle = LF_MapShareRow.Make("event:style", LF_EMapShareAudience.EVERYONE, "", 5, 5, 0, 0, "", 0);
		badStyle.m_iAreaRadius = 50;
		badStyle.m_iAreaStyle = 99;
		LF_MapShares.Put(badStyle);
		ctx.True(LF_MapAreas.Find("event:style") == null, "an unknown style id draws no area");

		// A replaced row replaces its area; it never leaves two.
		LF_MapShareRow grown = LF_MapShareRow.Make(KEY, LF_EMapShareAudience.EVERYONE, "", 120, 340, 0, 0, "#LF-EventMap_Place", 0);
		grown.WithArea(300, LF_MapAreaStyle.GATED);
		LF_MapShares.Put(grown);
		area = LF_MapAreas.Find(KEY);
		ctx.True(area != null, "the replaced row still has an area");
		if (area)
		{
			ctx.True(Math.AbsFloat(RadiusOfArea(area, centre) - 300) < 0.01, "the replacement takes the new radius");
			ctx.True(area.m_eStyle == LF_MapAreaStyle.GATED, "the replacement takes the new style");
		}

		array<ref LF_MapArea> drawn = {};
		LF_MapAreas.Collect(drawn);
		int named;
		foreach (LF_MapArea one : drawn)
		{
			if (one.m_sName == KEY)
				named++;
		}
		ctx.Equal(1, named, "one key holds one area");

		// A replacement without an area takes the old circle off.
		LF_MapShares.Put(LF_MapShareRow.Make(KEY, LF_EMapShareAudience.EVERYONE, "", 120, 340, 0, 0, "", 0));
		ctx.True(LF_MapAreas.Find(KEY) == null, "a row replaced without an area drops its circle");
		LF_MapShareRow again = LF_MapShareRow.Make(KEY, LF_EMapShareAudience.EVERYONE, "", 120, 340, 0, 0, "#LF-EventMap_Place", 0);
		again.WithArea(150, LF_MapAreaStyle.EVENT);
		LF_MapShares.Put(again);

		// A spawn clears the client and resends: the area is there again afterwards.
		sink.m_aSent.Clear();
		LF_MapShares.OnSpawned(PLAYER);
		ctx.True(sink.Got("clear:1"), "a spawn clears the client first");
		ctx.True(LF_MapAreas.Find(KEY) != null, "a late joiner or respawn gets the area back with the resend");
		ctx.True(LF_MapAreas.Find("event:wide") == null, "the resend does not invent an area");

		// Remove clears it.
		ctx.True(LF_MapShares.Remove(KEY), "the row is removed");
		ctx.True(LF_MapAreas.Find(KEY) == null, "removing the row clears its client area");
		ctx.False(LF_MapShareAreas.IsShown(KEY), "and the client forgets it");

		// Expiry clears it.
		LF_MapShareRow timed = LF_MapShareRow.Make("event:timed", LF_EMapShareAudience.EVERYONE, "", 10, 10, 0, 0, "", 50);
		timed.WithArea(80, LF_MapAreaStyle.EVENT);
		LF_MapShares.Put(timed);
		ctx.True(LF_MapAreas.Find("event:timed") != null, "a timed row draws its area");
		LF_MapShares.Expire(50);
		ctx.True(LF_MapAreas.Find("event:timed") == null, "an expired row's area is gone");

		// A row moved out of the player's audience takes its area with it.
		LF_MapShareRow everyone = LF_MapShareRow.Make("dispatch:1", LF_EMapShareAudience.EVERYONE, "", 10, 10, 0, 0, "", 0);
		everyone.WithArea(60, LF_MapAreaStyle.CUSTOM);
		LF_MapShares.Put(everyone);
		ctx.True(LF_MapAreas.Find("dispatch:1") != null, "an everyone row draws its area");
		LF_MapShareRow moved = LF_MapShareRow.Make("dispatch:1", LF_EMapShareAudience.ROLE, LF_MapShares.ROLE_POLICE, 10, 10, 0, 0, "", 0);
		moved.WithArea(60, LF_MapAreaStyle.CUSTOM);
		LF_MapShares.Put(moved);
		ctx.True(LF_MapAreas.Find("dispatch:1") == null, "a row moved to an audience the player is not in loses its area");

		// Only share areas go: a map-only area of another name stays.
		LF_MapAreas.RegisterCircle("town_line", LF_MapAreaStyle.TOWN, "", Vector(0, 0, 0), 100);
		LF_MapShareAreas.HideAll();
		ctx.True(LF_MapAreas.Find("town_line") != null, "clearing share areas leaves other areas alone");
		LF_MapAreas.Unregister("town_line");
	}
}
