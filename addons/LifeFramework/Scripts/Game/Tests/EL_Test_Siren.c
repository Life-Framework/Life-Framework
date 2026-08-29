// red-proof: point the police prefab's EL_SirenManagerComponent.m_Modes at a
// nonexistent conf (or remove the manager component from M151A2_Police_Base.et) and
// run `tools\cli test --tier all`; the prefab load / spawn / manager assertions fail.

// tier: WORLD
class EL_Test_Siren : EL_Test
{
	protected static const ResourceName POLICE_PREFAB = "{C03B70B228FC49BA}Prefabs/Vehicles/Wheeled/M151A2/M151A2_Police_Base.et";
	protected static const ResourceName MODES_CONF = "{01051CFE4BC64945}Prefabs/Vehicles/SirenLights/LightBarLED.conf";

	override string GetName()
	{
		return "siren/police-vehicle-modes";
	}

	override void Run(EL_TestContext ctx)
	{
		Resource res = Resource.Load(POLICE_PREFAB);
		ctx.True(res.IsValid(), "police M151A2 prefab loads: " + POLICE_PREFAB);
		if (ctx.FailureCount() > 0)
			return;

		Resource modesRes = Resource.Load(MODES_CONF);
		ctx.True(modesRes.IsValid(), "siren mode conf loads: " + MODES_CONF);

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.LOCAL;

		IEntity police = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		ctx.True(police != null, "police M151A2 prefab spawns an entity");
		if (ctx.FailureCount() > 0)
			return;

		EL_SirenManagerComponent manager = EL_Component<EL_SirenManagerComponent>.Find(police);
		ctx.NotNull(manager, "spawned police vehicle carries EL_SirenManagerComponent");
	}
};