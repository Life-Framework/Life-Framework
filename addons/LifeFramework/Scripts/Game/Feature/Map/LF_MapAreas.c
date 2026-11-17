//------------------------------------------------------------------------------------------------
//! Map area data (#2233, epic #2223, docs/architecture/everon-map.md section 2). One list of areas
//! for the map to draw: the safe and lawless zones LF_Zones already holds, merged with the map-only
//! areas (town line, gated, custom) that LF_MapAreaComponent registers. Each area is an outline
//! vertex list (flat x, z) with a style, so the HUD cue and the map cannot disagree.
//!
//! The outline and style rules are pure and have a node twin, tools/map/rules/areas.mjs, which the
//! offline map emulator calls. Both run the case table Configs/Map/Rules/areas.cases.json
//! (LF_Test_MapAreas here, validation/map-areas-selftest.mjs there); change one, change the other.
//! The drawing is card M6 (LF_MapAreasUI); this file holds data only and runs on server and client.
//------------------------------------------------------------------------------------------------
//! Stable ids: appended only. The names (TOWN, GATED, ...) are what the case table spells.
enum LF_MapAreaStyle
{
	TOWN,
	GATED,
	CUSTOM,
	SAFE,
	LAWLESS,
	EVENT
}

//------------------------------------------------------------------------------------------------
//! How one style draws. Tokens are LF_UITheme `map-*` names (the theme rows arrive with M6); a pack
//! restyles the map by overriding them. Alpha is a percent. hatch: none, diagonal, cross.
//! dash: solid, dash, heavy-dash.
class LF_MapAreaLook : Managed
{
	string m_sFillToken;
	int m_iFillAlpha;
	string m_sHatch;
	string m_sLineToken;
	int m_iLineWidth;
	string m_sDash;
	string m_sLabelKey;

	//------------------------------------------------------------------------------------------------
	static LF_MapAreaLook Create(string fillToken, int fillAlpha, string hatch, string lineToken, int lineWidth, string dash, string labelKey)
	{
		LF_MapAreaLook look = new LF_MapAreaLook();
		look.m_sFillToken = fillToken;
		look.m_iFillAlpha = fillAlpha;
		look.m_sHatch = hatch;
		look.m_sLineToken = lineToken;
		look.m_iLineWidth = lineWidth;
		look.m_sDash = dash;
		look.m_sLabelKey = labelKey;
		return look;
	}
}

//------------------------------------------------------------------------------------------------
//! One area the map draws.
class LF_MapArea : Managed
{
	string m_sName;
	LF_MapAreaStyle m_eStyle;
	//! A localization key or plain text; empty means the style's own label.
	string m_sLabel;
	//! World vertices (x, z used), in order, not closed (the last joins the first).
	ref array<vector> m_aOutline = {};
	//! Where the label goes; set by LF_MapAreas.Register and Collect.
	vector m_vCentroid;
	//! True when the area is a view of an LF_Zones zone (the zone owns it; nothing to register).
	bool m_bFromZone;
}

//------------------------------------------------------------------------------------------------
class LF_MapAreas
{
	static const string FEATURE = "MapAreas";

	//! Sides of a circle's polygon (the doc: circles become 32-gons).
	static const int DEFAULT_SEGMENTS = 32;	// lf-fixed: geometry, the 32-gon the design and the case table spell
	static const int MIN_SEGMENTS = 3;	// lf-fixed: geometry, fewer is not a polygon

	//! A corridor bend sharper than this (1 + cos of the turn) takes the next segment's normal, no spike.
	protected static const float MITER_MIN_DENOM = 0.2;	// lf-fixed: geometry, the bend limit the node twin shares

	protected static ref array<ref LF_MapArea> s_aAreas;

	//------------------------------------------------------------------------------------------------
	protected static array<ref LF_MapArea> Areas()
	{
		if (!s_aAreas)
			s_aAreas = new array<ref LF_MapArea>();

		return s_aAreas;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_aAreas = null;
	}

