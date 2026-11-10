// red-proof: KeySpawnOrigin returned the pad origin, so the key was created at the pad, outside the collecting player's replication range (a client could not resolve it; key and ID card moves hung on the throbber)
// tier: WORLD
//! A collected or bought vehicle key is created at its holder, never at the pad it was collected
//! from, and lands in the holder's inventory (maintainer playtest 2026-11-10: after an impound
//! collect, moving the key or the ID card stuck on the loading throbber).
class LF_Test_VehicleKeyInventory : LF_Test
{
	protected static const string OWNER = "key-inventory-owner";
	protected static const string STREET = "key-inventory-street";
	protected static const string PLATE = "KI NV 8008";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "vehicles/key-inventory";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		vector pad = "5000 0 5000";
		IEntity owner = LF_TestCarriedInventoryKit.SpawnLoaded(ctx);
		if (!owner)
			return;

		vector at = LF_VehicleOwnership.KeySpawnOrigin(owner, pad);
		ctx.True(vector.Distance(at, owner.GetOrigin()) < 0.01, "the key is created at the holder, not at the pad");
		ctx.True(vector.Distance(LF_VehicleOwnership.KeySpawnOrigin(null, pad), pad) < 0.01, "with no holder the key stays at the pad");

		int fee = LF_GarageConfig.ImpoundFee();
		array<ref LF_VehicleRecord> before = LF_VehicleOwnership.ExportAll();
		LF_VehicleOwnership.Register(STREET, OWNER, STREET, LF_VehicleCatalog.S105, LF_VehicleCatalog.S105_LOCKED, "roadmap-lot", PLATE, "#EL-Vehicle_S105", true);
		LF_MoneyUtils.AddCash(owner, fee);
		string reasonKey;
		ctx.True(LF_VehicleOwnership.CollectFromImpound(STREET, OWNER, owner, fee, "roadmap-impound", reasonKey), "the owner collects from the impound desk");
		IEntity key = LF_VehicleOwnership.GetKey(STREET);
		ctx.True(key != null, "a key exists for the collected car");

		SCR_InventoryStorageManagerComponent manager = LF_Component<SCR_InventoryStorageManagerComponent>.Find(owner);
		array<IEntity> items = {};
		if (manager)
			manager.GetItems(items);
		ctx.True(key != null && items.Contains(key), "the collected key is in the owner's inventory");

		LF_VehicleOwnership.Store(STREET, OWNER);
		if (key)
			LF_TestCharacterFixture.Delete(key);

		LF_TestCharacterFixture.Delete(owner);
		LF_VehicleOwnership.ApplyAll(before);
		LF_LicensePlateRegistry.Release(PLATE);
	}
}
