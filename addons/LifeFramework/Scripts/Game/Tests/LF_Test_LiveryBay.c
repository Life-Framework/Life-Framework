// red-proof: #4571, 2026-11-23, `tools/cli test --no-build --red LF_Test_LiveryBay` (the owner's press skipped):
// RED-OK vehicles/livery-bay-rows (fails with the hook on: no repaint, no charge, the car keeps its paint).
//! The respray bay's action rows (WO-L9b, #4571): the list a reader sees is the options their job may
//! buy, in order, capped at the prefab's rows; it crosses as texts and numbers and a client row names
//! the paint by index and GUID; a press with a stale GUID, an empty slot, or from a stranger charges
//! nothing; the owner's press at the bay repaints their car on the bay's pad and charges once.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_LiveryBay : LF_Test
{
	protected static const string OWNER = "livery-bay-owner";
	protected static const string STRANGER = "livery-bay-stranger";
	protected static const string CAR = "livery-bay-car";
	protected static const string PLATE = "LV BY 4571";
	protected static const int CIVIL_PRICE = 250;
	protected static const int POLICE_PRICE = 300;
	protected static const ResourceName BAY_PREFAB = "{837750C2B0CBF9D9}Prefabs/Props/LF_LiveryBay.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "vehicles/livery-bay-rows";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<ref LF_VehicleRecord> before = LF_VehicleOwnership.ExportAll();
		LF_VehicleColoursConfig noColours = new LF_VehicleColoursConfig();
		noColours.m_aModels = {};
		LF_VehicleColoursConfig.Use(noColours);
		LF_LiveryShopConfig shop = LF_LiveryShopConfig.Make(150);
		// Option 0: a civilian paint; option 1: a police livery. Any locked prefab stands in for a paint.
		shop.m_aLiveries.Insert(LF_LiveryShopRow.Make(LF_VehicleCatalog.S105, LF_VehicleCatalog.S1203_LOCKED, "#EL-Vehicle_S105", CIVIL_PRICE, LF_Faction.CIVILIAN));
		shop.m_aLiveries.Insert(LF_LiveryShopRow.Make(LF_VehicleCatalog.S105, LF_VehicleCatalog.UAZ469_LOCKED, "#EL-Vehicle_S105", POLICE_PRICE, LF_Faction.POLICE));
		LF_LiveryShopConfig.Use(shop);

		IEntity owner = LF_TestCarriedInventoryKit.SpawnLoaded(ctx);
		IEntity stranger = LF_TestCarriedInventoryKit.SpawnLoaded(ctx);
		if (!owner || !stranger)
		{
			Cleanup(owner, stranger, null, before);
			return;
		}

		LF_VehicleOwnership.Register(CAR, OWNER, CAR, LF_VehicleCatalog.S105, LF_VehicleCatalog.S105_LOCKED, "grid-lot", PLATE, "#EL-Vehicle_S105", true);
		int fee = LF_GarageConfig.ImpoundFee();
		LF_MoneyUtils.AddCash(owner, fee);
		string reasonKey;
		ctx.True(LF_VehicleOwnership.CollectFromImpound(CAR, OWNER, owner, fee, LF_TestAnnex.IMPOUND_PAD_ID, reasonKey), "the owner collects the car");
		IEntity car = LF_VehicleOwnership.GetVehicle(CAR);
		ctx.NotNull(car, "the car stands on the pad");
		if (!car)
		{
			Cleanup(owner, stranger, null, before);
			return;
		}

		// The list a reader sees: the job filters it, the index stays the full list's.
		array<ref LF_LiveryOption> options = {};
		array<int> indexes = {};
		LF_LiveryBay.VisibleOptions(CAR, LF_Faction.CIVILIAN, options, indexes);
		ctx.Equal(1, options.Count(), "a civilian is offered the one civilian paint");
		ctx.True(indexes.Count() == 1 && indexes[0] == 0, "which is the full list's option 0");
		LF_LiveryBay.VisibleOptions(CAR, LF_Faction.POLICE, options, indexes);
		ctx.Equal(2, options.Count(), "an officer is offered both");
		ctx.True(indexes.Count() == 2 && indexes[1] == 1, "the livery keeps its full-list index");

		// The wire form and the client's rows.
		array<string> texts = {};
		array<int> numbers = {};
		LF_LiveryBay.Pack(options, indexes, texts, numbers);
		ctx.Equal(4, texts.Count(), "two texts per paint job");
		ctx.Equal(4, numbers.Count(), "two numbers per paint job");
		LF_LiveryBay.RememberAnswer(texts, numbers, 1000);
		int index;
		string guid;
		string label;
		ctx.True(LF_LiveryBay.RowAt(1, 1500, index, guid, label), "the client's second row exists");
		ctx.Equal(1, index, "and names option 1");
		ctx.EqualStr(LF_VehiclePriceModel.GuidOf(LF_VehicleCatalog.UAZ469_LOCKED), guid, "and the paint's GUID");
		ctx.True(label.Contains(POLICE_PRICE.ToString()), "and its label carries the price");
		ctx.False(LF_LiveryBay.RowAt(2, 1500, index, guid, label), "a third row is empty");
		ctx.False(LF_LiveryBay.RowAt(0, 1000 + LF_LiveryBay.ANSWER_FRESH_MS + 1, index, guid, label), "a stale answer draws no rows");
		LF_LiveryBay.RememberAnswer({}, {}, 2000);
		ctx.True(LF_LiveryBay.AnswerEmpty(2500), "an empty fresh answer shows the hint row");
		LF_LiveryBay.ResetStatics();

		// The bay prop three meters from the car; both players stand beside it.
		vector bayAt = car.GetOrigin() + "3 0 0";
		IEntity bay = LF_Utils.SpawnEntityPrefab(BAY_PREFAB, bayAt);
		ctx.NotNull(bay, "the bay prefab spawns");
		if (!bay)
		{
			Cleanup(owner, stranger, null, before);
			return;
		}

		owner.SetOrigin(bayAt + "0 0 -1.5");
		stranger.SetOrigin(bayAt + "1.5 0 0");
		ctx.True(LF_LiveryBay.BayNear(owner) == bay, "the server finds the bay by the rows it carries");
		LF_MoneyUtils.RemoveCash(owner, LF_MoneyUtils.GetCash(owner));
		LF_MoneyUtils.AddCash(owner, POLICE_PRICE + 1);
		int cash = LF_MoneyUtils.GetCash(owner);
		LF_Utils.SetUidForTests(owner, OWNER);

		LF_LiveryBay.ServerOffer(owner, texts, numbers);
		ctx.Equal(2, texts.Count(), "the owner's offer at the bay is one row (a civilian)");

		// A stale press and an empty slot charge nothing.
		ctx.False(LF_LiveryBay.ServerBuy(owner, 0, "DEADBEEFDEADBEEF"), "a press whose GUID is not the listed paint is refused");
		ctx.False(LF_LiveryBay.ServerBuySlot(owner, 5), "a slot past the list is refused");
		ctx.Equal(cash, LF_MoneyUtils.GetCash(owner), "refusals charge nothing");
		ctx.True(car == LF_VehicleOwnership.GetVehicle(CAR), "and leave the car alone");

		// A stranger beside the bay owns no car there.
		LF_Utils.SetUidForTests(stranger, STRANGER);
		LF_MoneyUtils.AddCash(stranger, 1000);
		int strangerCash = LF_MoneyUtils.GetCash(stranger);
		ctx.False(LF_LiveryBay.ServerBuySlot(stranger, 0), "a stranger cannot press a row for the owner's car");
		ctx.Equal(strangerCash, LF_MoneyUtils.GetCash(stranger), "and is charged nothing");
		LF_Utils.SetUidForTests(owner, OWNER);

		// The owner's press.
		bool done;
		if (!RedProof())
			done = LF_LiveryBay.ServerBuySlot(owner, 0);

		ctx.True(done, "the owner's press repaints the car");
		ctx.Equal(cash - CIVIL_PRICE, LF_MoneyUtils.GetCash(owner), "and charges the price once");
		IEntity painted = LF_VehicleOwnership.GetVehicle(CAR);
		ctx.True(painted && painted != car, "a new car stands in the new paint");
		ctx.True(painted && LF_VehiclePriceModel.SameResource(painted.GetPrefabData().GetPrefabName(), LF_VehicleCatalog.S1203_LOCKED), "the live car is the chosen variant");
		ctx.True(painted && vector.Distance(painted.GetOrigin(), bay.GetOrigin()) <= LF_LiveryShopConfig.Get().m_fBayRadius, "on the bay's pad");
		LF_LiveryBay.VisibleOptions(CAR, LF_Faction.CIVILIAN, options, indexes);
		ctx.Equal(0, options.Count(), "the car's own paint leaves the list");

		Cleanup(owner, stranger, bay, before);
	}

	//------------------------------------------------------------------------------------------------
	protected void Cleanup(IEntity owner, IEntity stranger, IEntity bay, array<ref LF_VehicleRecord> before)
	{
		IEntity key = LF_VehicleOwnership.GetKey(CAR);
		LF_VehicleOwnership.Store(CAR, OWNER);
		LF_Utils.SetUidForTests(null, "");
		LF_TestCharacterFixture.Delete(key);
		LF_TestCharacterFixture.Delete(owner);
		LF_TestCharacterFixture.Delete(stranger);
		if (bay)
			SCR_EntityHelper.DeleteEntityAndChildren(bay);

		LF_VehicleOwnership.ApplyAll(before);
		LF_LicensePlateRegistry.Release(PLATE);
		LF_LiveryBay.ResetStatics();
		LF_LiveryShopConfig.Reload();
		LF_VehicleColoursConfig.Reload();
	}
}
