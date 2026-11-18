// red-proof: `tools\cli test --no-build --red LF_Test_LicensePlateLayout`: with the hook on the layout name is empty and the load fails.

// #2476: the plate draws its text through LicensePlate.layout (RTTexture0 + Text0) on the client. The server has no
// renderer, so the draw itself is a session picture; this test guards what a headless run can: the spawned
// plate names a layout that resolves, and spawning the plate builds nothing that can throw here.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_LicensePlateLayout : LF_Test
{
	protected static const ResourceName FIXTURE_PREFAB = "{8821AF528FA28E8C}Prefabs/Vehicles/Core/Vehicle_Test_PlateFixture.et";

	override string GetName()
	{
		return "licenseplate/layout";
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
		{
			delete vehicle;
			return;
		}

		LF_LicensePlateEntity plate = plateManager.m_Plates[0].m_Object;
		ctx.NotNull(plate, "plate entity spawned under the manager");
		if (ctx.FailureCount() > 0)
		{
			delete vehicle;
			return;
		}

		ResourceName layout;
		if (!RedProof())
			layout = plate.GetLayout();

		ctx.False(layout.IsEmpty(), "the plate names its text layout");
		Resource layoutRes = Resource.Load(layout);
		ctx.True(layoutRes && layoutRes.IsValid(), "the plate text layout resolves: " + layout);

		delete vehicle;
	}
};
