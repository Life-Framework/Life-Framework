// red-proof: 2026-11-18, before the degrade (Step logged on every call):
//   tools\cli test --no-build --only LF_Test_CashVanRouteNoVan:
//   [LFTEST] FAIL crime/cash-van-no-van
//   [LFTEST]   - one error is logged for a route with no van, not one per dwell (expected 1, got 3)
//   Degrade added; green.
//! crime/cash-van-no-van: a cash van route whose van is not placed logs one error, not one per
//! dwell (0grind logged x117 in one session). A van that turns up later restarts nothing.
//! The timer-stop half of #3118 is not covered here (see the PR).
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_CashVanRouteNoVan : LF_Test
{
	protected static const string ROUTE_ID = "cash-van-no-van-route";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "crime/cash-van-no-van";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		IEntity anchor = LF_Places.Find(LF_Bank.PLACE_KIND, LF_TestAnnex.BANK_VAULT);
		ctx.NotNull(anchor, "the annex vault is placed, to spawn the route door near");
		if (!anchor)
			return;

		IEntity door = LF_Utils.SpawnEntityPrefab("{8FABDE609C994BC2}Prefabs/Vehicles/LF_CashVanBox.et", anchor.GetOrigin() + "0 -25 0");
		if (!door)
		{
			ctx.Fail("the cash van box spawns");
			return;
		}

		door.SetName(ROUTE_ID);
		LF_CashVanRouteComponent route = LF_CashVanRouteComponent.Cast(door.FindComponent(LF_CashVanRouteComponent));
		ctx.NotNull(route, "the box carries the cash van route");
		if (route)
		{
			ctx.True(LF_CashVanRoute.FindVan(ROUTE_ID) == null, "no van is placed for the route");

			route.Step();
			route.Step();
			route.Step();

			ctx.Equal(1, route.NoVanErrors(), "one error is logged for a route with no van, not one per dwell");
		}

		SCR_EntityHelper.DeleteEntityAndChildren(door);
	}
}
