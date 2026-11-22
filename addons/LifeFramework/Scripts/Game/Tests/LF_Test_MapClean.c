// red-proof: 2026-11-22, cli test --no-build --red LF_Test_MapClean: RED PROOF OK (RedProof() skips the descriptor rules, the help lines, the key lines, the key collapse and the toast pause, so those checks fail). Green without it.

//------------------------------------------------------------------------------------------------
//! map/clean (map clean-up 2026-11-22, "make sure the map is super clean and easy to use, with instructions"):
//! the vanilla symbols the open map hides and the zooms the town names wait for; the help card's lines for
//! each device, its fold and the saved choice; the key column's groups, columns and one-type filter; a
//! notification's clock paused while the map hides it. Pure script: no map, no widgets, no files.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_MapClean : LF_Test
{
	override string GetName()
	{
		return "map/clean";
	}

	override void Run(LF_TestContext ctx)
	{
		RunDescriptors(ctx);
		RunHelp(ctx);
		LF_MapLegend.ResetStatics();
		RunKey(ctx);
		LF_MapLegend.ResetStatics();
		RunToastPause(ctx);
		RunZoneOverlap(ctx);
	}

	//------------------------------------------------------------------------------------------------
	//! A drawn-only safe row overlapping a real safe zone is not drawn: the map showed two circles on the square.
	protected void RunZoneOverlap(LF_TestContext ctx)
	{
		array<ref LF_MapArea> areas = {};
		LF_MapArea zone = new LF_MapArea();
		zone.m_sName = "square-zone";
		zone.m_eStyle = LF_MapAreaStyle.SAFE;
		zone.m_bFromZone = true;
		LF_MapAreas.CircleOutline("0 0 0", 52, LF_MapAreas.DEFAULT_SEGMENTS, zone.m_aOutline);
		zone.m_vCentroid = LF_MapAreas.Centroid(zone.m_aOutline);
		areas.Insert(zone);

		LF_MapAreaConfig config = new LF_MapAreaConfig();
		config.m_aAreas = {};
		config.m_aAreas.Insert(Row("stale-square", "-18 0 -23", 40));
		config.m_aAreas.Insert(Row("far-field", "400 0 0", 30));
		int added;
		if (!RedProof())
			added = LF_MapAreas.AppendConfigRows(config, areas);

		ctx.Equal(1, added, "only the row clear of the real zone is added");
		bool stale;
		bool far;
		foreach (LF_MapArea area : areas)
		{
			if (area.m_sName == "stale-square")
				stale = true;
			if (area.m_sName == "far-field")
				far = true;
		}

		ctx.False(stale, "the stale circle over the square's real zone is not drawn");
		ctx.True(far, "a safe area away from every zone is drawn");
	}

	//------------------------------------------------------------------------------------------------
	protected LF_MapAreaRow Row(string id, vector centre, float radius)
	{
		LF_MapAreaRow row = new LF_MapAreaRow();
		row.m_sId = id;
		row.m_eStyle = LF_MapAreaStyle.SAFE;
		row.m_eShape = LF_MapAreaShape.CIRCLE;
		row.m_vCentre = centre;
		row.m_fRadius = radius;
		return row;
	}

	//------------------------------------------------------------------------------------------------
	protected void RunDescriptors(LF_TestContext ctx)
	{
		bool fuel;
		bool cross;
		bool townClose;
		if (!RedProof())
		{
			fuel = LF_MapDescriptors.HiddenAt(EMapDescriptorType.MDT_FUELSTATION, 3);
			cross = LF_MapDescriptors.HiddenAt(EMapDescriptorType.MDT_CROSS, 0);
			townClose = LF_MapDescriptors.HiddenAt(EMapDescriptorType.MDT_NAME_TOWN, 0);
		}

		ctx.True(fuel, "the vanilla fuel pump is hidden at every zoom: the Fuel badge says it");
		ctx.True(cross, "the black wayside crosses are hidden");
		ctx.True(townClose, "the town name is hidden at the close zoom, where it sat on the badges");
		ctx.True(LF_MapDescriptors.HiddenAt(EMapDescriptorType.MDT_NAME_CITY, 1), "and one zoom band out");
		ctx.False(LF_MapDescriptors.HiddenAt(EMapDescriptorType.MDT_NAME_TOWN, LF_MapDescriptors.NAMES_FROM_LAYER), "the town name shows at the wide zoom");
		ctx.False(LF_MapDescriptors.HiddenAt(EMapDescriptorType.MDT_NAME_HILL, 0), "hill names are terrain and stay");
		ctx.False(LF_MapDescriptors.HiddenAt(EMapDescriptorType.MDT_LIGHTHOUSE, 0), "a lighthouse is a landmark and stays");
		ctx.True(LF_MapDescriptors.HiddenAt(EMapDescriptorType.MDT_CHURCH, 0), "in town the Church badge stands for the church");
		ctx.False(LF_MapDescriptors.HiddenAt(EMapDescriptorType.MDT_CHURCH, 3), "far out the church symbol is a landmark");
		ctx.False(LF_MapDescriptors.HiddenAt(EMapDescriptorType.MDT_UNIT, 0), "game symbols (units) are left alone");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunHelp(LF_TestContext ctx)
	{
		array<string> keys = {};
		int mouse;
		if (!RedProof())
			mouse = LF_MapHelp.LineKeys(false, false, keys);

		ctx.Equal(5, mouse, "five lines: set, clear, click a place, hover, key");
		if (keys.Count() == 5)
		{
			ctx.EqualStr("#LF-MapHelp_Set", keys[0], "the first line says how to set a waypoint (Shift + Left click)");
			ctx.EqualStr("#LF-MapHelp_Clear", keys[1], "then how to clear it");
			ctx.EqualStr("#LF-MapHelp_Legend", keys[4], "the last points at the key");
		}

		LF_MapHelp.LineKeys(true, false, keys);
		ctx.EqualStr("#LF-MapHelp_SetPad", keys[0], "on a gamepad the lines name the buttons");
		ctx.Equal(0, LF_MapHelp.LineKeys(false, true, keys), "folded: no lines");
		ctx.EqualStr("#LF-MapHelp_Folded", LF_MapHelp.TitleKey(false, true), "folded: one line that says how to unfold");
		ctx.EqualStr("#LF-MapHelp_TitlePad", LF_MapHelp.TitleKey(true, false), "the title names the gamepad fold button");
		ctx.True(LF_MapHelp.CardHeight(0) < LF_MapHelp.CardHeight(5), "the folded card is shorter");
		ctx.EqualStr("#LF-MapHelp_NoWaypoint", LF_MapHelp.WaypointKey(false), "no waypoint: the row says so");
		ctx.EqualStr("#LF-MapHelp_Waypoint", LF_MapHelp.WaypointKey(true), "a waypoint: the row names it");
		ctx.True(LF_MapHelp.ParseFolded(LF_MapHelp.PrefText(true)), "the saved fold reads back folded");
		ctx.False(LF_MapHelp.ParseFolded(LF_MapHelp.PrefText(false)), "and unfolded");
		ctx.False(LF_MapHelp.ParseFolded(""), "no saved choice: a new player sees the card open");
		ctx.True(LF_MapHelp.ParseFolded(" Folded \n"), "case and spaces do not matter");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunKey(LF_TestContext ctx)
	{
		array<ref LF_MapPoi> pois = {};
		pois.Insert(Poi("StoreCounter", "poi_Bakery", "shop", "Bakery"));
		pois.Insert(Poi("Payphone", "poi_Payphone", "civic", "Payphone"));
		pois.Insert(Poi("Payphone", "poi_Payphone", "civic", "Payphone"));
		pois.Insert(Poi("StoreCounter", "poi_Grocer", "shop", "Supermarket"));
		pois.Insert(Poi("FuelTill", "poi_Fuel", "travel", "Filling station"));
		array<ref LF_MapKeyLine> lines = {};
		int count;
		if (!RedProof())
			count = LF_MapLegend.KeyLines(pois, lines);

		ctx.Equal(9, count, "three group headers, four type rows (the second payphone adds none) and the two sign lines");
		if (lines.Count() == 9)
		{
			ctx.EqualStr("#LF-MapKey_Alarm", lines[7].m_sNote, "the alarm ring has its key line");
			ctx.EqualStr("#LF-MapKey_Run", lines[8].m_sNote, "and so does the job-run chain");
			ctx.True(lines[0].m_bHeader && lines[0].m_sCategory == "civic", "civic leads, under its header");
			ctx.EqualStr("civic", lines[1].m_sCategory, "the payphone row sits under it");
			ctx.True(lines[2].m_bHeader && lines[2].m_sCategory == "shop", "then the shops' header");
			ctx.Equal(0, lines[3].m_iPoi, "the bakery row points at a bakery to draw its badge");
			ctx.True(lines[5].m_bHeader && lines[5].m_sCategory == "travel", "travel comes last here");
		}

		ctx.True(LF_MapLegend.CategoryColor("shop") != LF_MapLegend.CategoryColor("law"), "each group header has its own colour");
		ctx.Equal(1, LF_MapKeyLine.Columns(LF_MapKeyLine.MAX_PER_COLUMN), "a full column is one column");
		ctx.Equal(2, LF_MapKeyLine.Columns(LF_MapKeyLine.MAX_PER_COLUMN + 1), "one more line starts a second column");
		int over = LF_MapKeyLine.MAX_PER_COLUMN + 2;
		ctx.Equal(over / 2, LF_MapKeyLine.PerColumn(over), "a column and two lines split evenly over two columns");

		// Three lines to a column: a header that would end the first column opens the second instead.
		array<ref LF_MapKeyLine> flow = {};
		flow.Insert(LF_MapKeyLine.Header("civic"));
		flow.Insert(LF_MapKeyLine.Row("civic", "a", 0));
		flow.Insert(LF_MapKeyLine.Header("shop"));
		flow.Insert(LF_MapKeyLine.Row("shop", "b", 1));
		array<int> cols = {};
		array<int> rows = {};
		ctx.Equal(2, LF_MapKeyLine.Place(flow, 3, cols, rows), "two columns");
		if (cols.Count() == 4)
		{
			ctx.Equal(0, cols[0], "the first lines read down the left column");
			ctx.True(cols[2] == 1 && rows[2] == 0, "the shops' header moves to the top of the next column");
			ctx.True(cols[3] == 1 && rows[3] == 1, "with its row under it");
		}

		// Groups start collapsed (first-2-hours review C3): a header alone each, the sign lines stay; a group the
		// player opens shows its rows.
		array<string> open = {};
		int collapsed;
		if (!RedProof())
			collapsed = LF_MapKeyLine.Collapse(lines, open);
		else
			collapsed = lines.Count();

		ctx.Equal(5, collapsed, "three headers and the two sign lines are left when every group is shut");
		open.Clear();
		lines.Clear();
		LF_MapLegend.KeyLines(pois, lines);
		ctx.True(LF_MapKeyLine.ToggleOpen(open, "shop"), "a click on the shops' header opens it");
		ctx.Equal(7, LF_MapKeyLine.Collapse(lines, open), "the shops' two rows join the five lines");
		ctx.False(LF_MapKeyLine.ToggleOpen(open, "shop"), "a second click shuts it again");

		string bakery = LF_MapLegend.TypeKey(pois[0]);
		LF_MapLegend.ToggleSolo(bakery);
		array<ref LF_MapPoi> drawn = {};
		ctx.Equal(1, LF_MapLegend.Filter(pois, LF_MapLegend.Hidden(), drawn), "a click on the Bakery row draws only bakeries");
		LF_MapLegend.ToggleSolo(bakery);
		drawn.Clear();
		ctx.Equal(5, LF_MapLegend.Filter(pois, LF_MapLegend.Hidden(), drawn), "the same row again draws every place");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunToastPause(LF_TestContext ctx)
	{
		int resumed = 1000;
		if (!RedProof())
			resumed = LF_NotificationToastUI.ResumedShownAt(1000, 5000, 9000);

		ctx.Equal(5000, resumed, "four seconds behind the map add four seconds to the note's time on screen");
		ctx.Equal(1000, LF_NotificationToastUI.ResumedShownAt(1000, -1, 9000), "a note the map never hid keeps its time");
	}

	//------------------------------------------------------------------------------------------------
	protected LF_MapPoi Poi(string kind, string icon, string category, string label)
	{
		LF_MapPoi poi = new LF_MapPoi();
		poi.m_sKind = kind;
		poi.m_sId = kind + "." + label;
		poi.m_sIcon = icon;
		poi.m_sCategory = category;
		poi.m_sLabel = label;
		poi.m_eState = LF_EMapPoiState.FULL;
		return poi;
	}
}
