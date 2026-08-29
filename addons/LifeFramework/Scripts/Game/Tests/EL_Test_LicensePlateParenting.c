// red-proof: re-add the veh.AddChild(...) line in EL_LicensePlateManagerComponent.c
// (after the plate spawn) and run `tools\cli test --tier all`; the plate is already
// parented at spawn via params.Parent, so the second attach with
// RECALC_LOCAL_TRANSFORM breaks the spawn-time local offset and the
// parent/local-offset assertions fail.

// tier: WORLD
class EL_Test_LicensePlateParenting : EL_Test
{
	protected static const ResourceName FIXTURE_PREFAB = "{8821AF528FA28E8C}Prefabs/Vehicles/Core/Vehicle_Test_PlateFixture.et";

	override string GetName()
	{
		return "licenseplate/parenting";
	}

	override void Run(EL_TestContext ctx)
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

		EL_LicensePlateManagerComponent plateManager = EL_Component<EL_LicensePlateManagerComponent>.Find(vehicle);
		ctx.NotNull(plateManager, "spawned fixture carries EL_LicensePlateManagerComponent");
		if (ctx.FailureCount() > 0)
			return;

		ctx.Equal(1, plateManager.m_Plates.Count(), "fixture has exactly one configured plate point");

		EL_LicensePlateEntity plate = plateManager.m_Plates[0].m_Object;
		ctx.NotNull(plate, "plate entity spawned under the manager");
		if (ctx.FailureCount() > 0)
			return;

		ctx.True(plate.GetParent() == vehicle, "plate parent is the vehicle");

		vector localMat[4];
		plate.GetLocalTransform(localMat);
		ctx.EqualFloat(0.3, localMat[3][0], 0.01, "plate local offset X matches the configured point");
		ctx.EqualFloat(1.2, localMat[3][1], 0.01, "plate local offset Y matches the configured point");
		ctx.EqualFloat(0.5, localMat[3][2], 0.01, "plate local offset Z matches the configured point");
	}
};