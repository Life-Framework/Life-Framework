// red-proof: #2411, 2026-11-17, `tools/cli test --no-build --red LF_Test_VehicleChangeLocks` (ChangeLocks skipped):
// RED-OK vehicles/change-locks-revokes-keys (fails with the hook on, 27 assertions); - the old key no longer opens the car,
// - the saved record carries the new key id, - the respawned car refuses the old key.
// tier: WORLD
//! Changing the locks revokes every key cut before (#2411): the owner pays, the old key stops
//! opening the car (in the world and after a garage round trip), the new key opens it, a stranger
//! cannot do it, and the vehicle identifier that plates and records use stays the same.
class LF_Test_VehicleChangeLocks : LF_Test
{
	protected static const string OWNER = "change-locks-owner";
	protected static const string STREET = "change-locks-street";
	protected static const string PLATE = "CL KS 2411";
	protected static const int PRICE = 150;
	protected static const int CEILING = 10000;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "vehicles/change-locks-revokes-keys";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<ref LF_VehicleRecord> before = LF_VehicleOwnership.ExportAll();
		IEntity owner = LF_TestCarriedInventoryKit.SpawnLoaded(ctx);
		if (!owner)
			return;

		LF_VehicleOwnership.Register(STREET, OWNER, STREET, LF_VehicleCatalog.S105, LF_VehicleCatalog.S105_LOCKED, "grid-lot", PLATE, "#EL-Vehicle_S105", true);
		int fee = LF_GarageConfig.ImpoundFee();
		LF_MoneyUtils.AddCash(owner, fee + 2 * PRICE);
		string reasonKey;
		ctx.True(LF_VehicleOwnership.CollectFromImpound(STREET, OWNER, owner, fee, LF_TestAnnex.IMPOUND_PAD_ID, reasonKey), "the owner collects the car");
		IEntity car = LF_VehicleOwnership.GetVehicle(STREET);
		IEntity oldKey = LF_VehicleOwnership.GetKey(STREET);
		LF_VehicleLockComponent vehicleLock;
		if (car)
			vehicleLock = LF_Component<LF_VehicleLockComponent>.Find(car);

		ctx.True(vehicleLock && oldKey && vehicleLock.IsValidKey(oldKey), "the first key opens the car");
		if (!vehicleLock || !oldKey)
		{
			Cleanup(owner, oldKey, null, before);
			return;
		}

		int cashCharged;
		ctx.False(LF_VehicleOwnership.ChangeLocks(STREET, "change-locks-stranger", PRICE, CEILING, owner, owner.GetOrigin(), cashCharged, reasonKey), "a stranger cannot change the locks");
		ctx.EqualStr("#EL-Vehicle_NotOwner", reasonKey, "the stranger is told they do not own it");
		ctx.True(vehicleLock.IsValidKey(oldKey), "a refused change leaves the key working");

		int cashBefore = LF_MoneyUtils.GetCash(owner);
		if (!RedProof())
			ctx.True(LF_VehicleOwnership.ChangeLocks(STREET, OWNER, PRICE, CEILING, owner, owner.GetOrigin(), cashCharged, reasonKey), "the owner changes the locks");

		IEntity newKey = LF_VehicleOwnership.GetKey(STREET);
		ctx.Equal(cashBefore - PRICE, LF_MoneyUtils.GetCash(owner), "the locksmith is paid from carried cash");
		ctx.False(vehicleLock.IsValidKey(oldKey), "the old key no longer opens the car");
		ctx.True(newKey && newKey != oldKey && vehicleLock.IsValidKey(newKey), "the new key opens the car");
		ctx.True(LF_VehicleOwnership.HoldsKey(owner, vehicleLock.GetKeyId()), "the owner carries the new key");
		ctx.EqualStr(STREET, vehicleLock.GetVehicleIdentifier(), "the vehicle identifier is unchanged");
		ctx.True(LF_VehicleOwnership.IsOwnedEntity(car), "the car is still the owner's saved car");

		LF_VehicleRecord saved = LF_VehicleOwnership.Export(STREET);
		ctx.True(saved && saved.m_sKeyId == vehicleLock.GetKeyId() && saved.m_sKeyId != STREET, "the saved record carries the new key id");

		// The garage round trip spawns a fresh car from the record: the old key stays dead.
		ctx.True(LF_VehicleOwnership.Store(STREET, OWNER), "the owner garages the car");
		ctx.True(LF_VehicleOwnership.Retrieve(STREET, OWNER, LF_TestAnnex.GARAGE_PAD_ID, owner), "the owner retrieves it");
		IEntity respawned = LF_VehicleOwnership.GetVehicle(STREET);
		LF_VehicleLockComponent respawnedLock;
		if (respawned)
			respawnedLock = LF_Component<LF_VehicleLockComponent>.Find(respawned);

		ctx.True(respawnedLock && !respawnedLock.IsValidKey(oldKey), "the respawned car refuses the old key");
		ctx.True(respawnedLock && respawnedLock.IsValidKey(newKey), "the respawned car opens with the new key");
		ctx.True(newKey == LF_VehicleOwnership.GetKey(STREET), "the owner holding the new key is not handed another");

		LF_VehicleOwnership.Store(STREET, OWNER);
		Cleanup(owner, oldKey, newKey, before);
	}

	//------------------------------------------------------------------------------------------------
	protected void Cleanup(IEntity owner, IEntity oldKey, IEntity newKey, array<ref LF_VehicleRecord> before)
	{
		LF_TestCharacterFixture.Delete(oldKey);
		if (newKey != oldKey)
			LF_TestCharacterFixture.Delete(newKey);

		LF_TestCharacterFixture.Delete(owner);
		LF_VehicleOwnership.ApplyAll(before);
		LF_LicensePlateRegistry.Release(PLATE);
	}
}
