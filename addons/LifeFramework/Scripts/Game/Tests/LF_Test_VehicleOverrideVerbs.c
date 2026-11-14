// red-proof: observed 2026-11-14 with `tools\cli test --no-build --only LF_Test_VehicleOverrideVerbs`: making LF_VehiclePriceList.RemoveAt a no-op failed `- remove: one vehicle left (expected 1, got 2)` and `- remove: a removed vehicle is unpriced, not free (expected -1, got 1000)`; dropping the base-price check in Incomplete failed `- add: only the priced new vehicle is added (expected 3, got 4)`. Restored; green.
// tier: LOGIC
//! #1109: the vehicle price list on the override model (#962). `op: remove` takes a vehicle out of
//! the list, `op: patch` of an id no lower layer has is refused (nothing added), `replace` rebuilds
//! the entry in its place from the class defaults, `add` of a listed vehicle (by its lot prefab or
//! its locked child's GUID) is refused, and a new vehicle still needs a base price.
class LF_Test_VehicleOverrideVerbs : LF_Test
{
	override string GetName()
	{
		return "config/vehicle-override-verbs";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_VehiclePricesConfig MakeCore()
	{
		LF_VehiclePricesConfig config = new LF_VehiclePricesConfig();
		config.m_iBuyPercent = 100;
		config.m_aVehicles = {};
		config.m_aVehicles.Insert(LF_VehiclePriceEntry.Make(LF_VehicleCatalog.S105, LF_VehicleCatalog.S105_LOCKED, 1000));
		LF_VehiclePriceEntry uaz = LF_VehiclePriceEntry.Make(LF_VehicleCatalog.UAZ469, LF_VehicleCatalog.UAZ469_LOCKED, 2500);
		uaz.m_iImpoundOverride = 250;
		config.m_aVehicles.Insert(uaz);
		return config;
	}

	//------------------------------------------------------------------------------------------------
	protected LF_ConfigReader Layer(string vehiclesJson)
	{
		LF_ConfigService.Reset();
		LF_ConfigService.LoadOwnerString("{\"schemaVersion\":1,\"vehicles\":{\"vehicles\":" + vehiclesJson + "}}", "test");
		return LF_ConfigService.OpenOwnerSection("vehicles");
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		string s105 = LF_VehiclePriceModel.GuidOf(LF_VehicleCatalog.S105);
		string s105Locked = LF_VehiclePriceModel.GuidOf(LF_VehicleCatalog.S105_LOCKED);
		string uaz = LF_VehiclePriceModel.GuidOf(LF_VehicleCatalog.UAZ469);

		// remove: the vehicle leaves the list, addressed by the locked child's GUID.
		LF_VehiclePricesConfig config = MakeCore();
		LF_ConfigReader reader = Layer("{\"" + s105Locked + "\":{\"op\":\"remove\"}}");
		ctx.NotNull(reader, "the vehicles section opens");
		if (!reader)
			return;

		config.ApplyOwner(reader, "test-owner.json");
		ctx.Equal(1, config.m_aVehicles.Count(), "remove: one vehicle left");
		ctx.True(config.Find(LF_VehicleCatalog.S105) == null, "remove: the S105 is out of the list");
		ctx.Equal(-1, config.PriceFor(LF_EVehiclePriceAction.BUY, LF_VehicleCatalog.S105), "remove: a removed vehicle is unpriced, not free");
		ctx.Equal(0, reader.GetRejectedCount(), "remove: nothing refused");

		// patch of a missing id is refused and adds nothing, even with a base price.
		config = MakeCore();
		reader = Layer("{\"AAAABBBBCCCCDDDD\":{\"op\":\"patch\",\"basePrice\":777}}");
		config.ApplyOwner(reader, "test-owner.json");
		ctx.Equal(2, config.m_aVehicles.Count(), "patch of a missing id adds nothing");
		ctx.Equal(-1, config.PriceFor(LF_EVehiclePriceAction.BUY, "{AAAABBBBCCCCDDDD}"), "patch of a missing id: still unpriced");
		ctx.Equal(1, reader.GetRejectedCount(), "patch of a missing id: one record refused");

		// remove of a missing id is refused too.
		config = MakeCore();
		reader = Layer("{\"AAAABBBBCCCCDDDD\":{\"op\":\"remove\"}}");
		config.ApplyOwner(reader, "test-owner.json");
		ctx.Equal(2, config.m_aVehicles.Count(), "remove of a missing id changes nothing");
		ctx.Equal(1, reader.GetRejectedCount(), "remove of a missing id: one record refused");

		// patch changes only the fields given; replace rebuilds from the defaults in the same place.
		config = MakeCore();
		reader = Layer("{\"" + s105 + "\":{\"op\":\"patch\",\"basePrice\":1500},\"" + uaz + "\":{\"op\":\"replace\",\"basePrice\":3000}}");
		config.ApplyOwner(reader, "test-owner.json");
		ctx.Equal(1500, config.PriceFor(LF_EVehiclePriceAction.BUY, LF_VehicleCatalog.S105), "patch: the field given applies");
		LF_VehiclePriceEntry patchedEntry = config.Find(LF_VehicleCatalog.S105);
		ctx.NotNull(patchedEntry, "patch: the S105 is still listed");
		if (patchedEntry)
			ctx.EqualStr(LF_VehicleCatalog.S105_LOCKED, patchedEntry.m_SpawnPrefab, "patch: a field left out keeps the lower layer's value");
		LF_VehiclePriceEntry replaced = config.Find(LF_VehicleCatalog.UAZ469);
		ctx.NotNull(replaced, "replace: the vehicle is still listed");
		if (replaced)
			ctx.True(replaced.m_SpawnPrefab.IsEmpty(), "replace: the core locked child is gone, the entry is rebuilt from the layer alone");
		ctx.Equal(3000, config.PriceFor(LF_EVehiclePriceAction.BUY, "{" + uaz + "}"), "replace: the new base price applies");
		ctx.Equal(-1, config.m_aVehicles[1].m_iImpoundOverride, "replace: the core impound override is gone");
		ctx.EqualStr(uaz, LF_VehiclePriceModel.GuidOf(config.m_aVehicles[1].m_Prefab), "replace: the entry keeps its place in the list");
		ctx.Equal(2, config.m_aVehicles.Count(), "replace adds no entry");

		// add of a listed vehicle is refused; add of a new vehicle needs a base price.
		config = MakeCore();
		reader = Layer("{\"" + s105Locked + "\":{\"op\":\"add\",\"basePrice\":5},\"1111222233334444\":{\"op\":\"add\",\"buyOverride\":5},\"AAAABBBBCCCCDDDD\":{\"op\":\"add\",\"basePrice\":777}}");
		config.ApplyOwner(reader, "test-owner.json");
		ctx.Equal(3, config.m_aVehicles.Count(), "add: only the priced new vehicle is added");
		ctx.Equal(1000, config.PriceFor(LF_EVehiclePriceAction.BUY, LF_VehicleCatalog.S105), "add of a listed vehicle leaves its price");
		ctx.Equal(777, config.PriceFor(LF_EVehiclePriceAction.BUY, "{AAAABBBBCCCCDDDD}"), "add: the new vehicle is priced");
		ctx.Equal(2, reader.GetRejectedCount(), "add: the listed vehicle and the unpriced one are refused");

		// A bad verb is refused.
		config = MakeCore();
		reader = Layer("{\"" + s105 + "\":{\"op\":\"delete\",\"basePrice\":1}}");
		config.ApplyOwner(reader, "test-owner.json");
		ctx.Equal(1000, config.PriceFor(LF_EVehiclePriceAction.BUY, LF_VehicleCatalog.S105), "an unknown verb changes nothing");

		LF_ConfigService.Reset();
	}
}
