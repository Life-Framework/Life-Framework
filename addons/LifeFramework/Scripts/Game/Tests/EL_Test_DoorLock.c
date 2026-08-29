// red-proof: make EL_LockComponent.SetLocked a no-op (return before writing m_bLocked) and run
// `tools\cli test --tier all`; every lock/unlock transition assertion fails. For the key path,
// make EL_LockComponent.IsValidKey always return false; the bound-key assertions fail. The
// door action gate reads exactly the IsLocked state asserted here, so gating is covered by
// these transitions.

// tier: WORLD
class EL_Test_DoorLock : EL_Test
{
	protected static const ResourceName DOOR_FIXTURE = "{99D61E9EDBDF3CC8}Prefabs/Structures/BuildingParts/Doors/EL_Door_Base.et";
	protected static const ResourceName KEY_PREFAB = "{3735389521255DBE}Prefabs/Items/HouseKeyItem.et";

	override string GetName()
	{
		return "houses/door-lock";
	}

	override void Run(EL_TestContext ctx)
	{
		Resource res = Resource.Load(DOOR_FIXTURE);
		ctx.True(res.IsValid(), "door prefab loads: " + DOOR_FIXTURE);
		if (ctx.FailureCount() > 0)
			return;

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.LOCAL;

		IEntity door = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		ctx.True(door != null, "door prefab spawns an entity");
		if (ctx.FailureCount() > 0)
			return;

		EL_LockComponent doorLock = EL_Component<EL_LockComponent>.Find(door);
		ctx.NotNull(doorLock, "door carries EL_LockComponent");
		if (ctx.FailureCount() > 0)
			return;

		ctx.False(doorLock.IsLocked(), "door starts unlocked");
		doorLock.Lock();
		ctx.True(doorLock.IsLocked(), "door is locked after Lock()");
		doorLock.ToggleLock();
		ctx.False(doorLock.IsLocked(), "door unlocked after ToggleLock");
		doorLock.Lock();
		ctx.True(doorLock.IsLocked(), "door locked again");
		doorLock.Unlock();
		ctx.False(doorLock.IsLocked(), "door unlocked after Unlock()");

		Resource keyRes = Resource.Load(KEY_PREFAB);
		ctx.True(keyRes.IsValid(), "key prefab loads: " + KEY_PREFAB);
		if (ctx.FailureCount() > 0)
			return;

		IEntity key = GetGame().SpawnEntityPrefab(keyRes, GetGame().GetWorld(), params);
		ctx.True(key != null, "key prefab spawns an entity");
		if (ctx.FailureCount() > 0)
			return;

		EL_KeyComponent keyComponent = EL_Component<EL_KeyComponent>.Find(key);
		ctx.NotNull(keyComponent, "key entity carries EL_KeyComponent");
		if (ctx.FailureCount() > 0)
			return;

		ctx.False(doorLock.IsValidKey(key), "an unbound key opens nothing");

		doorLock.SetHouseIdentifier("test-house");
		keyComponent.SetHouseIdentifier("test-house");
		ctx.True(doorLock.IsValidKey(key), "a key bound to the house identifier is valid");

		keyComponent.SetHouseIdentifier("other-house");
		ctx.False(doorLock.IsValidKey(key), "a key bound to a different house is rejected");
	}
};