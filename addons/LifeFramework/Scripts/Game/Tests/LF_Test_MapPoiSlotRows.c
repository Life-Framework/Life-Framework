// red-proof: 2026-11-18, cli test --no-build --red LF_Test_MapPoiSlotRows: RED-OK (RedProof() leaves the slot rows out of the table, so every shop keeps the base name and "the bakery reads Bakery" fails). Green without it.
//! map/poi-slot-rows (#3515): shops share one front kind (StoreCounter) yet each reads as itself on the
//! map. A pack row keyed "<kind>/<slot>" restyles the places of that kind whose id ends in ".<slot>":
//! label, icon, category and min zoom come from the slot row; whether and to whom the place shows stays
//! the base kind's row, so a shop the player has not heard of stays hidden whatever its slot row says.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_MapPoiSlotRows : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "map/poi-slot-rows";
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_MapPoiKind Row(string kind, string label, string icon, LF_EMapPoiVisibility visibility)
	{
		LF_MapPoiKind row = LF_MapPoiKind.Make(kind, visibility);
		row.m_sCategory = "shop";
		row.m_sLabel = label;
		row.m_sIcon = icon;
		return row;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_MapPoi Find(notnull array<ref LF_MapPoi> pois, string id)
	{
		foreach (LF_MapPoi poi : pois)
		{
			if (poi && poi.m_sId == id)
				return poi;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		ctx.EqualStr("bakery", LF_MapPoiConfig.SlotOf("town.bakery"), "the slot is what follows the last dot");
		ctx.EqualStr("", LF_MapPoiConfig.SlotOf("nodot"), "an id with no dot has no slot");
		ctx.EqualStr("", LF_MapPoiConfig.SlotOf("trailing."), "an id ending in a dot has no slot");

		LF_MapPoiConfig config = new LF_MapPoiConfig();
		config.m_aKinds = new array<ref LF_MapPoiKind>();
		config.m_aKinds.Insert(Row("StoreCounter", "#base", "poi_Shop", LF_EMapPoiVisibility.DISCOVER));
		LF_MapPoiKind bakery = Row("StoreCounter/bakery", "#bakery", "", LF_EMapPoiVisibility.PUBLIC);
		if (!RedProof())
		{
			config.m_aKinds.Insert(bakery);
			config.m_aKinds.Insert(Row("StoreCounter/butcher", "#butcher", "poi_Butcher", LF_EMapPoiVisibility.PUBLIC));
		}

		ctx.True(LF_MapPoiConfig.IsSlotRow(bakery), "a row keyed kind/slot is a slot row");
		ctx.True(!LF_MapPoiConfig.IsSlotRow(config.Find("StoreCounter")), "the base row is not a slot row");
		ctx.True(config.FindSlotRow("StoreCounter", "town.bakery") == bakery, "the bakery place finds its slot row");
		ctx.True(config.FindSlotRow("StoreCounter", "town.hardware") == null, "a slot with no row finds none");
		ctx.True(config.FindSlotRow("StoreCounter", "nodot") == null, "an id with no slot finds none");

		array<ref LF_MapPoiPlace> places = {};
		places.Insert(LF_MapPoiPlace.Make("StoreCounter", "town.bakery", "1 0 1", true));
		places.Insert(LF_MapPoiPlace.Make("StoreCounter", "town.butcher", "2 0 2", true));
		places.Insert(LF_MapPoiPlace.Make("StoreCounter", "town.hardware", "3 0 3", true));

		// A player who has heard of the bakery and the hardware store, not of the butcher.
		LF_MapPoiAudience audience = new LF_MapPoiAudience();
		audience.SetKnown("town.bakery", LF_EMapPoiKnown.HEARD);
		audience.SetKnown("town.hardware", LF_EMapPoiKnown.VISITED);

		array<ref LF_MapPoi> pois = {};
		LF_MapPois.Collect(config, audience, places, pois);

		LF_MapPoi bakeryPoi = Find(pois, "town.bakery");
		ctx.NotNull(bakeryPoi, "the heard-of bakery is listed");
		if (bakeryPoi)
		{
			ctx.EqualStr("#bakery", bakeryPoi.m_sLabel, "the bakery reads Bakery");
			ctx.EqualStr("poi_Shop", bakeryPoi.m_sIcon, "a slot row with no icon keeps the base icon");
			ctx.EqualStr("StoreCounter", bakeryPoi.m_sKind, "the poi keeps the base kind, so icon fallbacks still work");
		}

		LF_MapPoi hardwarePoi = Find(pois, "town.hardware");
		ctx.NotNull(hardwarePoi, "the visited hardware store is listed");
		if (hardwarePoi)
			ctx.EqualStr("#base", hardwarePoi.m_sLabel, "a slot with no row keeps the base name");

		ctx.True(Find(pois, "town.butcher") == null, "an unheard shop stays hidden: the base row's DISCOVER decides, not the slot row's PUBLIC");

		array<ref LF_MapPoi> seen = {};
		LF_MapPoiAudience butcherKnown = new LF_MapPoiAudience();
		butcherKnown.SetKnown("town.butcher", LF_EMapPoiKnown.HEARD);
		LF_MapPois.Collect(config, butcherKnown, places, seen);
		LF_MapPoi butcherPoi = Find(seen, "town.butcher");
		ctx.NotNull(butcherPoi, "the butcher shows once heard of");
		if (butcherPoi)
			ctx.EqualStr("poi_Butcher", butcherPoi.m_sIcon, "a slot row's own icon replaces the base icon");
	}
}
