// red-proof: Observed 2026-11-16 with `toolscli test --no-build --red LF_Test_MapAreas`: skipping `LF_MapAreas.Collect` (the merge act) failed "a safe zone is on the map", "a lawless zone is on the map" and "the map-only town area is in the merged list" (RED PROOF OK). The first run, before LF_MapAreas existed, failed to compile. Shape, style and rule cases: node twin proof in tools/validation/map-areas-selftest.mjs.

//------------------------------------------------------------------------------------------------
//! The shared case table as one JSON string (Configs/Map/Rules/areas.cases.conf).
[BaseContainerProps(configRoot: true)]
class LF_Test_MapAreasCases
{
	[Attribute("", UIWidgets.EditBox, "The case table, JSON")]
	string m_sJson;
}

//------------------------------------------------------------------------------------------------
//! map/areas (#2233, epic #2223): the map's area rules in pure script. A polygon, a circle, a box
//! and a corridor become outline vertex lists; safe and lawless zones take their style from the
//! rule; the zones and the map-only areas merge into one list. The shape, style and rule cases are
//! the shared table Configs/Map/Rules/areas.cases.json, which tools/map/rules/areas.mjs (the
//! emulator's twin) runs too, so the two cannot drift. Merge cases need game state, so they are here.
// tier: LOGIC
class LF_Test_MapAreas : LF_Test
{
	//! The game cannot read a loose .json from an addon, so the table travels in this conf as one string;
	//! tools/validation/map-areas-selftest.mjs keeps it equal to areas.cases.json.
	static const ResourceName CASES_CONF = "{3E8A5D17B2C94F60}Configs/Map/Rules/areas.cases.conf";

	override string GetName()
	{
		return "map/areas";
	}

	override void Run(LF_TestContext ctx)
	{
		string text = ReadCases();
		ctx.True(!text.IsEmpty(), "the shared case table reads from the addon");
		SCR_JsonLoadContext json = new SCR_JsonLoadContext(false);
		ctx.True(json.ImportFromString(text), "the shared case table parses");

		float tolerance = 0.011;
		json.ReadValue("tolerance", tolerance);
		int count;
		ctx.True(json.StartArray("cases", count), "the table has cases");
		ctx.True(count >= 20, "the table has the shape, style and rule cases");
		for (int c = 0; c < count; c++)
		{
			json.StartObject();
			string name = string.Empty;
			json.ReadValue("name", name);
			string op = string.Empty;
			if (json.StartObject("input"))
			{
				json.ReadValue("op", op);
				if (op == "outline")
					RunOutlineCase(ctx, json, name, tolerance);
				else if (op == "style")
					RunStyleCase(ctx, json, name);
				else if (op == "rule")
					RunRuleCase(ctx, json, name);
				else
					ctx.True(false, string.Format("%1: unknown op '%2'", name, op));
			}
			else
			{
				ctx.True(false, string.Format("%1: no input", name));
			}

			json.EndObject();
		}

		json.EndArray();
		RunProperties(ctx);
		RunMerge(ctx);
	}

