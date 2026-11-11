// SPIKE-A (throwaway branch ws/m3-spike-a, never merged): logs how moved resources resolve.
// red-proof: none, a probe that only logs; it asserts nothing that the move could break.

// tier: PERSISTENCE
class LF_Test_SpikePackMove : LF_Test
{
	protected static const string F = "SpikePack";
	protected static const vector LOOSE_CAR_AT = "40 0.5 40";
	protected static ref array<IEntity> s_aFound = {};

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "spike/pack-move-probe";
	}

	//------------------------------------------------------------------------------------------------
	protected void Probe(string label, ResourceName name)
	{
		Resource res = Resource.Load(name);
		bool valid = res && res.IsValid();
		LF_Debug.Info(F, string.Format("load %1 '%2' valid=%3 GetPath='%4'", label, name, valid, name.GetPath()));
	}

	//------------------------------------------------------------------------------------------------
	protected void Named(string entityName)
	{
		IEntity e = GetGame().GetWorld().FindEntityByName(entityName);
		if (!e)
		{
			LF_Debug.Info(F, string.Format("named %1 found=0", entityName));
			return;
		}
		LF_Debug.Info(F, string.Format("named %1 found=1 prefab='%2' at %3", entityName, LF_Utils.GetPrefabName(e), e.GetOrigin()));
	}

	//------------------------------------------------------------------------------------------------
	protected static bool Collect(IEntity e)
	{
		s_aFound.Insert(e);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		string carOld = "{E1A105BE1A000001}Prefabs/Vehicles/Wheeled/S105/EL_S105_beige.et";
		string carNew = "{E1A105BE1A000001}Prefabs/Everon1989/Vehicles/EL_S105_beige.et";
		string signOld = "{D8D55097BA6DB899}Prefabs/Props/Signs/EL_MiningSign.et";
		string signNew = "{D8D55097BA6DB899}Prefabs/Everon1989/Signs/EL_MiningSign.et";
		Probe("car-old-path", carOld);
		Probe("car-new-path", carNew);
		Probe("car-guid-only", "{E1A105BE1A000001}");
		Probe("car-old-path-no-guid", "Prefabs/Vehicles/Wheeled/S105/EL_S105_beige.et");
		Probe("car-new-path-no-guid", "Prefabs/Everon1989/Vehicles/EL_S105_beige.et");
		Probe("car-wrong-guid-old-path", "{0000000000000001}Prefabs/Vehicles/Wheeled/S105/EL_S105_beige.et");
		Probe("sign-old-path", signOld);
		Probe("sign-new-path", signNew);
		Probe("header-old-path", "{1054CC8646357708}Missions/LF_PlaytestTown.conf");
		Probe("catalog-S105_LOCKED", LF_VehicleCatalog.S105_LOCKED);

		IEntity car = LF_Utils.SpawnEntityPrefab(carOld, "30 0.5 30");
		string spawned = LF_Utils.GetPrefabName(car);
		LF_Debug.Info(F, string.Format("spawn from old path ok=%1 GetPrefabName='%2' equalsOld=%3 equalsNew=%4 equalsCatalog=%5", car != null, spawned, spawned == carOld, spawned == carNew, spawned == LF_VehicleCatalog.S105_LOCKED));
		if (car)
			SCR_EntityHelper.DeleteEntityAndChildren(car);

		Named("Roadmap_Vehicle_S105");
		Named("Roadmap_Lockpick_Car");
		Named("LF_MiningSign_Debug");
		Named("Village_Payphone");
		Named("RoadmapStreet");

		s_aFound.Clear();
		GetGame().GetWorld().QueryEntitiesBySphere(LOOSE_CAR_AT, 8, Collect, null, EQueryEntitiesFlags.ALL);
		int cars;
		foreach (IEntity e : s_aFound)
		{
			string prefab = LF_Utils.GetPrefabName(e);
			if (prefab.Contains("E1A105BE1A000001"))
			{
				cars++;
				LF_Debug.Info(F, string.Format("loose car near %1: prefab='%2' at %3", LOOSE_CAR_AT, prefab, e.GetOrigin()));
			}
		}
		LF_Debug.Info(F, string.Format("loose cars near %1: %2", LOOSE_CAR_AT, cars));

#ifndef LF_E2E_LOAD_BOOT
		if (cars == 0)
		{
			IEntity loose = LF_Utils.SpawnEntityPrefab(carOld, LOOSE_CAR_AT);
			LF_Debug.Info(F, string.Format("pass 1 left a loose S105 for the engine save: %1", loose != null));
		}
#endif
		ctx.True(true, "spike probe ran");
	}
}
