// red-proof: 2026-11-23 `tools/cli test --no-build --red LF_Test_StolenCarPager` RED PROOF OK: with RedProof() on the report is not filed or paged, so the second pager held 0 lines (expected 1) and the second report left one line (expected 2). Green without the hook.
//------------------------------------------------------------------------------------------------
//! police/stolen-car-pager (#4212): an owner's stolen-car report puts the plate (from the plate
//! registry, Feature/LicensePlate) on the hotlist and on every on-duty officer's pager line, next
//! to the car; the report stays on the hotlist; recovering the car clears the hotlist and the line.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_StolenCarPager : LF_Test
{
	static const ResourceName PAGER_PREFAB = "{747E7E1669676AA1}Prefabs/Items/Roleplay/LF_Pager.et";
	protected static const string PLATE = "ST CP 4212";
	protected static const string OTHER_PLATE = "ST CQ 4213";
	protected static const string CAR_KEY = "#EL-Vehicle_S105";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "police/stolen-car-pager";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_StolenVehicles.ResetStatics();
		IEntity firstEntity = LF_Utils.SpawnEntityPrefab(PAGER_PREFAB, vector.Zero);
		IEntity secondEntity = LF_Utils.SpawnEntityPrefab(PAGER_PREFAB, vector.Zero);
		IEntity offDutyEntity = LF_Utils.SpawnEntityPrefab(PAGER_PREFAB, vector.Zero);
		ctx.True(firstEntity && secondEntity && offDutyEntity, "the pager prefab spawns");
		if (!firstEntity || !secondEntity || !offDutyEntity)
			return;

		LF_PagerComponent first = LF_PagerComponent.Cast(firstEntity.FindComponent(LF_PagerComponent));
		LF_PagerComponent second = LF_PagerComponent.Cast(secondEntity.FindComponent(LF_PagerComponent));
		LF_PagerComponent offDuty = LF_PagerComponent.Cast(offDutyEntity.FindComponent(LF_PagerComponent));
		ctx.True(first && second && offDuty, "the pager prefab carries LF_PagerComponent");
		if (first && second && offDuty)
		{
			// The plate is the one the plate registry knows (Feature/LicensePlate).
			LF_LicensePlateRegistry.Remember(PLATE, "stolen-pager-car");
			LF_LicensePlateRegistry.Remember(OTHER_PLATE, "stolen-pager-other");

			// The subscriber is on the bus for both events once the pager installs.
			LF_PagerSubscriber.Install();
			ctx.True(LF_EventBus.IsSubscribed(LF_OnVehicleReportedStolen, LF_StolenCarPagerSubscriber.s_Subscriber), "a stolen-car report reaches the pager subscriber");
			ctx.True(LF_EventBus.IsSubscribed(LF_OnVehicleRecovered, LF_StolenCarPagerSubscriber.s_Subscriber), "a recovery reaches the pager subscriber");

			// A report: the hotlist and the pagers.
			LF_StolenReport report;
			if (!RedProof())
				report = LF_StolenVehicles.FileReport("stolen-pager-car", PLATE, CAR_KEY, "stolen-pager-owner", "#LF-Wanted_PlaceGrid 003-002");

			LF_StolenReport found;
			ctx.Equal(LF_EPlateStatus.STOLEN, LF_StolenVehicles.CheckPlate(PLATE, found), "the reported plate is on the hotlist");

			array<LF_PagerComponent> onDuty = {};
			onDuty.Insert(first);
			onDuty.Insert(second);
			if (report && !RedProof())
				ctx.Equal(2, LF_StolenCarPager.Page(report, onDuty, "21:14"), "the report is paged to both on-duty officers");

			ctx.Equal(1, first.MessageCount(), "the first officer's pager holds the report");
			ctx.Equal(1, second.MessageCount(), "the second officer's pager holds the report");
			ctx.Equal(0, offDuty.MessageCount(), "a pager nobody on duty carries stays empty");

			array<string> messages = {};
			first.GetMessages(messages);
			if (messages.Count() == 1)
			{
				ctx.True(LF_StolenCarPager.IsEntry(messages[0]), "the pager entry is a stolen-car line");
				ctx.True(messages[0].Contains(PLATE), "the pager entry carries the plate");
				ctx.True(messages[0].Contains(CAR_KEY), "the pager entry carries the car");
				ctx.True(LF_StolenCarPager.EntryLine(messages[0]).Contains(PLATE), "the pager line reads the plate");
			}

			array<ref LF_UIPart> card = LF_Pager.BuildParts(messages);
			ctx.Equal(3, card.Count(), "the pager card is a title, the stolen-car line and a close button");

			// A second car's line, so a recovery can be told to take only its own.
			LF_StolenReport otherReport = LF_StolenVehicles.FileReport("stolen-pager-other", OTHER_PLATE, CAR_KEY, "stolen-pager-owner", "#LF-Wanted_PlaceGrid 003-003");
			if (otherReport)
				LF_StolenCarPager.Page(otherReport, onDuty, "21:20");

			ctx.Equal(2, first.MessageCount(), "a second report adds a second line");

			// The report stays until the car is recovered, however many other calls arrive.
			ctx.Equal(LF_EPlateStatus.STOLEN, LF_StolenVehicles.CheckPlate(PLATE, found), "the report is still on the hotlist before the recovery");

			// Recovery: the hotlist and the pager lines of that plate go; the other car's stay.
			if (!RedProof())
				LF_StolenVehicles.Clear("stolen-pager-car", "test recovery");

			ctx.Equal(LF_EPlateStatus.CLEAR, LF_StolenVehicles.CheckPlate(PLATE, found), "a recovered plate is off the hotlist");
			if (!RedProof())
				ctx.Equal(2, LF_StolenCarPager.Clear(PLATE, onDuty), "the recovery removes the line from both pagers");

			ctx.Equal(1, first.MessageCount(), "the first officer keeps only the other car's line");
			ctx.Equal(1, second.MessageCount(), "the second officer keeps only the other car's line");
			first.GetMessages(messages);
			ctx.True(messages.Count() == 1 && messages[0].Contains(OTHER_PLATE) && !messages[0].Contains(PLATE), "the line left is the other car's");
			ctx.Equal(LF_EPlateStatus.STOLEN, LF_StolenVehicles.CheckPlate(OTHER_PLATE, found), "the other car stays on the hotlist");

			LF_LicensePlateRegistry.Release(PLATE);
			LF_LicensePlateRegistry.Release(OTHER_PLATE);
		}

		LF_StolenVehicles.ResetStatics();
		SCR_EntityHelper.DeleteEntityAndChildren(firstEntity);
		SCR_EntityHelper.DeleteEntityAndChildren(secondEntity);
		SCR_EntityHelper.DeleteEntityAndChildren(offDutyEntity);
	}
}
