// red-proof: RedProof() skips the config load, so `the pack overlay added its three period cars` fails (run `tools\cli test --no-build --red LF_Test_EveronParkedCars`).

//! #3532 in game: under the Everon 1989 header (the pack leg of `tools\cli test`), the pack's parked-car overlay
//! adds three period civilian cars (Skoda 105, covered UAZ 469, Zil-style Z7211) to the core's two, and every car in
//! the merged list is a thin locked child: it resolves and carries LF_VehicleLockComponent.
[LF_TestInfo(LF_TestTier.LOGIC, pack: true)]
class LF_Test_EveronParkedCars : LF_Test
{
	protected static const string S105 = "E1A105BE1A000001";
	protected static const string UAZ469 = "E1A469BE1A000001";
	protected static const string Z7211 = "E1A7211BE1A00001";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/parked-cars";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ParkedCarConfig.Reload();
		LF_ParkedCarConfig config;
		if (!RedProof())
			config = LF_ParkedCarConfig.Get();

		ctx.True(config != null, "the parked car config loads");
		if (!config)
			return;

		ctx.True(config.m_aCars.Count() >= 5, "the pack overlay added its three period cars to the core's two");
		ctx.True(HasCar(config, S105), "the Skoda 105 is in the list");
		ctx.True(HasCar(config, UAZ469), "the covered UAZ 469 is in the list");
		ctx.True(HasCar(config, Z7211), "the Z7211 is in the list");

		foreach (LF_ParkedCarDef def : config.m_aCars)
		{
			Resource resource = Resource.Load(def.m_sPrefab);
			ctx.True(resource != null && resource.IsValid(), string.Format("%1 resolves", def.m_sPrefab));
			if (!resource || !resource.IsValid())
				continue;

			IEntitySource source = resource.GetResource().ToEntitySource();
			bool locked = false;
			if (source)
			{
				for (int i = 0, count = source.GetComponentCount(); i < count; i++)
				{
					if (source.GetComponent(i).GetClassName() == "LF_VehicleLockComponent")
						locked = true;
				}
			}

			ctx.True(locked, string.Format("%1 carries the lock", def.m_sPrefab));
		}

		LF_ParkedCarConfig.Reload();
	}

	//------------------------------------------------------------------------------------------------
	protected static bool HasCar(LF_ParkedCarConfig config, string guid)
	{
		foreach (LF_ParkedCarDef def : config.m_aCars)
		{
			if (LF_ParkedCarList.CarId(def.m_sPrefab) == guid)
				return true;
		}

		return false;
	}
}
