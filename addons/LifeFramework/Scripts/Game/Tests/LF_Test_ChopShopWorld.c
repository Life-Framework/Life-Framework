// red-proof: 2026-11-10 `tools/cli test --no-build --only LF_Test_ChopShopWorld` with the action count expectation set to 4 failed `- load, work and collect are on the bench (expected 4, got 3)`. Restored; green.
// tier: WORLD
//! The playtest chop shop yard is wired (#415): the bench is placed with its component and its three
//! actions (load, work, collect), starts empty, the config and the parts prefab resolve, and a bench
//! with no car in the drop area refuses. Spawns nothing. The steal-and-chop loop is the
//! two-client session proof.
class LF_Test_ChopShopWorld : LF_Test
{
	override string GetName()
	{
		return "e2e/chopshop-yard";
	}

	override void Run(LF_TestContext ctx)
	{
		IEntity bench = GetGame().GetWorld().FindEntityByName("Roadmap_ChopShop");
		ctx.True(bench != null, "Roadmap_ChopShop is placed");
		if (!bench)
			return;

		LF_ChopShopComponent shop = LF_Component<LF_ChopShopComponent>.Find(bench);
		ctx.NotNull(shop, "the bench carries LF_ChopShopComponent");
		if (!shop)
			return;

		ctx.EqualStr("roadmap-chop", shop.GetShopId(), "the yard has its id");
		ctx.Equal(LF_StationProgress.EMPTY, shop.GetPercent(), "the bench starts empty");

		ActionsManagerComponent actions = LF_Component<ActionsManagerComponent>.Find(bench);
		ctx.NotNull(actions, "the bench carries ActionsManagerComponent");
		if (actions)
		{
			array<BaseUserAction> actionList = {};
			actions.GetActionsList(actionList);
			int chopActions;
			foreach (BaseUserAction action : actionList)
			{
				if (LF_ChopAction.Cast(action))
					chopActions++;
			}

			ctx.Equal(3, chopActions, "load, work and collect are on the bench");
		}

		Resource parts = Resource.Load(LF_ChopShopConfig.PARTS_PREFAB);
		ctx.True(parts && parts.IsValid(), "the parts prefab resolves");
		ctx.NotNull(LF_ChopShopConfig.Get(), "the chop shop config resolves");

		// Nothing parked in the drop area: the load is refused and the bench stays empty.
		ctx.Equal(LF_EChopCheck.TOO_FAR, shop.ServerLoad(null, 1000), "a load with nobody at the bench is refused");
		ctx.Equal(LF_StationProgress.EMPTY, shop.GetPercent(), "a refused load leaves the bench empty");
	}
}