	// Style ---------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Pure: how a style draws. A fresh object each call.
	static LF_MapAreaLook LookOf(LF_MapAreaStyle style)
	{
		if (style == LF_MapAreaStyle.SAFE)
			return LF_MapAreaLook.Create("map-area-safe-fill", 15, "none", "map-area-safe-line", 2, "solid", "#LF-Map_AreaSafe");

		if (style == LF_MapAreaStyle.LAWLESS)
			return LF_MapAreaLook.Create("map-area-lawless-fill", 25, "diagonal", "map-area-lawless-line", 4, "heavy-dash", "#LF-Map_AreaLawless");

		if (style == LF_MapAreaStyle.TOWN)
			return LF_MapAreaLook.Create("map-area-town-fill", 0, "none", "map-area-town-line", 1, "dash", string.Empty);

		if (style == LF_MapAreaStyle.GATED)
			return LF_MapAreaLook.Create("map-area-gated-fill", 20, "cross", "map-area-gated-line", 2, "solid", "#LF-Map_AreaGated");

		if (style == LF_MapAreaStyle.EVENT)
			return LF_MapAreaLook.Create("map-area-event-fill", 20, "none", "map-area-event-line", 2, "dash", string.Empty);

		return LF_MapAreaLook.Create("map-area-custom-fill", 0, "none", "map-area-custom-line", 1, "solid", string.Empty);
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the style a zone rule draws with. False for NORMAL: the countryside and the policed band
	//! need no fill.
	static bool StyleForRule(LF_ZoneRule rule, out LF_MapAreaStyle style)
	{
		if (rule == LF_ZoneRule.SAFE)
		{
			style = LF_MapAreaStyle.SAFE;
			return true;
		}

		if (rule == LF_ZoneRule.LAWLESS)
		{
			style = LF_MapAreaStyle.LAWLESS;
			return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! The label key or text an area shows: its own when set, else the style's.
	static string LabelKeyOf(notnull LF_MapArea area)
	{
		if (!area.m_sLabel.IsEmpty())
			return area.m_sLabel;

		return LookOf(area.m_eStyle).m_sLabelKey;
	}

	// Outlines (pure) -----------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! A polygon's own points. False (and an empty outline) with fewer than three.
	static bool PolygonOutline(notnull array<vector> points, notnull array<vector> outline)
	{
		outline.Clear();
		if (points.Count() < 3)
			return false;

		foreach (vector point : points)
			outline.Insert(Vector(point[0], 0, point[2]));

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A circle as `segments` points, counter-clockwise from +x.
	static bool CircleOutline(vector center, float radius, int segments, notnull array<vector> outline)
	{
		outline.Clear();
		if (radius <= 0)
			return false;

		int count = Math.Max(segments, MIN_SEGMENTS);
		for (int i = 0; i < count; i++)
		{
			float angle = Math.PI2 * i / count;
			outline.Insert(Vector(center[0] + radius * Math.Cos(angle), 0, center[2] + radius * Math.Sin(angle)));
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A box turned by yaw degrees the way LF_ZoneArea.Contains turns it (world = rotate(local, yaw)).
	//! Corners run (-hx,-hz), (hx,-hz), (hx,hz), (-hx,hz). half uses x and z.
	static bool BoxOutline(vector center, vector half, float yawDegrees, notnull array<vector> outline)
	{
		outline.Clear();
		if (half[0] <= 0 || half[2] <= 0)
			return false;

		float c = Math.Cos(yawDegrees * Math.DEG2RAD);
		float s = Math.Sin(yawDegrees * Math.DEG2RAD);
		for (int i = 0; i < 4; i++)
		{
			float lx = half[0];
			float lz = half[2];
			if (i == 0 || i == 3)
				lx = -half[0];
			if (i < 2)
				lz = -half[2];

			outline.Insert(Vector(center[0] + lx * c - lz * s, 0, center[2] + lx * s + lz * c));
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A polyline corridor (LF_ZoneShape.POLYLINE): left side forward, right side back, mitred bends,
	//! flat caps. One point is a circle. False with no points or no half-width.
	static bool CorridorOutline(notnull array<vector> points, float halfWidth, int segments, notnull array<vector> outline)
	{
		outline.Clear();
		if (halfWidth <= 0 || points.IsEmpty())
			return false;

		if (points.Count() == 1)
			return CircleOutline(points[0], halfWidth, segments, outline);

		// One unit normal (x, z) per segment; a zero-length segment keeps the last good one.
		array<vector> normals = {};
		for (int i = 0; i < points.Count() - 1; i++)
		{
			float dx = points[i + 1][0] - points[i][0];
			float dz = points[i + 1][2] - points[i][2];
			float length = Math.Sqrt(dx * dx + dz * dz);
			if (length > 0)
				normals.Insert(Vector(-dz / length, 0, dx / length));
			else if (!normals.IsEmpty())
				normals.Insert(normals[normals.Count() - 1]);
			else
				normals.Insert("0 0 1");
		}

		array<vector> left = {};
		array<vector> right = {};
		for (int p = 0; p < points.Count(); p++)
		{
			vector offset;
			if (p == 0)
			{
				offset = normals[0] * halfWidth;
			}
			else if (p == points.Count() - 1)
			{
				offset = normals[p - 1] * halfWidth;
			}
			else
			{
				vector before = normals[p - 1];
				vector after = normals[p];
				float denom = 1 + before[0] * after[0] + before[2] * after[2];
				if (denom < MITER_MIN_DENOM)
					offset = after * halfWidth;
				else
					offset = (before + after) * (halfWidth / denom);
			}

			left.Insert(Vector(points[p][0] + offset[0], 0, points[p][2] + offset[2]));
			right.InsertAt(Vector(points[p][0] - offset[0], 0, points[p][2] - offset[2]), 0);
		}

		outline.InsertAll(left);
		outline.InsertAll(right);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The outline of a zone, whatever its shape. False when the zone's own numbers make none.
	static bool OutlineOfZone(notnull LF_ZoneArea zone, notnull array<vector> outline)
	{
		if (zone.m_eShape == LF_ZoneShape.CIRCLE)
			return CircleOutline(zone.m_vCenter, zone.m_fRadius, DEFAULT_SEGMENTS, outline);

		if (zone.m_eShape == LF_ZoneShape.BOX)
			return BoxOutline(zone.m_vCenter, zone.m_vHalfExtents, zone.m_fYaw, outline);

		return CorridorOutline(zone.m_aPoints, zone.m_fRadius, DEFAULT_SEGMENTS, outline);
	}

	//------------------------------------------------------------------------------------------------
	//! Area centroid of a polygon (shoelace), where a label sits. Worked from the first point: 32-bit
	//! floats lose the difference of two products of coordinates in the thousands. A polygon with no
	//! area falls back to the mean of its points.
	static vector Centroid(notnull array<vector> points)
	{
		if (points.IsEmpty())
			return vector.Zero;

		float ox = points[0][0];
		float oz = points[0][2];
		float area2;
		float cx;
		float cz;
		float sumX;
		float sumZ;
		for (int i = 0; i < points.Count(); i++)
		{
			vector next = points[(i + 1) % points.Count()];
			float ax = points[i][0] - ox;
			float az = points[i][2] - oz;
			float bx = next[0] - ox;
			float bz = next[2] - oz;
			float cross = ax * bz - bx * az;
			area2 += cross;
			cx += (ax + bx) * cross;
			cz += (az + bz) * cross;
			sumX += points[i][0];
			sumZ += points[i][2];
		}

		if (Math.AbsFloat(area2) < 0.001)
			return Vector(sumX / points.Count(), 0, sumZ / points.Count());

		return Vector(ox + cx / (3 * area2), 0, oz + cz / (3 * area2));
	}

	// The merged list -----------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Adds or replaces a map-only area by name. An empty name or fewer than three vertices is
	//! refused and logged. Fills in the centroid.
	static bool Register(LF_MapArea area)
	{
		if (!area || area.m_sName.IsEmpty())
		{
			LF_Debug.Error(FEATURE, "map area refused: empty name");
			return false;
		}

		if (area.m_aOutline.Count() < 3)
		{
			LF_Debug.Error(FEATURE, string.Format("map area %1 refused: %2 vertices, need 3", area.m_sName, area.m_aOutline.Count()));
			return false;
		}

		area.m_vCentroid = Centroid(area.m_aOutline);
		LF_MapArea old = Find(area.m_sName);
		if (old)
			Areas().RemoveItem(old);

		Areas().Insert(area);
		LF_Debug.Info(FEATURE, string.Format("map area %1 is %2: %3 vertices", area.m_sName, typename.EnumToString(LF_MapAreaStyle, area.m_eStyle), area.m_aOutline.Count()));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static void Unregister(string name)
	{
		LF_MapArea area = Find(name);
		if (!area)
			return;

		Areas().RemoveItem(area);
		LF_Debug.Info(FEATURE, string.Format("map area %1 removed", name));
	}

	//------------------------------------------------------------------------------------------------
	//! A registered map-only area by name (not a zone), or null.
	static LF_MapArea Find(string name)
	{
		foreach (LF_MapArea area : Areas())
		{
			if (area.m_sName == name)
				return area;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Every area the map draws: the zones LF_Zones holds that have a style (safe, lawless), then the
	//! map-only areas, then the rows of Configs/Map/LF_MapAreas.conf and its pack overlays (a name already
	//! in the list is not added twice), each as a fresh LF_MapArea built now, so it never disagrees with LF_Zones. A zone
	//! whose numbers make no outline is skipped and logged. The caller keeps the result until the next
	//! pan or zoom end (the map re-reads then), not per frame.
	static void Collect(notnull array<ref LF_MapArea> into)
	{
		into.Clear();
		foreach (LF_ZoneArea zone : LF_Zones.GetAll())
		{
			LF_MapAreaStyle style;
			if (!StyleForRule(zone.m_eRule, style))
				continue;

			LF_MapArea area = new LF_MapArea();
			area.m_sName = zone.m_sName;
			area.m_eStyle = style;
			area.m_bFromZone = true;
			if (!OutlineOfZone(zone, area.m_aOutline))
			{
				LF_Debug.Warn(FEATURE, string.Format("zone %1 makes no outline, not on the map", zone.m_sName));
				continue;
			}

			area.m_vCentroid = Centroid(area.m_aOutline);
			into.Insert(area);
		}

		foreach (LF_MapArea own : Areas())
			into.Insert(own);

		// Last: a zone or a layer entity of the same name already in the list wins over the conf row.
		AppendConfigRows(LF_MapAreaConfig.Get(), into);
	}

	//------------------------------------------------------------------------------------------------
	//! One conf row as an area: its outline, style and label. False (nothing made) when the row's own
	//! numbers make no outline (a polygon under three points, a circle with no radius).
	static bool AreaFromRow(notnull LF_MapAreaRow row, notnull LF_MapArea area)
	{
		area.m_sName = row.m_sId;
		area.m_eStyle = row.m_eStyle;
		area.m_sLabel = row.m_sLabel;
		area.m_aOutline.Clear();
		bool made;
		if (row.m_eShape == LF_MapAreaShape.CIRCLE)
		{
			made = CircleOutline(row.m_vCentre, row.m_fRadius, DEFAULT_SEGMENTS, area.m_aOutline);
		}
		else
		{
			array<vector> points = {};
			if (row.m_aPoints)
				points.Copy(row.m_aPoints);

			made = PolygonOutline(points, area.m_aOutline);
		}

		if (made)
			area.m_vCentroid = Centroid(area.m_aOutline);

		return made;
	}

	//------------------------------------------------------------------------------------------------
	//! Adds the conf rows of `config` to `into` (null config adds none). A row with no outline is
	//! skipped and logged. A row whose name is already in `into` is skipped, so a zone entity (safe,
	//! lawless) of the same name stays the one drawn: the rule owns the picture. \return Rows added.
	static int AppendConfigRows(LF_MapAreaConfig config, notnull array<ref LF_MapArea> into)
	{
		if (!config || !config.m_aAreas)
			return 0;

		int added;
		foreach (LF_MapAreaRow row : config.m_aAreas)
		{
			if (!row || row.m_sId.IsEmpty())
				continue;

			bool taken;
			foreach (LF_MapArea have : into)
			{
				if (have.m_sName == row.m_sId)
					taken = true;
			}

			if (taken)
				continue;

			LF_MapArea area = new LF_MapArea();
			if (!AreaFromRow(row, area))
			{
				LF_Debug.Warn(FEATURE, string.Format("area row %1 makes no outline, not on the map", row.m_sId));
				continue;
			}

			into.Insert(area);
			added++;
		}

		return added;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_MapAreas' statics.
class LF_MapAreas_StaticResetHook : LF_StaticResetHook
{
	protected static bool s_bRegistered = LF_StaticReset.Register(new LF_MapAreas_StaticResetHook());

	override string Owner()
	{
		return "LF_MapAreas";
	}

	override void Apply()
	{
		LF_MapAreas.ResetStatics();
	}
}
