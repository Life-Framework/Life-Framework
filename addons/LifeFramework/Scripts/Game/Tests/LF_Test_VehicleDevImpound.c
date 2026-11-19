// red-proof: 2026-11-19 `tools\cli test --no-build --red LF_Test_VehicleDevImpound`: RED PROOF OK (RedProof() skips the take): `a second take finds no car in the world (expected '', got 'dev-impound-car')`.
//! The dev impound (#3491) puts the owner's newest car at the impound lot, and the desk collects it: the
//! car leaves the world, reads as impounded, a second take finds no car, and a collect brings it back.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_VehicleDevImpound : LF_Test
{
	protected static const string OWNER = "dev-impound-owner";
	protected static const string VEHICLE = "dev-impound-car";
	protected static const string PLATE = "DI MP 3491";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "vehicles/dev-impound";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<ref LF_VehicleRecord> before = LF_VehicleOwnership.ExportAll();
		array<ref LF_StorageSnapshotRecord> snapshotsBefore = LF_StorageSnapshot.ExportAll();
		IEntity owner = LF_TestCarriedInventoryKit.SpawnLoaded(ctx);
		if (!owner)
			return;

		LF_VehicleOwnership.Register(VEHICLE, OWNER, VEHICLE, LF_VehicleCatalog.S105, LF_VehicleCatalog.S105_LOCKED, "grid-lot", PLATE, "#EL-Vehicle_S105", true);
		int fee = LF_GarageConfig.ImpoundFee();
		LF_MoneyUtils.AddCash(owner, fee);
		string reasonKey;
		ctx.True(LF_VehicleOwnership.CollectFromImpound(VEHICLE, OWNER, owner, fee, LF_TestAnnex.IMPOUND_PAD_ID, reasonKey), "fixture: the car is in the world");
		ctx.True(LF_VehicleOwnership.GetVehicle(VEHICLE) != null, "fixture: the car has an entity");
		ctx.False(LF_VehicleOwnership.IsImpounded(VEHICLE), "the car in the world is not impounded");

		string taken = "";
		if (!RedProof())
			taken = LF_VehicleDevImpound.TakeLatest(OWNER);

		ctx.EqualStr(VEHICLE, taken, "the dev impound names the owner's car");
		ctx.True(LF_VehicleOwnership.IsImpounded(VEHICLE), "the car is at the impound lot");
		ctx.True(LF_VehicleOwnership.GetVehicle(VEHICLE) == null, "the car left the world");
		ctx.EqualStr("", LF_VehicleDevImpound.TakeLatest(OWNER), "a second take finds no car in the world");
		ctx.EqualStr("", LF_VehicleDevImpound.TakeLatest("dev-impound-stranger"), "a stranger's take touches nothing");

		LF_MoneyUtils.AddCash(owner, fee);
		ctx.True(LF_VehicleOwnership.CollectFromImpound(VEHICLE, OWNER, owner, fee, LF_TestAnnex.IMPOUND_PAD_ID, reasonKey), "the desk collects the impounded car");
		ctx.True(LF_VehicleOwnership.GetVehicle(VEHICLE) != null, "the collected car is back in the world");

		LF_TestCharacterFixture.Delete(LF_VehicleOwnership.GetKey(VEHICLE));
		LF_TestCharacterFixture.Delete(LF_VehicleOwnership.GetVehicle(VEHICLE));
		LF_TestCharacterFixture.Delete(owner);
		LF_VehicleOwnership.ApplyAll(before);
		LF_StorageSnapshot.ApplyAll(snapshotsBefore);
		LF_LicensePlateRegistry.Release(PLATE);
	}
}
