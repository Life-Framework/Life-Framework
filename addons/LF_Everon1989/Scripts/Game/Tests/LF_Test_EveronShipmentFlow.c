// red-proof: observed 2026-11-19 (#3768): `tools\cli test --no-build --red LF_Test_QuestChoice,LF_Test_EveronShipmentQuest,LF_Test_EveronShipmentFlow,LF_Test_BuyerSale` printed `RED PROOF OK (all 2 listed test(s) fail with the hook on)` in both legs; the acts that RedProof() skips fail the choice, status, standing and sale assertions. `--only` then passed.
//! #3768 (M7 arc 3, beat 2 of #1036): the shipment quest is playable end to end under the Everon 1989 header.
//! The harbour master (a quest giver the talk reaches through the QuestConverse reply) hands out the count; holding the
//! sample counts it and the sample stays in the bag; the police desk takes the report and the sample; a sale to the buyer
//! closes the other ending. The two endings are one choice: reporting fails the sale and a sale shuts the report out, and
//! each pays its own standing. A fresh character walks count -> report; another walks count -> sell.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronShipmentFlow : LF_Test
{
	static const ResourceName HARBOUR_MASTER = "{B45B7C6C168822D4}Configs/Npcs/LF_Npc_Townsfolk_HarbourMaster.et";
	static const ResourceName POLICE_DESK = "{034D1397A95787DB}Prefabs/Police/LF_PoliceDutyDesk.et";
	static const ResourceName SAMPLE = "{968892A9535FE6A4}Prefabs/Items/Contraband/LF_ContrabandCrate.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/shipment-flow";
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_EQuestStatus Status(LF_QuestTrackerComponent tracker, string questId)
	{
		return tracker.GetTracker().GetStatus(questId);
	}

	//------------------------------------------------------------------------------------------------
	protected static string UidOf(IEntity player)
	{
		return "fixture-" + player.GetID().ToString();
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_QuestManager manager = LF_QuestManager.GetInstance();
		LF_QuestGiverTable.ResetStatics();

		// The harbour master: the world's own when it stands there, else a fixture.
		IEntity marcel;
		LF_QuestGiverComponent harbour = manager.GetGiver("harbour_master");
		if (!harbour)
		{
			marcel = LF_Utils.SpawnEntityPrefab(HARBOUR_MASTER, "60 0 60");
			ctx.NotNull(marcel, "the harbour master prefab spawns");
			harbour = LF_Component<LF_QuestGiverComponent>.Find(marcel);
		}

		ctx.NotNull(harbour, "the harbour master is a quest giver");
		IEntity desk;
		LF_QuestGiverComponent police = manager.GetGiver("police_desk");
		if (!police)
		{
			desk = LF_Utils.SpawnEntityPrefab(POLICE_DESK, "70 0 70");
			ctx.NotNull(desk, "the police duty desk prefab spawns");
			police = LF_Component<LF_QuestGiverComponent>.Find(desk);
		}

		ctx.NotNull(police, "the police duty desk is a quest giver");
		if (harbour && police)
		{
			ctx.True(harbour.IsConverse(), "the harbour master talks in person");
			ctx.True(police.IsConverse(), "the police desk talks in person");
			RunWith(ctx, manager, harbour, police);
		}

		if (marcel)
			SCR_EntityHelper.DeleteEntityAndChildren(marcel);

		if (desk)
			SCR_EntityHelper.DeleteEntityAndChildren(desk);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunWith(LF_TestContext ctx, LF_QuestManager manager, LF_QuestGiverComponent harbour, LF_QuestGiverComponent police)
	{
		ctx.EqualStr("harbour_master", manager.GetGiverOfQuest("shipment_count"), "the harbour master offers the count");
		ctx.EqualStr("police_desk", manager.GetGiverOfQuest("shipment_report"), "the police desk takes the report");
		ctx.True(manager.GetDefinition("shipment_sold") != null, "the sale ending is registered");
		if (ctx.FailureCount() > 0)
			return;

		vector atHarbour = harbour.GetOwner().GetOrigin() + "0 0 1.5";
		vector atDesk = police.GetOwner().GetOrigin() + "0 0 1.5";
		IEntity reporter = LF_ExploitGuardsWorldFixture.SpawnCharacter(ctx, atHarbour, true);
		IEntity seller = LF_ExploitGuardsWorldFixture.SpawnCharacter(ctx, atHarbour, true);
		if (reporter && seller)
		{
			RunReport(ctx, manager, harbour, police, reporter, atHarbour, atDesk);
			RunSell(ctx, manager, harbour, police, seller, atHarbour, atDesk);
		}

		if (reporter)
			SCR_EntityHelper.DeleteEntityAndChildren(reporter);

		if (seller)
			SCR_EntityHelper.DeleteEntityAndChildren(seller);

		LF_Reputation.ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	//! The count: Marcel hands it out, holding the sample counts it, and the sample stays in the bag.
	//! Leaves the player with the sale running.
	protected bool Count(LF_TestContext ctx, LF_QuestGiverComponent harbour, IEntity player, LF_QuestTrackerComponent tracker, vector atHarbour)
	{
		player.SetOrigin(atHarbour);
		if (!RedProof())
			tracker.RequestConverse("harbour_master");

		ctx.True(Status(tracker, "shipment_count") == LF_EQuestStatus.ACTIVE, "talking to the harbour master takes the count");
		ctx.True(Status(tracker, "shipment_sold") == LF_EQuestStatus.AVAILABLE, "the sale waits for the count");

		ctx.Equal(1, LF_InventoryUtils.AddAmount(player, SAMPLE, 1), "the character holds the sample");
		tracker.RequestConverse("harbour_master");
		ctx.True(Status(tracker, "shipment_count") == LF_EQuestStatus.TURNED_IN, "holding the sample counts it and the count is turned in");
		ctx.Equal(1, LF_InventoryUtils.GetAmount(player, SAMPLE), "the count leaves the sample in the bag");
		ctx.True(Status(tracker, "shipment_sold") == LF_EQuestStatus.ACTIVE, "the sale ending starts on its own");
		ctx.True(Status(tracker, "shipment_report") == LF_EQuestStatus.AVAILABLE, "the report is not taken until the desk is spoken to");
		return Status(tracker, "shipment_count") == LF_EQuestStatus.TURNED_IN;
	}

	//------------------------------------------------------------------------------------------------
	protected void RunReport(LF_TestContext ctx, LF_QuestManager manager, LF_QuestGiverComponent harbour, LF_QuestGiverComponent police, IEntity player, vector atHarbour, vector atDesk)
	{
		LF_QuestTrackerComponent tracker = LF_QuestTrackerComponent.Cast(player.FindComponent(LF_QuestTrackerComponent));
		ctx.NotNull(tracker, "the reporter carries a quest tracker");
		if (!tracker)
			return;

		string uid = UidOf(player);
		int town = LF_Reputation.Points(uid, "TOWN");
		int policePoints = LF_Reputation.Points(uid, "POLICE");
		int underworld = LF_Reputation.Points(uid, "UNDERWORLD");
		if (!Count(ctx, harbour, player, tracker, atHarbour))
			return;

		// At the desk the player reports: the sale is closed, the sample is handed over, the three standings move.
		player.SetOrigin(atDesk);
		tracker.RequestConverse("police_desk");
		ctx.True(Status(tracker, "shipment_report") == LF_EQuestStatus.TURNED_IN, "the desk takes the report");
		ctx.True(Status(tracker, "shipment_sold") == LF_EQuestStatus.FAILED, "reporting closes the sale ending");
		ctx.Equal(0, LF_InventoryUtils.GetAmount(player, SAMPLE), "the desk keeps the sample");
		ctx.True(LF_Reputation.Points(uid, "TOWN") > town, "reporting raises the town's standing");
		ctx.True(LF_Reputation.Points(uid, "POLICE") > policePoints, "reporting raises the police's standing");
		ctx.True(LF_Reputation.Points(uid, "UNDERWORLD") < underworld, "reporting lowers the underworld's standing");

		// A buyer paying later changes nothing: the sale is closed for good.
		int townAfter = LF_Reputation.Points(uid, "TOWN");
		array<ResourceName> sold = {};
		sold.Insert(SAMPLE);
		LF_QuestSignals.ReportBuyerSale(player, sold);
		ctx.True(Status(tracker, "shipment_sold") == LF_EQuestStatus.FAILED, "a sale after the report does not reopen the sale ending");
		ctx.Equal(townAfter, LF_Reputation.Points(uid, "TOWN"), "and costs the town nothing");

		// Marcel thanks the reporter and not the seller.
		RunThanks(ctx, harbour, player, "shipment_report", "shipment_sold");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunSell(LF_TestContext ctx, LF_QuestManager manager, LF_QuestGiverComponent harbour, LF_QuestGiverComponent police, IEntity player, vector atHarbour, vector atDesk)
	{
		LF_QuestTrackerComponent tracker = LF_QuestTrackerComponent.Cast(player.FindComponent(LF_QuestTrackerComponent));
		ctx.NotNull(tracker, "the seller carries a quest tracker");
		if (!tracker)
			return;

		string uid = UidOf(player);
		int town = LF_Reputation.Points(uid, "TOWN");
		int policePoints = LF_Reputation.Points(uid, "POLICE");
		int underworld = LF_Reputation.Points(uid, "UNDERWORLD");
		if (!Count(ctx, harbour, player, tracker, atHarbour))
			return;

		// Another good sold to the buyer does not close the ending; the sample does.
		array<ResourceName> other = {};
		other.Insert("{5280F09CEB005042}Prefabs/Items/Drinks/Moonshine.et");
		LF_QuestSignals.ReportBuyerSale(player, other);
		ctx.True(Status(tracker, "shipment_sold") == LF_EQuestStatus.ACTIVE, "selling moonshine does not close the sale ending");

		array<ResourceName> sample = {};
		sample.Insert(SAMPLE);
		if (!RedProof())
			LF_QuestSignals.ReportBuyerSale(player, sample);

		ctx.True(Status(tracker, "shipment_sold") == LF_EQuestStatus.TURNED_IN, "the buyer paying for the sample closes the sale ending");
		ctx.True(LF_Reputation.Points(uid, "UNDERWORLD") > underworld, "selling raises the underworld's standing");
		ctx.True(LF_Reputation.Points(uid, "TOWN") < town, "selling costs the town a little");
		ctx.Equal(policePoints, LF_Reputation.Points(uid, "POLICE"), "selling does not touch the police's standing");

		// The desk no longer takes the report: the choice was made.
		player.SetOrigin(atDesk);
		tracker.RequestConverse("police_desk");
		ctx.True(Status(tracker, "shipment_report") == LF_EQuestStatus.AVAILABLE, "a seller is not offered the report");
		ctx.Equal(1, LF_InventoryUtils.GetAmount(player, SAMPLE), "and the desk takes nothing from them");

		RunThanks(ctx, harbour, player, "shipment_sold", "shipment_report");
	}

	//------------------------------------------------------------------------------------------------
	//! Marcel's greeting offers the thanks for the ending the player took, and only that one.
	protected void RunThanks(LF_TestContext ctx, LF_QuestGiverComponent harbour, IEntity player, string took, string refused)
	{
		LF_DialogueConfig config = LF_DialogueComponent.ConfigOf(harbour.GetOwner());
		ctx.NotNull(config, "the harbour master has a playable dialogue");
		if (!config)
			return;

		LF_DialogueNode greet = LF_DialogueFlow.FindNode(config, "shipment.ask");
		ctx.NotNull(greet, "the harbour master has the crate question");
		if (!greet)
			return;

		string tookNext = "shipment.thanks_report";
		string refusedNext = "shipment.thanks_sell";
		if (took == "shipment_sold")
		{
			tookNext = "shipment.thanks_sell";
			refusedNext = "shipment.thanks_report";
		}

		bool offeredTook;
		bool offeredRefused;
		foreach (LF_DialogueOption option : greet.m_aOptions)
		{
			if (option.m_sNext == tookNext && LF_DialogueHook.Offers(option, player, harbour.GetOwner()))
				offeredTook = true;

			if (option.m_sNext == refusedNext && LF_DialogueHook.Offers(option, player, harbour.GetOwner()))
				offeredRefused = true;
		}

		ctx.True(offeredTook, "Marcel offers the reply for " + took);
		ctx.False(offeredRefused, "Marcel does not offer the reply for " + refused);
	}
}
