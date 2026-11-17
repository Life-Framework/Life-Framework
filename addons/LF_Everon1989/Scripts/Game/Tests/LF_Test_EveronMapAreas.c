// red-proof: 2026-11-17, cli test --no-build --red LF_Test_EveronMapAreas: RED-OK (RedProof() skips LF_MapAreas.Collect, so "the map draws the town line" and "the map draws the safe square" fail). Green without it under the pack header. Origin checks (map proof pass): failed on the quoted conf ("a town line point is not the world origin: <0,0,0>", 12 times), green on the unquoted one.
// tier: WORLD
//! everon/map-areas (#2233 split M5b): under the Everon 1989 header (the pack leg), the header lists
//! the pack's area overlay, the layered table holds Saint-Philippe's town line and safe square over
//! the empty core file, and the map's merged list draws both. No lawless area until M7 (D4).
class LF_Test_EveronMapAreas : LF_Test
{
	static const ResourceName AREAS_OVERLAY = "{304F85569D5FEC87}Configs/Map/LF_MapAreas_Everon1989.conf";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/map-areas";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_MissionHeader header = LF_MissionHeader.GetCurrent();
		ctx.NotNull(header, "the pack leg runs under an LF mission header");
		if (header && header.m_aLF_ConfigOverlays)
			ctx.True(header.m_aLF_ConfigOverlays.Find(AREAS_OVERLAY) != -1, "the header lists the Everon map area overlay");
		else
			ctx.Fail("the header lists no config overlays");

		LF_MapAreaConfig.Reload();
		LF_MapAreaConfig config = LF_MapAreaConfig.Get();
		ctx.NotNull(config, "the area table loads under the pack");
		if (!config)
			return;

		LF_MapAreaRow line = config.Find("saint-philippe-town-line");
		ctx.NotNull(line, "the pack adds the Saint-Philippe town line");
		if (line)
		{
			ctx.True(line.m_eStyle == LF_MapAreaStyle.TOWN, "the town line is the town style");
			ctx.True(line.m_eShape == LF_MapAreaShape.POLYGON, "the town line is a polygon");
			ctx.True(line.m_aPoints && line.m_aPoints.Count() >= 3, "the town line has its points");
			// The conf's values reach the row (a quoted vector read as zeros: the map proof pass found the
			// line drawn from the world origin). No coordinates asserted, only that none is the origin.
			if (line.m_aPoints)
			{
				foreach (vector point : line.m_aPoints)
					ctx.True(point[0] != 0 || point[2] != 0, string.Format("a town line point is not the world origin: %1", point));
			}
		}

		LF_MapAreaRow square = config.Find("saint-philippe-square-safe");
		ctx.NotNull(square, "the pack adds the Saint-Philippe safe square");
		if (square)
		{
			ctx.True(square.m_eStyle == LF_MapAreaStyle.SAFE, "the square is the safe style");
			ctx.True(square.m_eShape == LF_MapAreaShape.CIRCLE, "the square is a circle");
			ctx.True(square.m_fRadius > 0, "the square has a radius");
			ctx.True(square.m_vCentre[0] != 0 || square.m_vCentre[2] != 0, string.Format("the square's centre is not the world origin: %1", square.m_vCentre));
			if (line && line.m_aPoints && !line.m_aPoints.IsEmpty())
			{
				float minX = line.m_aPoints[0][0];
				float maxX = minX;
				float minZ = line.m_aPoints[0][2];
				float maxZ = minZ;
				foreach (vector corner : line.m_aPoints)
				{
					minX = Math.Min(minX, corner[0]);
					maxX = Math.Max(maxX, corner[0]);
					minZ = Math.Min(minZ, corner[2]);
					maxZ = Math.Max(maxZ, corner[2]);
				}

				vector c = square.m_vCentre;
				ctx.True(c[0] >= minX && c[0] <= maxX && c[2] >= minZ && c[2] <= maxZ, "the safe square sits inside the town line's box");
			}
		}

		foreach (LF_MapAreaRow row : config.m_aAreas)
			ctx.True(row.m_eStyle != LF_MapAreaStyle.LAWLESS, string.Format("no lawless area until M7 (D4): %1", row.m_sId));

		// The act: the map collects its areas.
		array<ref LF_MapArea> areas = {};
		if (!RedProof())
			LF_MapAreas.Collect(areas);

		bool hasLine;
		bool hasSquare;
		foreach (LF_MapArea area : areas)
		{
			if (area.m_sName == "saint-philippe-town-line" && area.m_aOutline.Count() >= 3)
				hasLine = true;
			if (area.m_sName == "saint-philippe-square-safe" && area.m_aOutline.Count() >= 3)
				hasSquare = true;
		}

		ctx.True(hasLine, "the map draws the town line");
		ctx.True(hasSquare, "the map draws the safe square");
	}
}
