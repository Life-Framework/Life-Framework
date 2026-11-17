// red-proof: 2026-11-17 `tools/cli test --no-build --red LF_Test_DevGameMasterReset,LF_Test_CharacterCreationReset,LF_Test_VehicleStateReset,LF_Test_WhitelistStaticReset,LF_Test_OrgStaticReset,LF_Test_UiSheetStaticReset`: RED PROOF OK (all 6 fail with the hook on, e.g. "a world end clears the missing-file counter (expected 0, got 1)"); green with the hooks (--only, 6 passed).
// tier: LOGIC
//! vehicle/state-reset (#2415): vehicle records, sessions, the plate registry and the garage config
//! are process statics. A second world loaded in the same process kept the last world's cars and
//! plates, so a fresh save listed vehicles nobody had bought and refused plates as taken.
class LF_Test_VehicleStateReset : LF_Test
{
	protected static const string VEHICLE = "vehicle-state-reset-car";
	protected static const string PLATE = "RST 2415";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "vehicle/state-reset";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<ref LF_VehicleRecord> saved = LF_VehicleOwnership.ExportAll();

		ctx.True(LF_VehicleOwnership.Register(VEHICLE, "reset-owner", "reset-key", "reset-prefab", "reset-spawn", "reset-shop", PLATE, "#reset", false), "a vehicle is registered");
		ctx.True(LF_VehicleOwnership.Export(VEHICLE) != null, "the vehicle record is live");
		ctx.True(LF_LicensePlateRegistry.IsLive(PLATE), "the plate is live");

		LF_GarageConfig garageBefore = LF_GarageConfig.Get();
		ctx.True(garageBefore != null, "the garage config resolves");

		if (!RedProof())
		{
			LF_StaticResetHook hook1 = new LF_VehicleOwnership_StaticResetHook();
			hook1.Apply();
			LF_StaticResetHook hook2 = new LF_LicensePlateRegistry_StaticResetHook();
			hook2.Apply();
			LF_StaticResetHook hook3 = new LF_GarageConfig_StaticResetHook();
			hook3.Apply();
		}

		ctx.True(LF_VehicleOwnership.Export(VEHICLE) == null, "a world end drops the vehicle record");
		ctx.Equal(0, LF_VehicleOwnership.ExportAll().Count(), "a world end drops every vehicle record");
		ctx.False(LF_LicensePlateRegistry.IsLive(PLATE), "a world end drops the live plates");
		LF_GarageConfig garageAfter = LF_GarageConfig.Get();
		ctx.True(garageAfter != garageBefore, "a world end drops the cached garage config");

		// Put the test world back as it was.
		LF_VehicleOwnership.ApplyAll(saved);
	}
}
