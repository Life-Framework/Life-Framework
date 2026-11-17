// red-proof: 2026-11-17, cli test --no-build --red LF_Test_MapAreaConf: RED-OK (RedProof() skips LF_MapAreas.AppendConfigRows, so "the zone's own area and the two rows are in the list (expected 3, got 1)", "the circle row is on the map" and "the polygon row is on the map" fail). Green without it: passed=2 with LF_Test_MapAreas.
//------------------------------------------------------------------------------------------------
// tier: LOGIC
//! map/area-conf (#2233 split M5b, everon-map.md section 2): the area table Configs/Map/LF_MapAreas.conf.
//! The core file loads and holds no row (core holds no coordinate). Its rows, a circle and a polygon,
//! become map areas through LF_MapAreas.AppendConfigRows; a row with no outline is skipped; a name
//! already in the list (a zone entity, a layer entity) is not drawn twice. A pack's own rows, merged
//! over the core file, are tested in the pack.
class LF_Test_MapAreaConf : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "map/area-conf";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_MapAreaConfig core = LF_MapAreaConfig.LoadCore();
		ctx.NotNull(core, "the core area table loads");
		if (core)
		{
			ctx.NotNull(core.m_aAreas, "the core table has a row list");
			ctx.Equal(0, core.m_aAreas.Count(), "core holds no area row: a pack's rows hold the places");
		}

		LF_MapAreaConfig layered = LF_MapAreaConfig.Get();
		ctx.NotNull(layered, "the layered area table loads");
		if (layered && core)
			ctx.True(layered.m_aAreas.Count() >= core.m_aAreas.Count(), "layering a pack over the core file never drops a core row");

		RunRows(ctx);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunRows(LF_TestContext ctx)
	{
		LF_MapAreaConfig config = new LF_MapAreaConfig();
		config.m_aAreas = new array<ref LF_MapAreaRow>();
		config.m_aAreas.Insert(LF_MapAreaRow.MakeCircle("test-square", LF_MapAreaStyle.SAFE, Vector(100, 0, 200), 40));
		array<vector> corners = {Vector(0, 0, 0), Vector(100, 0, 0), Vector(100, 0, 100), Vector(0, 0, 100)};
		config.m_aAreas.Insert(LF_MapAreaRow.MakePolygon("test-line", LF_MapAreaStyle.TOWN, corners));
		array<vector> twoPoints = {Vector(0, 0, 0), Vector(10, 0, 0)};
		config.m_aAreas.Insert(LF_MapAreaRow.MakePolygon("test-too-few", LF_MapAreaStyle.CUSTOM, twoPoints));
		config.m_aAreas.Insert(LF_MapAreaRow.MakeCircle("test-no-radius", LF_MapAreaStyle.CUSTOM, vector.Zero, 0));
		config.m_aAreas.Insert(LF_MapAreaRow.MakeCircle("test-zone", LF_MapAreaStyle.SAFE, Vector(5, 0, 5), 10));

		array<ref LF_MapArea> list = {};
		LF_MapArea fromZone = new LF_MapArea();
		fromZone.m_sName = "test-zone";
		fromZone.m_eStyle = LF_MapAreaStyle.LAWLESS;
		list.Insert(fromZone);

		// The act: the conf rows join the map list.
		int added;
		if (!RedProof())
			added = LF_MapAreas.AppendConfigRows(config, list);

		ctx.Equal(2, added, "the circle and the polygon are added; the two with no outline and the name taken are not");
		ctx.Equal(3, list.Count(), "the zone's own area and the two rows are in the list");

		LF_MapArea square = FindIn(list, "test-square");
		ctx.NotNull(square, "the circle row is on the map");
		if (square)
		{
			ctx.True(square.m_eStyle == LF_MapAreaStyle.SAFE, "the row's style is the area's style");
			ctx.Equal(LF_MapAreas.DEFAULT_SEGMENTS, square.m_aOutline.Count(), "a circle row is a 32-gon");
			ctx.True(vector.DistanceXZ(Vector(100, 0, 200), square.m_vCentroid) < 0.5, "the label sits at the circle's centre");
		}

		LF_MapArea line = FindIn(list, "test-line");
		ctx.NotNull(line, "the polygon row is on the map");
		if (line)
		{
			ctx.Equal(4, line.m_aOutline.Count(), "a polygon row keeps its points");
			ctx.True(vector.DistanceXZ(Vector(50, 0, 50), line.m_vCentroid) < 0.5, "the label sits at the polygon's centroid");
		}

		ctx.True(FindIn(list, "test-too-few") == null, "a polygon with two points makes no area");
		ctx.True(FindIn(list, "test-no-radius") == null, "a circle with no radius makes no area");
		LF_MapArea kept = FindIn(list, "test-zone");
		ctx.True(kept && kept.m_eStyle == LF_MapAreaStyle.LAWLESS, "the zone's area stays; the row of the same name is not drawn twice");
		ctx.Equal(0, LF_MapAreas.AppendConfigRows(null, list), "a missing table adds nothing");
	}

	//------------------------------------------------------------------------------------------------
	protected LF_MapArea FindIn(notnull array<ref LF_MapArea> list, string name)
	{
		foreach (LF_MapArea area : list)
		{
			if (area.m_sName == name)
				return area;
		}

		return null;
	}
}
