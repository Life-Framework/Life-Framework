// red-proof: 2026-11-22, `tools\cli test --no-build --red LF_Test_EveronArrivalRoute` (RedProof skips the talk with the clerk): RED PROOF OK, failed "talking to the clerk registers the newcomer", "the pack holds $100 in cash", "the starter car stands on the garage pad"; green without the hook (pack leg worlds=LF_Everon1989_Test, with settling-in, welcome-gift, starting-map, market and clerk-welcome).
//! The island's arrival route (maintainer, 2026-11-22): under the Everon 1989 header (the pack leg of `tools\cli
//! test`) the clerk's row takes the prefab's five steps out and gives its own: register (the ID card, the welcome
//! pack and $100), take the starter car out of the garage, pick nine apples, press juice, sell it at the market.
//! A fixture newcomer walks it through the public quest paths: the pack and the car come once (a second talk and a
//! respawn that starts the garage step again give nothing), each errand waits for the one before, the garage
//! counter's own take-out action and a real sale to the island trader close their steps.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronArrivalRoute : LF_Test
{
	static const ResourceName CLERK_PREFAB = "{DE27E685D6FE93D7}Prefabs/Characters/Quests/QuestGiver_Clerk.et";
	static const ResourceName TRADER = "{3A9F6E21C47D8B71}Prefabs/Trader/LF_UniversalTrader_Everon1989.et";
	static const ResourceName APPLE = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";
	static const ResourceName JUICE = "{1DBD99E6347DE1AD}Prefabs/Items/Drinks/AppleJuice.et";
	static const ResourceName BREAD = "{787AA46A1D92C7F7}Prefabs/Items/Food/LF_Baguette.et";
	static const ResourceName MEAT = "{A2A09084A440CB11}Prefabs/Items/Food/LF_Saucisson.et";
	static const ResourceName WATER = "{A231A5F8D479B5DC}Prefabs/Items/Drinks/WaterBottle.et";
	static const ResourceName T601 = "{6303FEB76E4B719A}Prefabs/Vehicles/Wheeled/T601/T601_beige.et";
	static const int PACK_CASH = 100;
	static const int APPLES = 9;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/arrival-route";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_VehicleGarageComponent garage = NearestGarage("0 0 0");
		ctx.NotNull(garage, "the test world has a vehicle garage");
		if (!garage)
			return;

		// The world's own clerk when one is placed (a second giver with his id is not reachable), else a
		// fixture 6 m from the garage counter. The newcomer stands beside him (GIVER_RADIUS 5).
		IEntity clerk;
		IEntity fixture;
		LF_QuestGiverComponent placed = LF_QuestManager.GetInstance().GetGiver("clerk");
		if (placed)
			clerk = placed.GetOwner();

		if (!clerk)
		{
			fixture = LF_Utils.SpawnEntityPrefab(CLERK_PREFAB, garage.GetOwner().GetOrigin() + Vector(6, 0, 0));
			clerk = fixture;
		}

		ctx.NotNull(clerk, "a clerk stands in the test world");
		if (!clerk)
			return;

		CheckRoute(ctx);
		if (ctx.FailureCount() == 0)
			Walk(ctx, clerk, garage);

		if (fixture)
			SCR_EntityHelper.DeleteEntityAndChildren(fixture);
	}

	//------------------------------------------------------------------------------------------------
	//! The steps the manager holds for the clerk: the pack's, in order, and none of the prefab's errands.
	protected void CheckRoute(LF_TestContext ctx)
	{
		LF_QuestManager manager = LF_QuestManager.GetInstance();
		array<string> ids = {"arrival_register", "arrival_garage", "arrival_apples", "arrival_juice", "arrival_done"};
		foreach (int i, string id : ids)
		{
			LF_QuestDefinition def = manager.GetDefinition(id);
			ctx.NotNull(def, "the clerk offers " + id);
			if (!def)
				continue;

			ctx.False(def.m_sTargetKind.IsEmpty(), id + " guides the player to a place");
			if (i > 0)
			{
				ctx.EqualStr(ids[i - 1], def.m_sRequiredPreviousQuestId, id + " needs " + ids[i - 1]);
				ctx.True(def.m_bAutoAccept && def.m_bAutoTurnIn, id + " starts and finishes on its own");
			}
		}

		array<string> dropped = {"arrival_board", "arrival_work", "arrival_bank"};
		foreach (string gone : dropped)
		{
			ctx.True(manager.GetDefinition(gone) == null, "the island has no " + gone + " step (the row drops the prefab's)");
		}

		LF_QuestDefinition register = manager.GetDefinition("arrival_register");
		if (register)
		{
			ctx.True(register.m_bRewardIdCard, "registering hands the ID card");
			ctx.Equal(PACK_CASH, register.m_iRewardMoney, "registering hands $100 in cash");
		}

		LF_QuestDefinition garageStep = manager.GetDefinition("arrival_garage");
		if (garageStep)
		{
			ctx.True(LF_Utils.SamePrefab(garageStep.m_sStartVehiclePrefab, T601), "the garage step puts a T601 in the garage");
			string shopId;
			ctx.NotNull(LF_VehicleLotComponent.FindOfferOnAnyLot(garageStep.m_sStartVehiclePrefab, shopId), "a lot in this world offers the starter car");
			vector target;
			ctx.True(LF_ArrivalGuide.ResolveTarget(garageStep, "0 0 0", target), "the garage step's waypoint finds a garage");
		}

		ctx.True(LF_Test_EveronMarket.TraderPrice(LF_Test_EveronMarket.TraderRows(TRADER), JUICE) > 0, "the island trader buys apple juice");
	}

	//------------------------------------------------------------------------------------------------
	protected void Walk(LF_TestContext ctx, IEntity clerk, LF_VehicleGarageComponent garage)
	{
		LF_QuestManager manager = LF_QuestManager.GetInstance();
		IEntity player = LF_ExploitGuardsWorldFixture.SpawnCharacter(ctx, clerk.GetOrigin() + Vector(0, 0, -2), true);
		if (!player)
			return;

		LF_QuestTrackerComponent tracker = LF_Component<LF_QuestTrackerComponent>.Find(player);
		string uid = "fixture-" + player.GetID().ToString();
		LF_Utils.SetUidForTests(player, uid);
		IEntity trader;
		IEntity car;

		// 1. The clerk: the card, the welcome pack and the cash, once.
		int cashBefore = LF_MoneyUtils.GetCash(player);
		int breadBefore = manager.CountItem(player, BREAD);
		int waterBefore = manager.CountItem(player, WATER);
		if (!RedProof())
			ctx.True(tracker.RequestConverse("clerk"), "talking to the clerk registers the newcomer");

		ctx.True(Status(tracker, "arrival_register") == LF_EQuestStatus.TURNED_IN, "step 1 is done at the clerk");
		ctx.Equal(cashBefore + PACK_CASH, LF_MoneyUtils.GetCash(player), "the pack holds $100 in cash");
		int breadAfter = manager.CountItem(player, BREAD);
		ctx.True(breadAfter > breadBefore, "the pack holds a baguette");
		ctx.Equal(waterBefore + 1, manager.CountItem(player, WATER), "the pack holds a bottle of water");
		ctx.True(Status(tracker, "arrival_garage") == LF_EQuestStatus.ACTIVE, "step 2 (the garage) starts on its own");

		// 2. The starter car: one, stored, owned, and never a second.
		ctx.Equal(1, LF_VehicleOwnership.StoredFor(uid).Count(), "one starter car waits in the newcomer's garage");
		string vehicleId = LF_VehicleOwnership.LatestStored(uid);
		ctx.True(LF_Utils.SamePrefab(LF_VehicleOwnership.GetPrefab(vehicleId), T601), "the starter car is the T601");
		ctx.False(tracker.RequestConverse("clerk"), "a second talk hands nothing over");
		ctx.Equal(cashBefore + PACK_CASH, LF_MoneyUtils.GetCash(player), "a second talk pays nothing");
		ctx.Equal(breadAfter, manager.CountItem(player, BREAD), "a second talk hands no second pack");

		LF_QuestDefinition garageStep = manager.GetDefinition("arrival_garage");
		if (garageStep)
			ctx.False(LF_StarterVehicle.Grant(player, uid, garageStep), "the garage step starting again (a respawn) gives no second car");
		ctx.Equal(1, LF_VehicleOwnership.StoredFor(uid).Count(), "still one car in the garage");

		// Out of order: apples, juice and a sale do not move the garage step.
		manager.ReportGather(player, APPLE);
		manager.ReportProcess(player, JUICE);
		ctx.True(Status(tracker, "arrival_apples") == LF_EQuestStatus.AVAILABLE, "apples picked before the car do not count");
		ctx.True(Status(tracker, "arrival_juice") == LF_EQuestStatus.AVAILABLE, "juice pressed before the car does not count");

		// Taking the car out at the counter, with the counter's own action.
		player.SetOrigin(garage.GetOwner().GetOrigin() + Vector(1, 0, 1));
		LF_VehicleRetrieveAction takeOut = RetrieveActionOf(garage.GetOwner());
		ctx.NotNull(takeOut, "the garage counter has the take-out action");
		if (takeOut)
			takeOut.PerformAction(garage.GetOwner(), player);

		car = LF_VehicleOwnership.GetVehicle(vehicleId);
		ctx.NotNull(car, "the starter car stands on the garage pad");
		ctx.True(Status(tracker, "arrival_garage") == LF_EQuestStatus.TURNED_IN, "taking the car out finishes step 2");
		ctx.True(Status(tracker, "arrival_apples") == LF_EQuestStatus.ACTIVE, "step 3 (apples) starts on its own");

		// 3. Apples: nine picks.
		for (int i = 0; i < APPLES - 1; i++)
		{
			manager.ReportGather(player, APPLE);
		}

		ctx.True(Status(tracker, "arrival_apples") == LF_EQuestStatus.ACTIVE, "eight apples are not yet nine");
		manager.ReportProcess(player, JUICE);
		ctx.True(Status(tracker, "arrival_juice") == LF_EQuestStatus.AVAILABLE, "juice before the ninth apple does not count");
		manager.ReportGather(player, APPLE);
		ctx.True(Status(tracker, "arrival_apples") == LF_EQuestStatus.TURNED_IN, "the ninth apple finishes step 3");

		// 4. The juice press.
		ctx.True(Status(tracker, "arrival_juice") == LF_EQuestStatus.ACTIVE, "step 4 (juice) starts on its own");
		manager.ReportProcess(player, JUICE);
		ctx.True(Status(tracker, "arrival_juice") == LF_EQuestStatus.TURNED_IN, "a pressed bottle finishes step 4");

		// 5. The market: a real sale of a bottle to the island trader.
		ctx.True(Status(tracker, "arrival_done") == LF_EQuestStatus.ACTIVE, "step 5 (the market) starts on its own");
		trader = LF_Utils.SpawnEntityPrefab(TRADER, player.GetOrigin() + Vector(1.5, 0, 0));
		SCR_InventoryStorageManagerComponent pockets = LF_Component<SCR_InventoryStorageManagerComponent>.Find(player);
		// The jacket pockets are full after the pack: make room for the bottle.
		LF_InventoryUtils.RemoveAmount(player, BREAD, manager.CountItem(player, BREAD));
		LF_InventoryUtils.RemoveAmount(player, WATER, manager.CountItem(player, WATER));
		LF_InventoryUtils.RemoveAmount(player, MEAT, manager.CountItem(player, MEAT));
		ctx.Equal(1, LF_InventoryUtils.AddAmount(pockets, JUICE, 1), "the newcomer carries a bottle of juice");
		array<IEntity> bottles = LF_InventoryUtils.FindItemsByPrefab(LF_InventoryUtils.GetResponsibleStorageManager(player), JUICE);
		ctx.True(trader != null && !bottles.IsEmpty(), "the trader stands beside the newcomer with the bottle");
		if (trader && !bottles.IsEmpty())
		{
			int cashBeforeSale = LF_MoneyUtils.GetCash(player);
			ctx.True(LF_TraderSale.Sell(player, bottles[0], trader, true) == LF_ETraderSale.SOLD, "the trader buys the juice");
			ctx.True(LF_MoneyUtils.GetCash(player) > cashBeforeSale, "the sale pays cash");
		}

		ctx.True(Status(tracker, "arrival_done") == LF_EQuestStatus.TURNED_IN, "selling the juice finishes the route");

		// Clean up what the test made: the car (and its record), the trader, the body.
		if (car)
			SCR_EntityHelper.DeleteEntityAndChildren(car);
		LF_VehicleOwnership.Scrap(vehicleId);
		if (trader)
			SCR_EntityHelper.DeleteEntityAndChildren(trader);
		LF_Utils.SetUidForTests(null, "");
		LF_TestCharacterFixture.Delete(player);
	}

	//------------------------------------------------------------------------------------------------
	protected LF_VehicleRetrieveAction RetrieveActionOf(IEntity counter)
	{
		ActionsManagerComponent actions = LF_Component<ActionsManagerComponent>.Find(counter);
		if (!actions)
			return null;

		array<BaseUserAction> list = {};
		actions.GetActionsList(list);
		foreach (BaseUserAction action : list)
		{
			LF_VehicleRetrieveAction takeOut = LF_VehicleRetrieveAction.Cast(action);
			if (takeOut)
				return takeOut;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected LF_VehicleGarageComponent NearestGarage(vector from)
	{
		LF_VehicleGarageComponent best;
		float bestDistance = -1;
		foreach (LF_VehicleGarageComponent garage : LF_VehicleGarageComponent.GetAll())
		{
			if (!garage || !garage.GetOwner() || garage.GetPadId().IsEmpty())
				continue;

			float distance = vector.DistanceSq(garage.GetOwner().GetOrigin(), from);
			if (bestDistance < 0 || distance < bestDistance)
			{
				bestDistance = distance;
				best = garage;
			}
		}

		return best;
	}

	//------------------------------------------------------------------------------------------------
	protected LF_EQuestStatus Status(LF_QuestTrackerComponent tracker, string questId)
	{
		return tracker.GetTracker().GetStatus(questId);
	}
}
