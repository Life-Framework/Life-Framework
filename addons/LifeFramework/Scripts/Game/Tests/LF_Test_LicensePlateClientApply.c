// red-proof: RED-OK 2026-11-17 `tools/cli test --no-build --red LF_Test_LicensePlateClientApply`: with the hook on (no OnRegistrationUpdated call) the plate record read "" instead of "RPL 2475".

// #2475: a registration that arrives through the replication callback (OnRegistrationUpdated, the RplProp
// onRpl handler) is applied to every plate the manager spawned. The applied text lives on the manager's plate
// record so it is readable without a text widget (dedicated server).
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_LicensePlateClientApply : LF_Test
{
	protected static const ResourceName FIXTURE_PREFAB = "{8821AF528FA28E8C}Prefabs/Vehicles/Core/Vehicle_Test_PlateFixture.et";
	protected static const string REPLICATED_PLATE = "RPL 2475";

	override string GetName()
	{
		return "licenseplate/client-apply";
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

		// What the replication layer does on a client: write the RplProp, then call its onRpl handler.
		plateManager.m_Registration = REPLICATED_PLATE;
		if (!RedProof())
			plateManager.OnRegistrationUpdated();

		int spawned;
		foreach (LF_LicensePlatePointInfo plate : plateManager.m_Plates)
		{
			if (!plate.m_Object)
				continue;
			spawned++;
			ctx.EqualStr(REPLICATED_PLATE, plate.GetAppliedText(), "plate record shows the replicated registration");
		}
		ctx.True(spawned > 0, "the manager spawned at least one plate");

		delete vehicle;
	}
};
