// red-proof: observed 2026-11-08 with `tools\cli test --no-build --only EL_Test_PoliceCarKey`, the key
// not yet in the POLICE kit and EL_VehicleKeyComponent without its INIT event mask:
// [ELTEST]   - the POLICE kit carries the station car key
// [ELTEST]   - an officer's key opens the station car
// Fixed both; green.

//! The starter police kit carries the key to the station car (maintainer playtest 2026-11-08).
//! Station cars are bound to the shared "POLICE" identifier, so every officer's key opens every
//! station car; the key lives in the duty inventory (LF_DutyLocker swaps it with the uniform).
// tier: WORLD
class EL_Test_PoliceCarKey : EL_Test
{
	static const ResourceName KEY = "{976BD4B5B482A8D9}Prefabs/Items/Roleplay/LF_PoliceCarKey.et";

	override string GetName()
	{
		return "police/car-key";
	}

	override void Run(EL_TestContext ctx)
	{
		EL_SpawnLogic spawnLogic = EL_SpawnLogic.GetInstance();
		ctx.NotNull(spawnLogic, "the game mode has EL_SpawnLogic");
		if (spawnLogic)
		{
			EL_FactionLoadout kit = spawnLogic.GetFactionLoadout(EL_Faction.POLICE);
			ctx.NotNull(kit, "the game mode has a POLICE kit");
			if (kit)
				ctx.True(kit.m_aDirectItems && kit.m_aDirectItems.Contains(KEY), "the POLICE kit carries the station car key");
		}

		IEntity car = GetGame().GetWorld().FindEntityByName("Roadmap_Escort_Car");
		ctx.NotNull(car, "DebugWorld places the station car Roadmap_Escort_Car");
		EL_VehicleLockComponent carLock;
		if (car)
			carLock = EL_Component<EL_VehicleLockComponent>.Find(car);

		ctx.NotNull(carLock, "the station car has EL_VehicleLockComponent");
		if (carLock)
			ctx.EqualStr("POLICE", carLock.GetVehicleIdentifier(), "the station car is bound to the police fleet");

		Resource resource = Resource.Load(KEY);
		ctx.True(resource && resource.IsValid(), "the police car key prefab loads");
		if (!resource || !resource.IsValid() || !carLock)
			return;

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.LOCAL;
		IEntity key = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), params);
		ctx.NotNull(key, "the police car key spawns");
		if (!key)
			return;

		ctx.True(carLock.IsValidKey(key), "an officer's key opens the station car");

		Resource plain = Resource.Load("{9D94B834AF514B4A}Prefabs/Items/Roleplay/VehicleKey.et");
		IEntity other;
		if (plain && plain.IsValid())
			other = GetGame().SpawnEntityPrefab(plain, GetGame().GetWorld(), params);

		if (other)
		{
			ctx.False(carLock.IsValidKey(other), "an unbound key does not open the station car");
			SCR_EntityHelper.DeleteEntityAndChildren(other);
		}

		SCR_EntityHelper.DeleteEntityAndChildren(key);
	}
}
