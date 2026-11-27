// red-proof: `tools\cli test --no-build --only LF_Test_CivilianCarPlates` on the unfixed prefabs (no plate points on any civilian car) failed every "carries a front and a rear plate point" and "spawned its plate entities" line; green with the plate points. `--red` skips the plate count read.

//! #5129: a car a player can own draws its registration. Only the police UAZ had plate points, so every other
//! civilian car had a registration (records, run-plate and witnesses used it) and no plate to read it on. Every
//! prefab the car lot, the truck yard and the colour and livery builds spawn must carry a front and a rear point,
//! and spawning it must put a plate entity on each. Pack variants inherit these, so one family is one prefab here.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_CivilianCarPlates : LF_Test
{
	protected static const ResourceName Z7211 = "{E1A7211BE1A00001}Prefabs/Vehicles/Wheeled/Z7211/EL_Z7211_red_CIV.et";
	protected static const ResourceName URAL = "{2B402516452EC797}Prefabs/Vehicles/Wheeled/Ural4320/LF_Ural4320_transport_CIV.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "licenseplate/civilian-cars";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<ResourceName> cars = {};
		cars.Insert(LF_VehicleCatalog.S105_LOCKED);
		cars.Insert(LF_VehicleCatalog.UAZ469_LOCKED);
		cars.Insert(LF_VehicleCatalog.S1203_LOCKED);
		cars.Insert(LF_VehicleCatalog.UAZ452_LOCKED);
		cars.Insert(LF_VehicleCatalog.M923_LOCKED);
		cars.Insert(LF_VehicleCatalog.DANA2101_LOCKED);
		cars.Insert(LF_VehicleCatalog.SPUTNIK_T601_LOCKED);
		cars.Insert(Z7211);
		cars.Insert(URAL);
		foreach (ResourceName prefab : cars)
		{
			CheckCar(ctx, prefab);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckCar(LF_TestContext ctx, ResourceName prefab)
	{
		Resource res = Resource.Load(prefab);
		ctx.True(res && res.IsValid(), "car prefab loads: " + prefab);
		if (!res || !res.IsValid())
			return;

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.LOCAL;
		IEntity vehicle = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		ctx.True(vehicle != null, "car spawns: " + prefab);
		if (!vehicle)
			return;

		LF_LicensePlateManagerComponent manager = LF_Component<LF_LicensePlateManagerComponent>.Find(vehicle);
		ctx.NotNull(manager, "car carries a plate manager: " + prefab);
		if (manager)
		{
			int points;
			if (!RedProof() && manager.m_Plates)
				points = manager.m_Plates.Count();
			ctx.True(points >= 2, "carries a front and a rear plate point (" + points + "): " + prefab);

			int spawned;
			if (manager.m_Plates)
			{
				foreach (LF_LicensePlatePointInfo plate : manager.m_Plates)
				{
					if (plate && plate.m_Object)
						spawned++;
				}
			}
			ctx.Equal(points, spawned, "spawned its plate entities: " + prefab);
			ctx.False(manager.m_Registration.IsEmpty(), "car holds a registration: " + prefab);
			if (!manager.m_Registration.IsEmpty())
				LF_LicensePlateRegistry.Release(manager.m_Registration);
		}

		SCR_EntityHelper.DeleteEntityAndChildren(vehicle);
	}
};
