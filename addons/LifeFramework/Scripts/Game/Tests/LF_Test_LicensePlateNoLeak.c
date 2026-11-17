// red-proof: observed red on the unfixed manager: SetRegistration overwrote the spawn plate without releasing it, so A stayed live after B replaced it.

//! An owned car keeps one plate: the plate EOnInit issued is released when the owner's plate replaces it.
// tier: WORLD
class LF_Test_LicensePlateNoLeak : LF_Test
{
	protected static const ResourceName FIXTURE_PREFAB = "{8821AF528FA28E8C}Prefabs/Vehicles/Core/Vehicle_Test_PlateFixture.et";
	protected static const string PLATE_B = "NL BB 7001";
	protected static const string PLATE_C = "NL CC 7002";

	override string GetName()
	{
		return "licenseplate/no-leak";
	}

	override void Run(LF_TestContext ctx)
	{
		Resource res = Resource.Load(FIXTURE_PREFAB);
		ctx.True(res.IsValid(), "plate fixture prefab loads: " + FIXTURE_PREFAB);
		if (ctx.FailureCount() > 0)
			return;

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.LOCAL;

		IEntity vehicle = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		ctx.True(vehicle != null, "plate fixture spawns a vehicle");
		if (ctx.FailureCount() > 0)
			return;

		LF_LicensePlateManagerComponent plateManager = LF_Component<LF_LicensePlateManagerComponent>.Find(vehicle);
		ctx.NotNull(plateManager, "spawned fixture carries LF_LicensePlateManagerComponent");
		if (ctx.FailureCount() > 0)
			return;

		string plateA = plateManager.m_Registration;
		ctx.False(plateA.IsEmpty(), "the spawn issued plate A");
		ctx.True(LF_LicensePlateRegistry.IsLive(plateA), "plate A is live after the spawn");
		if (ctx.FailureCount() > 0)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(vehicle);
			return;
		}

		// The owner issues its own plate (LF_VehicleOwnership) before it hands it over.
		LF_LicensePlateRegistry.Remember(PLATE_B, "no-leak-owner");
		plateManager.SetRegistration(PLATE_B);
		ctx.EqualStr(PLATE_B, plateManager.m_Registration, "the owner's plate B is on the car");
		ctx.True(LF_LicensePlateRegistry.IsLive(PLATE_B), "plate B is held in the registry");
		ctx.False(LF_LicensePlateRegistry.IsLive(plateA), "plate A is free again after B replaced it");

		// Setting the same plate twice changes nothing.
		plateManager.SetRegistration(PLATE_B);
		ctx.EqualStr(PLATE_B, plateManager.m_Registration, "setting B twice keeps B");
		ctx.True(LF_LicensePlateRegistry.IsLive(PLATE_B), "setting B twice keeps B held");

		// A later swap never releases a plate the manager did not issue.
		LF_LicensePlateRegistry.Remember(PLATE_C, "no-leak-owner");
		plateManager.SetRegistration(PLATE_C);
		ctx.True(LF_LicensePlateRegistry.IsLive(PLATE_B), "plate B, issued by the owner, is left to the owner");

		LF_LicensePlateRegistry.Release(PLATE_B);
		LF_LicensePlateRegistry.Release(PLATE_C);
		SCR_EntityHelper.DeleteEntityAndChildren(vehicle);
	}
};