	//------------------------------------------------------------------------------------------------
	//! The reader is inside "input"; this closes it and "expect", and leaves the reader in the case object.
	protected void RunOutlineCase(LF_TestContext ctx, SCR_JsonLoadContext json, string name, float tolerance)
	{
		string shape = string.Empty;
		string pointsText = string.Empty;
		string centerText = string.Empty;
		string halfText = string.Empty;
		float radius = 0;
		float yaw = 0;
		int segments = LF_MapAreas.DEFAULT_SEGMENTS;
		json.ReadValue("shape", shape);
		json.ReadValue("points", pointsText);
		json.ReadValue("center", centerText);
		json.ReadValue("half", halfText);
		json.ReadValue("radius", radius);
		json.ReadValue("yaw", yaw);
		json.ReadValue("segments", segments);
		json.EndObject();

		string outlineText = string.Empty;
		string centroidText = string.Empty;
		if (json.StartObject("expect"))
		{
			json.ReadValue("outline", outlineText);
			json.ReadValue("centroid", centroidText);
			json.EndObject();
		}

		array<vector> points = {};
		array<vector> centers = {};
		array<vector> halves = {};
		array<vector> want = {};
		array<vector> wantCentroid = {};
		ParseList(pointsText, points);
		ParseList(centerText, centers);
		ParseList(halfText, halves);
		ParseList(outlineText, want);
		ParseList(centroidText, wantCentroid);
		vector center;
		if (!centers.IsEmpty())
			center = centers[0];
		vector half;
		if (!halves.IsEmpty())
			half = halves[0];

		array<vector> got = {};
		bool made;
		if (shape == "polygon")
			made = LF_MapAreas.PolygonOutline(points, got);
		else if (shape == "circle")
			made = LF_MapAreas.CircleOutline(center, radius, segments, got);
		else if (shape == "box")
			made = LF_MapAreas.BoxOutline(center, half, yaw, got);
		else if (shape == "corridor")
			made = LF_MapAreas.CorridorOutline(points, radius, segments, got);
		else
			ctx.True(false, string.Format("%1: unknown shape '%2'", name, shape));

		if (want.IsEmpty())
		{
			ctx.False(made, string.Format("%1: refused", name));
			ctx.True(got.IsEmpty(), string.Format("%1: a refusal leaves no vertices", name));
			return;
		}

		ctx.True(made, string.Format("%1: made an outline", name));
		ctx.Equal(want.Count(), got.Count(), string.Format("%1: vertex count", name));
		int checkCount = Math.Min(want.Count(), got.Count());
		for (int v = 0; v < checkCount; v++)
		{
			ctx.EqualFloat(want[v][0], got[v][0], tolerance, string.Format("%1: vertex %2 x", name, v));
			ctx.EqualFloat(want[v][2], got[v][2], tolerance, string.Format("%1: vertex %2 z", name, v));
		}

		if (!wantCentroid.IsEmpty())
		{
			vector gotCentroid = LF_MapAreas.Centroid(got);
			ctx.EqualFloat(wantCentroid[0][0], gotCentroid[0], tolerance, string.Format("%1: centroid x", name));
			ctx.EqualFloat(wantCentroid[0][2], gotCentroid[2], tolerance, string.Format("%1: centroid z", name));
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void RunStyleCase(LF_TestContext ctx, SCR_JsonLoadContext json, string name)
	{
		string kind = string.Empty;
		json.ReadValue("kind", kind);
		json.EndObject();

		string fillToken = string.Empty;
		string hatch = string.Empty;
		string lineToken = string.Empty;
		string dash = string.Empty;
		string labelKey = string.Empty;
		int fillAlpha = -1;
		int lineWidth = -1;
		if (json.StartObject("expect"))
		{
			json.ReadValue("fillToken", fillToken);
			json.ReadValue("hatch", hatch);
			json.ReadValue("lineToken", lineToken);
			json.ReadValue("dash", dash);
			json.ReadValue("labelKey", labelKey);
			json.ReadValue("fillAlpha", fillAlpha);
			json.ReadValue("lineWidth", lineWidth);
			json.EndObject();
		}

		LF_MapAreaStyle style = typename.StringToEnum(LF_MapAreaStyle, kind);
		ctx.EqualStr(kind, typename.EnumToString(LF_MapAreaStyle, style), string.Format("%1: a known kind", name));
		LF_MapAreaLook look = LF_MapAreas.LookOf(style);
		ctx.EqualStr(fillToken, look.m_sFillToken, string.Format("%1: fill token", name));
		ctx.Equal(fillAlpha, look.m_iFillAlpha, string.Format("%1: fill alpha", name));
		ctx.EqualStr(hatch, look.m_sHatch, string.Format("%1: hatch", name));
		ctx.EqualStr(lineToken, look.m_sLineToken, string.Format("%1: line token", name));
		ctx.Equal(lineWidth, look.m_iLineWidth, string.Format("%1: line width", name));
		ctx.EqualStr(dash, look.m_sDash, string.Format("%1: dash", name));
		ctx.EqualStr(labelKey, look.m_sLabelKey, string.Format("%1: label key", name));
	}

	//------------------------------------------------------------------------------------------------
	protected void RunRuleCase(LF_TestContext ctx, SCR_JsonLoadContext json, string name)
	{
		string ruleName = string.Empty;
		json.ReadValue("rule", ruleName);
		json.EndObject();

		string kind = string.Empty;
		if (json.StartObject("expect"))
		{
			json.ReadValue("kind", kind);
			json.EndObject();
		}

		LF_ZoneRule rule = typename.StringToEnum(LF_ZoneRule, ruleName);
		ctx.EqualStr(ruleName, typename.EnumToString(LF_ZoneRule, rule), string.Format("%1: a known zone rule", name));
		LF_MapAreaStyle style;
		string gotKind = string.Empty;
		if (LF_MapAreas.StyleForRule(rule, style))
			gotKind = typename.EnumToString(LF_MapAreaStyle, style);

		ctx.EqualStr(kind, gotKind, string.Format("%1: the style", name));
	}

	//------------------------------------------------------------------------------------------------
	//! What a table of numbers cannot say: the outline of a real zone is the zone's own edge.
	protected void RunProperties(LF_TestContext ctx)
	{
		LF_ZoneArea circle = new LF_ZoneArea();
		circle.m_sName = "ma-prop-circle";
		circle.m_eRule = LF_ZoneRule.SAFE;
		circle.m_eShape = LF_ZoneShape.CIRCLE;
		circle.m_vCenter = "300 0 400";
		circle.m_fRadius = 25;
		CheckEdge(ctx, circle, "circle");

		LF_ZoneArea box = new LF_ZoneArea();
		box.m_sName = "ma-prop-box";
		box.m_eRule = LF_ZoneRule.LAWLESS;
		box.m_eShape = LF_ZoneShape.BOX;
		box.m_vCenter = "500 0 600";
		box.m_fYaw = 33;
		box.m_vHalfExtents = "40 0 12";
		CheckEdge(ctx, box, "turned box");

		LF_ZoneArea line = new LF_ZoneArea();
		line.m_sName = "ma-prop-line";
		line.m_eRule = LF_ZoneRule.LAWLESS;
		line.m_eShape = LF_ZoneShape.POLYLINE;
		line.m_fRadius = 6;
		line.m_aPoints.Insert("700 0 700");
		line.m_aPoints.Insert("780 0 700");
		CheckEdge(ctx, line, "straight corridor");
	}

	//------------------------------------------------------------------------------------------------
	//! Pulled 5 % toward the centroid every outline vertex is inside the zone; pushed 5 % out it is not.
	protected void CheckEdge(LF_TestContext ctx, LF_ZoneArea zone, string label)
	{
		array<vector> outline = {};
		ctx.True(LF_MapAreas.OutlineOfZone(zone, outline), string.Format("%1: the zone makes an outline", label));
		vector middle = LF_MapAreas.Centroid(outline);
		int insideCount;
		int outsideCount;
		foreach (vector point : outline)
		{
			vector inward = middle + (point - middle) * 0.95;
			vector outward = middle + (point - middle) * 1.05;
			if (zone.Contains(inward))
				insideCount++;
			if (!zone.Contains(outward))
				outsideCount++;
		}

		ctx.Equal(outline.Count(), insideCount, string.Format("%1: every vertex pulled in is inside the zone", label));
		ctx.Equal(outline.Count(), outsideCount, string.Format("%1: every vertex pushed out is outside the zone", label));
	}

	//------------------------------------------------------------------------------------------------
	protected void RunMerge(LF_TestContext ctx)
	{
		LF_Zones.RegisterCircle("ma-safe", LF_ZoneRule.SAFE, "9000 0 9000", 30);
		LF_Zones.RegisterCircle("ma-normal", LF_ZoneRule.NORMAL, "9500 0 9000", 30);
		LF_ZoneArea lawless = new LF_ZoneArea();
		lawless.m_sName = "ma-lawless";
		lawless.m_eRule = LF_ZoneRule.LAWLESS;
		lawless.m_eShape = LF_ZoneShape.BOX;
		lawless.m_vCenter = "9000 0 9500";
		lawless.m_vHalfExtents = "20 0 10";
		LF_Zones.Register(lawless);

		LF_MapArea town = new LF_MapArea();
		town.m_sName = "ma-town";
		town.m_eStyle = LF_MapAreaStyle.TOWN;
		town.m_sLabel = "Test Town";
		town.m_aOutline.Insert("9900 0 9900");
		town.m_aOutline.Insert("9990 0 9900");
		town.m_aOutline.Insert("9990 0 9990");
		ctx.True(LF_MapAreas.Register(town), "a map-only town area registers");

		LF_MapArea thin = new LF_MapArea();
		thin.m_sName = "ma-thin";
		thin.m_aOutline.Insert("1 0 1");
		thin.m_aOutline.Insert("2 0 2");
		ctx.False(LF_MapAreas.Register(thin), "an area with two vertices is refused");
		LF_MapArea nameless = new LF_MapArea();
		nameless.m_aOutline.Insert("1 0 1");
		nameless.m_aOutline.Insert("2 0 2");
		nameless.m_aOutline.Insert("3 0 1");
		ctx.False(LF_MapAreas.Register(nameless), "an area with no name is refused");

		array<ref LF_MapArea> merged = {};
		if (!RedProof())
			LF_MapAreas.Collect(merged);

		LF_MapArea safeArea = Named(merged, "ma-safe");
		ctx.True(safeArea != null, "a safe zone is on the map");
		if (safeArea)
		{
			ctx.True(safeArea.m_eStyle == LF_MapAreaStyle.SAFE, "the safe zone takes the safe style");
			ctx.True(safeArea.m_bFromZone, "the safe zone says it came from LF_Zones");
			ctx.Equal(LF_MapAreas.DEFAULT_SEGMENTS, safeArea.m_aOutline.Count(), "the circular zone is a 32-gon");
			ctx.EqualFloat(9000, safeArea.m_vCentroid[0], 0.01, "the zone's label sits at its centre");
		}

		LF_MapArea lawlessArea = Named(merged, "ma-lawless");
		ctx.True(lawlessArea != null, "a lawless zone is on the map");
		if (lawlessArea)
		{
			ctx.True(lawlessArea.m_eStyle == LF_MapAreaStyle.LAWLESS, "the lawless zone takes the lawless style");
			ctx.Equal(4, lawlessArea.m_aOutline.Count(), "the box zone is four corners");
		}

		ctx.True(Named(merged, "ma-normal") == null, "a normal zone draws nothing");

		LF_MapArea townArea = Named(merged, "ma-town");
		ctx.True(townArea != null, "the map-only town area is in the merged list");
		if (townArea)
		{
			ctx.True(townArea.m_eStyle == LF_MapAreaStyle.TOWN, "the town area keeps its style");
			ctx.False(townArea.m_bFromZone, "the town area did not come from LF_Zones");
			ctx.EqualStr("Test Town", LF_MapAreas.LabelKeyOf(townArea), "a given label wins over the style's label");
			ctx.EqualFloat(9960, townArea.m_vCentroid[0], 0.01, "the town area's centroid is worked out when it registers");
		}

		if (safeArea)
			ctx.EqualStr("#LF-Map_AreaSafe", LF_MapAreas.LabelKeyOf(safeArea), "a safe zone without a label uses the style's key");

		// A zone that goes leaves the map; an area that goes leaves too; merging again is a fresh answer.
		LF_Zones.Unregister("ma-safe");
		LF_MapAreas.Unregister("ma-town");
		array<ref LF_MapArea> after = {};
		LF_MapAreas.Collect(after);
		ctx.True(Named(after, "ma-safe") == null, "a removed zone leaves the map");
		ctx.True(Named(after, "ma-town") == null, "a removed map area leaves the map");
		ctx.True(Named(after, "ma-lawless") != null, "the other zone stays");

		LF_Zones.Unregister("ma-lawless");
		LF_Zones.Unregister("ma-normal");
	}

	//------------------------------------------------------------------------------------------------
	protected LF_MapArea Named(array<ref LF_MapArea> list, string name)
	{
		foreach (LF_MapArea area : list)
		{
			if (area.m_sName == name)
				return area;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! "x z x z ..." to vectors (x, 0, z).
	protected void ParseList(string text, notnull array<vector> into)
	{
		array<string> parts = {};
		text.Split(" ", parts, true);
		for (int i = 0; i + 1 < parts.Count(); i += 2)
			into.Insert(Vector(parts[i].ToFloat(), 0, parts[i + 1].ToFloat()));
	}

	//------------------------------------------------------------------------------------------------
	protected string ReadCases()
	{
		Resource container = BaseContainerTools.LoadContainer(CASES_CONF);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error("MapAreas", string.Format("test: case table conf does not resolve: %1", CASES_CONF));
			return string.Empty;
		}

		LF_Test_MapAreasCases table = LF_Test_MapAreasCases.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!table)
		{
			LF_Debug.Error("MapAreas", "test: case table conf has an unexpected root class");
			return string.Empty;
		}

		return table.m_sJson;
	}
}
