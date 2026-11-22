// red-proof: 2026-11-22, cli test --no-build --red LF_Test_MapDeclutter,LF_Test_MapWaypointClick: RED PROOF OK (RedProof() skips the merge, the hit test and the legend lines, so those checks fail). Green without it.

//------------------------------------------------------------------------------------------------
//! map/declutter (0.2.0 playtest map rework): the paper-map rules for place icons. The icon size follows the
//! zoom; icons that would overlap on the screen are one mark led by the most important place; the pointer
//! finds the mark under it (mouse or gamepad cursor); the legend lists one line per icon and type.
//! Pure script: positions are screen units, no map and no widgets.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_MapDeclutter : LF_Test
{
	override string GetName()
	{
		return "map/declutter";
	}

	override void Run(LF_TestContext ctx)
	{
		RunSize(ctx);
		RunOrder(ctx);
		RunMerge(ctx);
		RunHit(ctx);
		RunLegendRows(ctx);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunSize(LF_TestContext ctx)
	{
		float solo = LF_MapClusterRules.SOLO_ZOOM;
		ctx.EqualFloat(LF_MapDeclutter.ICON_MIN, LF_MapDeclutter.IconSize(solo, false), 0.01, "at the solo zoom an icon is the smallest size");
		ctx.True(LF_MapDeclutter.IconSize(solo + 2, false) > LF_MapDeclutter.IconSize(solo, false), "closer in, an icon grows");
		ctx.EqualFloat(LF_MapDeclutter.ICON_MAX, LF_MapDeclutter.IconSize(solo + 20, false), 0.01, "it never grows past the largest size");
		ctx.EqualFloat(LF_MapDeclutter.ICON_MIN, LF_MapDeclutter.IconSize(0, false), 0.01, "far out it never shrinks below the smallest");
		ctx.EqualFloat(LF_MapDeclutter.STAMP_SIZE, LF_MapDeclutter.IconSize(2, true), 0.01, "a cluster stamp has its own size");
		ctx.True(LF_MapDeclutter.IconSize(solo + 20, false) < 56, "no icon is drawn at its 64 px image size");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunOrder(LF_TestContext ctx)
	{
		int civic = LF_MapDeclutter.Priority("civic", LF_EMapPoiState.FULL, false);
		int shop = LF_MapDeclutter.Priority("shop", LF_EMapPoiState.FULL, false);
		int faded = LF_MapDeclutter.Priority("civic", LF_EMapPoiState.FADED, false);
		ctx.True(LF_MapDeclutter.Priority("", LF_EMapPoiState.FULL, true) < civic, "a stamp comes before any place");
		ctx.True(civic < shop, "the legend's category order decides between places");
		ctx.True(shop < faded, "a place only heard of comes after every visited one");

		array<int> priorities = {5, 1, 5, 0};
		array<int> order = {};
		LF_MapDeclutter.Order(priorities, order);
		ctx.Equal(4, order.Count(), "every index is ordered");
		ctx.Equal(3, order[0], "lowest priority first");
		ctx.Equal(1, order[1], "then the next");
		ctx.Equal(0, order[2], "equal priorities keep their input order (first)");
		ctx.Equal(2, order[3], "equal priorities keep their input order (second)");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunMerge(LF_TestContext ctx)
	{
		// 0 and 1 overlap (the bank and its ATM), 2 is clear of both, 3 sits on 2.
		array<float> xs = {100, 108, 300, 305};
		array<float> ys = {100, 104, 100, 96};
		array<float> sizes = {24, 24, 24, 24};
		array<int> order = {1, 0, 2, 3};	// the ATM is the more important here: it leads
		array<int> leadOf = {};
		if (!RedProof())
			LF_MapDeclutter.Merge(xs, ys, sizes, order, leadOf);

		ctx.Equal(4, leadOf.Count(), "every icon has a lead");
		if (leadOf.Count() < 4)
			return;

		ctx.Equal(1, leadOf[1], "the first in order leads its own mark");
		ctx.Equal(1, leadOf[0], "an overlapping icon joins the mark before it");
		ctx.Equal(2, leadOf[2], "an icon clear of the others is its own mark");
		ctx.Equal(2, leadOf[3], "a third icon joins the mark it overlaps");

		// Side by side, just touching: still two marks.
		array<float> apartX = {0, 24};
		array<float> apartY = {0, 0};
		array<float> apartSize = {24, 24};
		array<int> apartOrder = {0, 1};
		array<int> apartLead = {};
		LF_MapDeclutter.Merge(apartX, apartY, apartSize, apartOrder, apartLead);
		ctx.Equal(1, apartLead[1], "two icons that only touch stay two marks");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunHit(LF_TestContext ctx)
	{
		array<float> xs = {100, 300};
		array<float> ys = {100, 100};
		array<float> sizes = {24, 24};
		array<bool> shown = {true, true};
		int hit = -1;
		if (!RedProof())
			hit = LF_MapDeclutter.Hit(105, 96, xs, ys, sizes, shown);

		ctx.Equal(0, hit, "the pointer over an icon finds it");
		ctx.Equal(1, LF_MapDeclutter.Hit(300 + 12 + LF_MapDeclutter.HIT_PAD - 1, 100, xs, ys, sizes, shown), "just outside the drawn edge still hits (the gamepad cursor)");
		ctx.Equal(-1, LF_MapDeclutter.Hit(200, 100, xs, ys, sizes, shown), "between icons is no hit");
		shown[0] = false;
		ctx.Equal(-1, LF_MapDeclutter.Hit(100, 100, xs, ys, sizes, shown), "an icon off the screen is never hit");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunLegendRows(LF_TestContext ctx)
	{
		array<ref LF_MapPoi> pois = {};
		pois.Insert(Poi("StoreCounter", "poi_Bakery", "shop", "Bakery", LF_EMapPoiState.FULL));
		pois.Insert(Poi("Payphone", "poi_Payphone", "civic", "Payphone", LF_EMapPoiState.FULL));
		pois.Insert(Poi("Payphone", "poi_Payphone", "civic", "Payphone", LF_EMapPoiState.FULL));
		pois.Insert(Poi("StoreCounter", "poi_Grocer", "shop", "Supermarket", LF_EMapPoiState.FULL));
		pois.Insert(Poi("Fence", "", "underworld", "Fence", LF_EMapPoiState.PENCIL));
		pois.Insert(Poi("FuelTill", "poi_Fuel", "travel", "Filling station", LF_EMapPoiState.FULL));
		pois.Insert(Poi("RoadFuel", "poi_Fuel", "travel", "Filling station", LF_EMapPoiState.FULL));
		array<int> rows = {};
		int count;
		if (!RedProof())
			count = LF_MapLegend.IconRows(pois, rows);

		ctx.Equal(4, count, "one legend line per icon and type; the second payphone, the second filling station kind and the pencil mark add none");
		if (rows.Count() < 4)
			return;

		ctx.Equal(1, rows[0], "civic comes before shop in the legend");
		ctx.Equal(0, rows[1], "then the shops, first seen first");
		ctx.Equal(3, rows[2], "a shop with its own icon has its own line");
		ctx.Equal(5, rows[3], "two kinds with the same icon and name are one line");
	}

	//------------------------------------------------------------------------------------------------
	protected LF_MapPoi Poi(string kind, string icon, string category, string label, LF_EMapPoiState state)
	{
		LF_MapPoi poi = new LF_MapPoi();
		poi.m_sKind = kind;
		poi.m_sId = kind + "." + label;
		poi.m_sIcon = icon;
		poi.m_sCategory = category;
		poi.m_sLabel = label;
		poi.m_eState = state;
		return poi;
	}
}
