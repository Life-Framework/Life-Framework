// red-proof: 2026-11-23 `tools/cli test --no-build --red LF_Test_StolenCarChopPager` RED PROOF OK: with RedProof() on the chop is never handled, so both pagers keep 2 lines (expected 1) and the chopped plate stays on the hotlist. Green without the hook.
//------------------------------------------------------------------------------------------------
//! police/stolen-car-chop-pager (#4355): a stolen car scrapped at a chop shop (LF_OnVehicleChopped)
//! leaves the hotlist and its line leaves the on-duty officers' pagers, like a recovery does; a second
//! car's line stays, and a chop of a car nobody reported removes nothing.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_StolenCarChopPager : LF_Test
{
	static const ResourceName PAGER_PREFAB = "{747E7E1669676AA1}Prefabs/Items/Roleplay/LF_Pager.et";
	protected static const string PLATE = "ST CH 4355";
	protected static const string OTHER_PLATE = "ST CJ 4356";
	protected static const string UNREPORTED_PLATE = "ST CK 4357";
	protected static const string CAR_KEY = "#EL-Vehicle_S105";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "police/stolen-car-chop-pager";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_StolenVehicles.ResetStatics();
		IEntity firstEntity = LF_Utils.SpawnEntityPrefab(PAGER_PREFAB, vector.Zero);
		IEntity secondEntity = LF_Utils.SpawnEntityPrefab(PAGER_PREFAB, vector.Zero);
		ctx.True(firstEntity && secondEntity, "the pager prefab spawns");
		if (!firstEntity || !secondEntity)
			return;

		LF_PagerComponent first = LF_PagerComponent.Cast(firstEntity.FindComponent(LF_PagerComponent));
		LF_PagerComponent second = LF_PagerComponent.Cast(secondEntity.FindComponent(LF_PagerComponent));
		ctx.True(first && second, "the pager prefab carries LF_PagerComponent");
		if (first && second)
		{
			LF_LicensePlateRegistry.Remember(PLATE, "stolen-chop-car");
			LF_LicensePlateRegistry.Remember(OTHER_PLATE, "stolen-chop-other");

			// The pager subscriber listens for a chop as well as a report and a recovery.
			LF_PagerSubscriber.Install();
			ctx.True(LF_EventBus.IsSubscribed(LF_OnVehicleChopped, LF_StolenCarPagerSubscriber.s_Subscriber), "a chopped car reaches the pager subscriber");

			array<LF_PagerComponent> onDuty = {};
			onDuty.Insert(first);
			onDuty.Insert(second);

			// Two reported cars, both paged.
			LF_StolenReport report = LF_StolenVehicles.FileReport("stolen-chop-car", PLATE, CAR_KEY, "stolen-chop-owner", "#LF-Wanted_PlaceGrid 003-002");
			LF_StolenReport otherReport = LF_StolenVehicles.FileReport("stolen-chop-other", OTHER_PLATE, CAR_KEY, "stolen-chop-owner", "#LF-Wanted_PlaceGrid 003-003");
			if (report)
				LF_StolenCarPager.Page(report, onDuty, "21:14");

			if (otherReport)
				LF_StolenCarPager.Page(otherReport, onDuty, "21:20");

			ctx.Equal(2, first.MessageCount(), "the first pager holds both reports");
			ctx.Equal(2, second.MessageCount(), "the second pager holds both reports");

			LF_StolenReport found;
			ctx.Equal(LF_EPlateStatus.STOLEN, LF_StolenVehicles.CheckPlate(PLATE, found), "the plate is on the hotlist before the chop");

			// A chop of a car nobody reported takes nothing off the pagers.
			LF_OnVehicleChopped unreported = LF_OnVehicleChopped.Create("chopper-uid", "shop-1", "stolen-chop-none", UNREPORTED_PLATE, "", 400, 4);
			ctx.Equal(0, LF_StolenCarPager.OnChopped(unreported, onDuty), "a chopped car nobody reported removes no line");
			ctx.Equal(2, first.MessageCount(), "the pagers keep both lines after an unreported chop");

			// The chop: the hotlist entry and the line of that plate go, on both pagers.
			LF_OnVehicleChopped chopped = LF_OnVehicleChopped.Create("chopper-uid", "shop-1", "stolen-chop-car", PLATE, "stolen-chop-owner", 400, 4);
			if (!RedProof())
				ctx.Equal(2, LF_StolenCarPager.OnChopped(chopped, onDuty), "the chop removes the line from both pagers");

			ctx.Equal(LF_EPlateStatus.CLEAR, LF_StolenVehicles.CheckPlate(PLATE, found), "a chopped car is off the hotlist");
			ctx.Equal(1, first.MessageCount(), "the first officer keeps only the other car's line");
			ctx.Equal(1, second.MessageCount(), "the second officer keeps only the other car's line");

			array<string> messages = {};
			first.GetMessages(messages);
			ctx.True(messages.Count() == 1 && messages[0].Contains(OTHER_PLATE) && !messages[0].Contains(PLATE), "the line left is the other car's");
			ctx.Equal(LF_EPlateStatus.STOLEN, LF_StolenVehicles.CheckPlate(OTHER_PLATE, found), "the other car stays on the hotlist");

			LF_LicensePlateRegistry.Release(PLATE);
			LF_LicensePlateRegistry.Release(OTHER_PLATE);
		}

		LF_StolenVehicles.ResetStatics();
		SCR_EntityHelper.DeleteEntityAndChildren(firstEntity);
		SCR_EntityHelper.DeleteEntityAndChildren(secondEntity);
	}
}
