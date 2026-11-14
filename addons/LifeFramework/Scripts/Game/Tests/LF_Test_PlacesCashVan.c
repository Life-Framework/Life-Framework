// red-proof: 2026-11-14, with LF_CashVanRoute.FindVan back on FindEntityByName(routeId),
//   cli test --no-build --only LF_Test_PlacesCashVan:
//   [LFTEST]   - the route finds its van by component
// Restored; green.
// tier: WORLD
//! places/cash-van: the cash van route finds its van and its stops by component, never by entity
//! name (#1045). An unnamed van and three unnamed stops, placed under a route id, are found; the
//! stops come back in their order whatever order they were placed in; a gap ends the route; a
//! deleted van is not found. Under the old FindEntityByName lookup an unnamed van was never found.
class LF_Test_PlacesCashVan : LF_Test
{
	protected static const string ROUTE_ID = "places-cash-van-route";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "places/cash-van";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		IEntity anchor = LF_Places.Find(LF_Bank.PLACE_KIND, LF_TestAnnex.BANK_VAULT);
		ctx.NotNull(anchor, "the annex vault is placed, to spawn the stand-ins near");
		if (!anchor)
			return;

		vector at = anchor.GetOrigin() + "0 -20 0";
		array<IEntity> spawned = {};
		for (int i = 0; i < 4; i++)
			spawned.Insert(LF_Utils.SpawnEntityPrefab(LF_Bank.BAG_PREFAB, at + Vector(3 * i, 0, 0), "0 0 0", false));

		IEntity van = spawned[0];
		IEntity stopA = spawned[1];
		IEntity stopB = spawned[2];
		IEntity stopC = spawned[3];
		ctx.True(van && stopA && stopB && stopC, "a van and three stops spawn");
		if (!van || !stopA || !stopB || !stopC)
		{
			foreach (IEntity entity : spawned)
				SCR_EntityHelper.DeleteEntityAndChildren(entity);

			return;
		}

		ctx.EqualStr("", van.GetName(), "the van has no name");
		ctx.True(LF_CashVanRoute.FindVan(ROUTE_ID) == null, "an unplaced route has no van");

		array<IEntity> stops = {};
		ctx.Equal(0, LF_CashVanRoute.FindStops(ROUTE_ID, stops), "an unplaced route has no stops");

		ctx.True(LF_Places.Register(LF_CashVanRoute.PLACE_KIND_VAN, ROUTE_ID, van), "the unnamed van is placed under the route id");
		ctx.True(LF_Places.Register(LF_CashVanRoute.PLACE_KIND_STOP, LF_Places.OrderedId(ROUTE_ID, 2), stopC), "stop 2 is placed first");
		ctx.True(LF_Places.Register(LF_CashVanRoute.PLACE_KIND_STOP, LF_Places.OrderedId(ROUTE_ID, 0), stopA), "stop 0 is placed");
		ctx.True(LF_CashVanRoute.FindVan(ROUTE_ID) == van, "the route finds its van by component");

		ctx.Equal(1, LF_CashVanRoute.FindStops(ROUTE_ID, stops), "a gap at stop 1 ends the route after stop 0");
		ctx.True(LF_Places.Register(LF_CashVanRoute.PLACE_KIND_STOP, LF_Places.OrderedId(ROUTE_ID, 1), stopB), "stop 1 is placed");

		stops.Clear();
		ctx.Equal(3, LF_CashVanRoute.FindStops(ROUTE_ID, stops), "three stops are found");
		if (stops.Count() == 3)
		{
			ctx.True(stops[0] == stopA, "stop 0 comes first");
			ctx.True(stops[1] == stopB, "stop 1 comes second");
			ctx.True(stops[2] == stopC, "stop 2 comes last");
		}

		SCR_EntityHelper.DeleteEntityAndChildren(van);
		ctx.True(LF_CashVanRoute.FindVan(ROUTE_ID) == null, "a deleted van is not found");

		SCR_EntityHelper.DeleteEntityAndChildren(stopA);
		SCR_EntityHelper.DeleteEntityAndChildren(stopB);
		SCR_EntityHelper.DeleteEntityAndChildren(stopC);
	}
}
