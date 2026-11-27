// red-proof: before any sign was placed, `tools\cli test --no-build --only LF_Test_SignCoverage`
// (2026-11-08) failed signs/coverage with "catalog sign 'police_whitelist' is placed in DebugWorld".
// 2026-11-14 (#323 batch 1, mining sign moved to the pack world) it failed for 'mining' until IsPackPlaced.

[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_SignCoverage : LF_Test
{
	protected static ref array<string> s_aIds;
	protected static ref array<string> s_aUnusable;

	override string GetName()
	{
		return "signs/coverage";
	}

	//------------------------------------------------------------------------------------------------
	protected static bool CollectSign(IEntity entity)
	{
		if (!entity)
			return true;

		LF_SignComponent sign = LF_SignComponent.Cast(entity.FindComponent(LF_SignComponent));
		if (!sign)
			return true;

		s_aIds.Insert(sign.GetSignId());
		if (!LF_SignCatalog.IsUsable(sign.GetSign()))
			s_aUnusable.Insert(string.Format("'%1' at %2", sign.GetSignId(), entity.GetOrigin()));

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		s_aIds = {};
		s_aUnusable = {};
		BaseWorld world = GetGame().GetWorld();
		ctx.True(world != null, "DebugWorld is loaded");
		if (!world)
			return;

		world.QueryEntitiesBySphere("256 0 256", 1000, CollectSign, null, EQueryEntitiesFlags.ALL); // town-layout-ok: a whole-world sweep, not a position
		ctx.True(s_aIds.Count() > 0, "DebugWorld places signs");

		foreach (string unusable : s_aUnusable)
		{
			ctx.True(false, "placed sign " + unusable + " resolves a titled LF_Signs.conf entry");
		}

		LF_SignConfig config = LF_SignCatalog.Load(LF_SignCatalog.CONFIG_PATH);
		ctx.NotNull(config, "LF_Signs.conf loads");
		if (!config || !config.m_aSigns)
			return;

		foreach (LF_SignDef def : config.m_aSigns)
		{
			if (!IsPackPlaced(def.m_sId))
				ctx.True(s_aIds.Contains(def.m_sId), string.Format("catalog sign '%1' is placed in DebugWorld", def.m_sId));
			ExpectTranslated(ctx, def.m_sId, "title", def.m_sTitle);
			ExpectTranslated(ctx, def.m_sId, "what", def.m_sWhat);
		}

		s_aIds = null;
		s_aUnusable = null;
	}

	//------------------------------------------------------------------------------------------------
	//! A catalog sign whose only placement is the pack world (Everon1989Town, #323 batch 1), which the
	//! core test mission does not load (docs/architecture/core-pack-inventory.md).
	protected static bool IsPackPlaced(string id)
	{
		// plot_for_sale: the face of the LF_PlotSign prefab (#2780), placed one per plot by a pack or the grid; the core world has no plot.
		// chalk_sample: the chalk style's sample board (docs/architecture/sign-system.md), placed in its GridWorld cell only.
		// price_board: the trader price board (#3299, LF_PriceBoard), placed in its GridWorld cell until a town pass puts it on Saint-Philippe.
		// house_for_sale: the face of the LF_HouseSaleSign prefab (#3207), placed one per house by a pack or the grid.
		// taxi_stand: the plate on the LF_TaxiStand prefab, placed in its GridWorld cell (Grid_taxi_stand.layer) only.
		// bus_timetable: the board on the LF_BusStop prefab, placed with each stop (GridWorld cell bus-stops, the pack island).
		// export_orders: the dock export order board (#4411, LF_ExportOrderBoard), placed in its GridWorld cell until WK-W24_3 puts it on the dock.
		// outpost_stock: the chalkboard of the LF_OutpostBoard prefab (#4452), placed at the island outposts and on the playtest town square (Village_Places.layer, pack world).
		// house_sold, plot_sold: the faces the two sale signs switch to once the house or plot is owned (LF_SignComponent.m_sSoldSignId); no prop names them as its own id.
		// shift_board: the post of the mine shift board (#4180), placed at Coalman's Brow in the pack world only since the town pass moved it out of DebugWorld.
		return id == "shift_board" || id == "house_sold" || id == "plot_sold" || id == "outpost_stock" || id == "export_orders" || id == "mining" || id == "plot_for_sale" || id == "house_for_sale" || id == "chalk_sample" || id == "price_board" || id == "bus_timetable" || id == "taxi_stand";
	}

	//------------------------------------------------------------------------------------------------
	//! A section is set, and a #key resolves to text instead of echoing the key.
	protected void ExpectTranslated(LF_TestContext ctx, string id, string field, string value)
	{
		string bare = value;
		bare.Replace("#", "");
		string text = LF_SignCatalog.Resolve(value);
		bool ok = !value.IsEmpty() && !text.IsEmpty() && text != value && text != bare;
		if (!value.IsEmpty() && !LF_SignCatalog.IsKey(value))
			ok = true;

		// The per-sign text is the pack's (#325): with no pack table the card falls back to the neutral title.
		if (!ok && WidgetManager.Translate("#LF-Sign_Mining_Title") == "#LF-Sign_Mining_Title")
			ok = true;

		ctx.True(ok, string.Format("sign '%1' %2 has text (%3)", id, field, value));
	}
}
