// red-proof: Observed 2026-11-18 with `tools\cli test --no-build --red LF_Test_QuestSignalsProgress`: `RED PROOF OK`; with each act skipped, `selling the fish moves its objective`, `which reaches level 2 (expected 2, got 1)` and `reaching level 2 moves the level 2 objective` fail. `--only` passes. Flake fix 2026-11-18: failed `--tier all` (and `--only LF_Test_FishSpeciesPrefabs,LF_Test_QuestSignalsProgress`) with `the seller lands a fish (expected 1, got 0)` on the unfixed test; the fish sale now runs on its own seller wearing a pack and both runs pass.
//! quests/signals-progress (#3123): each hour-2 milestone reports its SIGNAL through the public server
//! entry point of its own feature (plot buy, car lot buy, licence desk visit, fish counter sale, job XP
//! award), moves only its own objective, and a refused purchase or sale moves nothing.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_QuestSignalsProgress : LF_Test
{
	protected static const string UID = "lf-test-quest-progress";
	protected static const ResourceName PLOT_SIGN = "{DA9D4FEB9BD67EA5}Prefabs/Props/Property/LF_PlotSign.et";
	protected static const ResourceName FISH_COUNTER = "{2F5749F913C9B23D}Prefabs/Props/Commercial/LF_FishCounter.et";
	protected static const ResourceName FISH = "{ACCC8B6FEB32034C}Prefabs/Items/Fishing/LF_FishTrout.et";
	protected static const string PLOT = "plot-a";
	protected static const string JOB = "DELIVERY";
	protected static const int FEE = 300;
	protected static const int T0 = 14400000;	// in-game minute 10000 days after 1970-01-01

	override string GetName()
	{
		return "quests/signals-progress";
	}

	override void Run(LF_TestContext ctx)
	{
		ctx.EqualStr("vehicle_bought", LF_QuestProgressSignals.VEHICLE_BOUGHT, "the car key");
		ctx.EqualStr("plot_bought", LF_QuestProgressSignals.PLOT_BOUGHT, "the plot key");
		ctx.EqualStr("permit_collected", LF_QuestProgressSignals.PERMIT_COLLECTED, "the permit key");
		ctx.EqualStr("fish_sold", LF_QuestProgressSignals.FISH_SOLD, "the fish key");
		ctx.EqualStr("job_level:DELIVERY:2", LF_QuestProgressSignals.JobLevel(JOB, 2), "the job level key names the job and the level");
		ctx.True(LF_QuestProgressSignals.IsFish(FISH), "a trout is a fish");
		ctx.False(LF_QuestProgressSignals.IsFish(PLOT_SIGN), "a plot sign is not");

		LF_QuestManager manager = LF_QuestManager.GetInstance();
		ctx.NotNull(manager, "the quest manager exists");
		IEntity player = LF_TestCharacterFixture.Spawn(ctx);
		if (!manager || !player)
			return;

		LF_QuestTrackerComponent tracker = LF_QuestTrackerComponent.Cast(player.FindComponent(LF_QuestTrackerComponent));
		ctx.NotNull(tracker, "the test character carries a quest tracker");
		if (!tracker)
		{
			LF_TestCharacterFixture.Delete(player);
			return;
		}

		array<string> keys = {
			LF_QuestProgressSignals.VEHICLE_BOUGHT,
			LF_QuestProgressSignals.PLOT_BOUGHT,
			LF_QuestProgressSignals.PERMIT_COLLECTED,
			LF_QuestProgressSignals.FISH_SOLD,
			LF_QuestProgressSignals.JobLevel(JOB, 2),
			LF_QuestProgressSignals.JobLevel(JOB, 3)
		};
		foreach (string key : keys)
			Open(manager, tracker, key);

		// Another feature's signal moves none of them.
		ctx.False(manager.ReportSignal(player, LF_QuestSignals.ATM_DEPOSIT), "a deposit moves none of the milestones");

		RunPlot(ctx, tracker, player);
		RunVehicle(ctx, tracker, player);
		RunPermit(ctx, tracker, player);
		RunFish(ctx, manager);
		RunJobLevel(ctx, tracker, player);

		LF_TestCharacterFixture.Delete(player);
		LF_QuestProgressSignals.SetBodyForTest(UID, null);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunPlot(LF_TestContext ctx, LF_QuestTrackerComponent tracker, IEntity player)
	{
		LF_PropertyConfig.Reload();
		LF_Properties.ResetStatics();
		int price = LF_PropertyConfig.PlotPrice(PLOT);
		ctx.True(price > 0, "the example plot has a price");
		IEntity sign = LF_Utils.SpawnEntityPrefab(PLOT_SIGN, player.GetOrigin() + "1 0 0");
		LF_PlotComponent plot;
		if (sign)
			plot = LF_Component<LF_PlotComponent>.Find(sign);

		ctx.NotNull(plot, "the plot sign spawns beside the buyer");
		if (price <= 0 || !plot)
		{
			LF_TestCharacterFixture.Delete(sign);
			return;
		}

		plot.SetPlotKey(PLOT);
		SetCash(player, price - 1);
		string plotKey;
		ctx.Equal(LF_EPlotBuy.REFUSED_NO_CASH, LF_PlotComponent.Buy(player, UID, plotKey, PLOT), "a buyer one short is refused");
		ctx.False(Done(tracker, LF_QuestProgressSignals.PLOT_BOUGHT), "a refused plot purchase moves nothing");

		SetCash(player, price);
		if (!RedProof())
			ctx.Equal(LF_EPlotBuy.BOUGHT, LF_PlotComponent.Buy(player, UID, plotKey, PLOT), "a buyer with the price buys the plot");

		ctx.True(Done(tracker, LF_QuestProgressSignals.PLOT_BOUGHT), "buying the plot moves its objective");
		ctx.False(Done(tracker, LF_QuestProgressSignals.VEHICLE_BOUGHT), "and not the car's");
		LF_Properties.ResetStatics();
		LF_TestCharacterFixture.Delete(sign);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunVehicle(LF_TestContext ctx, LF_QuestTrackerComponent tracker, IEntity player)
	{
		IEntity lotEntity = GetGame().GetWorld().FindEntityByName(LF_TestAnnex.VEHICLE_LOT);
		ctx.NotNull(lotEntity, "the world places the annex vehicle lot");
		if (!lotEntity)
			return;

		LF_VehicleLotComponent lot = LF_Component<LF_VehicleLotComponent>.Find(lotEntity);
		ctx.NotNull(lot, "the lot carries LF_VehicleLotComponent");
		if (!lot)
			return;

		string vehicleId;
		int cashCharged;
		string reasonKey;
		ctx.False(lot.Buy(UID, "{0000000000000000}Prefabs/Vehicles/NotSold.et", player, vehicleId, cashCharged, reasonKey), "a car the lot does not sell is refused");
		ctx.False(Done(tracker, LF_QuestProgressSignals.VEHICLE_BOUGHT), "a refused car purchase moves nothing");

		LF_BankAccount account = LF_ATMManager.GetOrCreate(UID);
		ctx.NotNull(account, "the buyer has a bank account");
		if (!account)
			return;

		account.Deposit(lot.GetPrice(LF_VehicleCatalog.S105));
		bool bought;
		if (!RedProof())
			bought = lot.Buy(UID, LF_VehicleCatalog.S105, player, vehicleId, cashCharged, reasonKey);

		ctx.True(bought || RedProof(), "the lot sells the S105 to a buyer with the price");
		ctx.True(Done(tracker, LF_QuestProgressSignals.VEHICLE_BOUGHT), "buying the car moves its objective");

		if (!vehicleId.IsEmpty())
		{
			IEntity vehicle = LF_VehicleOwnership.GetVehicle(vehicleId);
			IEntity key = LF_VehicleOwnership.GetKey(vehicleId);
			LF_VehicleOwnership.Scrap(vehicleId);
			LF_TestCharacterFixture.Delete(vehicle);
			LF_TestCharacterFixture.Delete(key);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void RunPermit(LF_TestContext ctx, LF_QuestTrackerComponent tracker, IEntity player)
	{
		LF_Progression.Forget(UID);
		LF_LicencesConfig.SetDeskForTest(FEE, 0);
		LF_LicenceDesk.SetNowForTest(T0);
		LF_PlayerAccountManager.GetOrCreate(UID);
		LF_EntityCashWallet wallet = new LF_EntityCashWallet(player, LF_LicenceDesk.LEDGER_REASON, string.Empty);
		int left;

		SetCash(player, FEE - 1);
		ctx.Equal(LF_ELicenceDesk.NOT_ENOUGH_CASH, LF_LicenceDesk.VisitFor(player, UID, wallet, left), "an applicant short of the fee is refused");
		ctx.False(Done(tracker, LF_QuestProgressSignals.PERMIT_COLLECTED), "a refused application moves nothing");

		SetCash(player, FEE);
		if (!RedProof())
			ctx.Equal(LF_ELicenceDesk.ISSUED, LF_LicenceDesk.VisitFor(player, UID, wallet, left), "with the fee and no wait the desk issues the permit");

		ctx.True(Done(tracker, LF_QuestProgressSignals.PERMIT_COLLECTED), "collecting the permit moves its objective");

		LF_Progression.Forget(UID);
		LF_LicenceDesk.SetNowForTest(-1);
		LF_LicencesConfig.SetDeskForTest(-1, -1);
	}

	//------------------------------------------------------------------------------------------------
	//! The sale runs on its own fresh character wearing a pack: the jacket alone rejected the trout
	//! after fishing/species-prefabs ran first (#3123).
	protected void RunFish(LF_TestContext ctx, LF_QuestManager manager)
	{
		IEntity seller = LF_TestCharacterFixture.Spawn(ctx);
		if (!seller)
			return;

		// The jacket alone is too small for a trout once other tests have run; the fishing tests wear a pack too.
		InventoryStorageManagerComponent inventory = LF_InventoryUtils.GetResponsibleStorageManager(seller);
		IEntity pack = LF_Utils.SpawnEntityPrefab(LF_TestVaultDrillFixture.PACK_PREFAB, seller.GetOrigin());
		bool wearing = pack && inventory && inventory.TryInsertItem(pack, EStoragePurpose.PURPOSE_LOADOUT_PROXY);
		ctx.True(wearing, "the seller wears a pack");
		LF_QuestTrackerComponent tracker = LF_QuestTrackerComponent.Cast(seller.FindComponent(LF_QuestTrackerComponent));
		ctx.NotNull(tracker, "the seller carries a quest tracker");
		if (!wearing || !tracker)
		{
			if (!wearing)
				LF_TestCharacterFixture.Delete(pack);

			LF_TestCharacterFixture.Delete(seller);
			return;
		}

		Open(manager, tracker, LF_QuestProgressSignals.FISH_SOLD);
		IEntity counter = LF_Utils.SpawnEntityPrefab(FISH_COUNTER, seller.GetOrigin() + "1 0 0");
		LF_ShopComponent shop;
		if (counter)
			shop = LF_Component<LF_ShopComponent>.Find(counter);

		ctx.NotNull(shop, "the fish counter spawns with a shop");
		if (!shop)
		{
			LF_TestCharacterFixture.Delete(counter);
			LF_TestCharacterFixture.Delete(seller);
			return;
		}

		int earned;
		string missing;
		ctx.True(shop.Sell(FISH, 1, seller, earned, missing) != LF_EShopRefusal.NONE, "selling a fish the seller does not carry is refused");
		ctx.False(Done(tracker, LF_QuestProgressSignals.FISH_SOLD), "a refused sale moves nothing");

		ctx.Equal(1, LF_InventoryUtils.AddAmount(seller, FISH, 1), "the seller lands a fish");
		if (!RedProof())
			ctx.Equal(LF_EShopRefusal.NONE, shop.Sell(FISH, 1, seller, earned, missing), "the counter buys the fish");

		ctx.True(Done(tracker, LF_QuestProgressSignals.FISH_SOLD), "selling the fish moves its objective");
		LF_TestCharacterFixture.Delete(counter);
		LF_TestCharacterFixture.Delete(seller);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunJobLevel(LF_TestContext ctx, LF_QuestTrackerComponent tracker, IEntity player)
	{
		LF_JobLevels.Forget(UID);
		LF_QuestProgressSignals.SetBodyForTest(UID, player);
		ctx.True(LF_JobLevelSync.EnsureSubscribed(), "the level sync is subscribed on the server");

		ctx.Equal(0, LF_JobLevels.AddXp(UID, JOB, 0, "test"), "a zero award is refused");
		ctx.Equal(400, LF_JobLevels.AddXp(UID, JOB, 400, "test"), "400 xp is granted");
		ctx.Equal(1, LF_JobLevels.LevelOf(UID, JOB), "and is still level 1");
		ctx.False(Done(tracker, LF_QuestProgressSignals.JobLevel(JOB, 2)), "a level not reached moves nothing");

		if (!RedProof())
			ctx.Equal(100, LF_JobLevels.AddXp(UID, JOB, 100, "test"), "100 more xp is granted");

		ctx.Equal(2, LF_JobLevels.LevelOf(UID, JOB), "which reaches level 2");
		ctx.True(Done(tracker, LF_QuestProgressSignals.JobLevel(JOB, 2)), "reaching level 2 moves the level 2 objective");
		ctx.False(Done(tracker, LF_QuestProgressSignals.JobLevel(JOB, 3)), "and not level 3's");

		LF_JobLevels.Forget(UID);
	}

	//------------------------------------------------------------------------------------------------
	//! A quest of one SIGNAL objective on this key, accepted by the character.
	protected void Open(LF_QuestManager manager, LF_QuestTrackerComponent tracker, string key)
	{
		LF_QuestObjective objective = new LF_QuestObjective();
		objective.m_eType = LF_EQuestEventType.SIGNAL;
		objective.m_iTarget = 1;
		objective.m_eMode = LF_EQuestProgressMode.ADD;
		objective.m_sFilter = key;

		LF_QuestDefinition def = new LF_QuestDefinition();
		def.m_sQuestId = QuestId(key);
		def.m_eRequiredJob = LF_EJobType.UNEMPLOYED;
		def.m_aObjectives = new array<ref LF_QuestObjective>();
		def.m_aObjectives.Insert(objective);
		manager.RegisterDefinition(def);
		tracker.GetTracker().Accept(def, 1, LF_EJobType.UNEMPLOYED, "", 0, new array<string>());
	}

	//------------------------------------------------------------------------------------------------
	protected string QuestId(string key)
	{
		return "lf_test_progress_" + key;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the key's objective is complete.
	protected bool Done(LF_QuestTrackerComponent tracker, string key)
	{
		return tracker.GetTracker().GetStatus(QuestId(key)) == LF_EQuestStatus.COMPLETED;
	}

	//------------------------------------------------------------------------------------------------
	protected void SetCash(IEntity character, int amount)
	{
		int carried = LF_MoneyUtils.GetCash(character);
		if (carried > 0)
			LF_MoneyUtils.RemoveCash(character, carried);

		if (amount > 0)
			LF_MoneyUtils.AddCash(character, amount);
	}
}
