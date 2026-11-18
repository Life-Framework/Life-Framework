// red-proof: Observed 2026-11-18 with `tools\cli test --no-build --only LF_Test_HouseLockAction` against a stub LF_HouseLockAction (never shown, never performed): `[LFTEST] FAIL houses/lock-action`, `- a key holder sees the lock action`, `- the key holder locks the house`, `- the owner locks the house without a key`. Green with the action.
//! houses/lock-action (#2471): the house door offers "Lock house" / "Unlock house" to its owner and to a
//! key holder. A stranger neither sees nor performs it, and a forged server-side perform by them leaves
//! the lock unchanged. The lock flips only through LF_HouseManagerComponent.SetHouseLocked, so the
//! manager, its doors and the saved row agree.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_HouseLockAction : LF_Test
{
	protected static const ResourceName BUILDING = "{74676598DF38F201}Prefabs/Structures/Core/EL_Building_Base.et";
	protected static const ResourceName DOOR = "{99D61E9EDBDF3CC8}Prefabs/Structures/BuildingParts/Doors/EL_Door_Base.et";
	protected static const ResourceName KEY = "{3735389521255DBE}Prefabs/Items/HouseKeyItem.et";
	protected static const string HOUSE = "lock-action-house";
	protected static const string OWNER = "lock-action-owner";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "houses/lock-action";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_HouseLockAction LockAction(IEntity door)
	{
		BaseActionsManagerComponent manager = BaseActionsManagerComponent.Cast(door.FindComponent(BaseActionsManagerComponent));
		if (!manager)
			return null;

		array<BaseUserAction> actions = {};
		manager.GetActionsList(actions);
		foreach (BaseUserAction action : actions)
		{
			LF_HouseLockAction lockAction = LF_HouseLockAction.Cast(action);
			if (lockAction)
				return lockAction;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<ref LF_HouseLockRecord> before = LF_Houses.ExportAll();
		LF_Houses.ApplyAll(null);

		IEntity building = LF_Utils.SpawnEntityPrefab(BUILDING, "68 0 68");
		IEntity door = LF_Utils.SpawnEntityPrefab(DOOR, "68 0 68");
		IEntity key = LF_Utils.SpawnEntityPrefab(KEY, "68 0 69");
		IEntity holder = LF_TestCharacterFixture.Spawn(ctx);
		IEntity stranger = LF_TestCharacterFixture.Spawn(ctx);
		IEntity owner = LF_TestCharacterFixture.Spawn(ctx);
		ctx.NotNull(building, "the building spawns");
		ctx.NotNull(door, "the door spawns");
		ctx.NotNull(key, "the house key spawns");
		if (building && door && key && holder && stranger && owner)
		{
			building.AddChild(door, -1);
			RunHouse(ctx, building, door, key, holder, stranger, owner);
		}

		LF_TestCharacterFixture.Delete(holder);
		LF_TestCharacterFixture.Delete(stranger);
		LF_TestCharacterFixture.Delete(owner);
		LF_TestCharacterFixture.Delete(key);
		// The door is the building's child once added, so deleting the building deletes it.
		if (building)
			LF_TestCharacterFixture.Delete(building);
		else
			LF_TestCharacterFixture.Delete(door);
		LF_Houses.ApplyAll(before);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunHouse(LF_TestContext ctx, IEntity building, IEntity door, IEntity key, IEntity holder, IEntity stranger, IEntity owner)
	{
		LF_HouseManagerComponent house = LF_Component<LF_HouseManagerComponent>.Find(building);
		LF_LockComponent doorLock = LF_Component<LF_LockComponent>.Find(door);
		LF_KeyComponent keyComponent = LF_Component<LF_KeyComponent>.Find(key);
		LF_HouseLockAction action = LockAction(door);
		ctx.NotNull(house, "the building carries a house manager");
		ctx.NotNull(doorLock, "the door carries a lock");
		ctx.NotNull(keyComponent, "the key carries LF_KeyComponent");
		ctx.NotNull(action, "the door has the house lock action");
		if (!house || !doorLock || !keyComponent || !action)
			return;

		ctx.True(house.BindIdentifier(HOUSE), "the house binds a stable identifier");
		keyComponent.SetHouseIdentifier(HOUSE);
		InventoryStorageManagerComponent storage = LF_InventoryUtils.GetResponsibleStorageManager(holder);
		ctx.True(storage && storage.TryInsertItem(key, EStoragePurpose.PURPOSE_DEPOSIT), "the key goes into the holder's pockets");
		LF_Houses.BindCharacter(owner, OWNER);
		house.SetOwner(OWNER);
		ctx.False(house.IsHouseLocked(), "the house starts unlocked");

		// Who sees and may perform it.
		ctx.False(action.CanBeShownScript(stranger), "a stranger does not see the lock action");
		ctx.False(action.CanBePerformedScript(stranger), "a stranger cannot perform the lock action");
		ctx.True(action.CanBeShownScript(holder), "a key holder sees the lock action");
		ctx.True(action.CanBePerformedScript(holder), "a key holder may perform the lock action");
		ctx.True(action.CanBeShownScript(owner), "the owner sees the lock action without a key");

		// A forged perform by a stranger changes nothing.
		action.PerformAction(door, stranger);
		ctx.False(house.IsHouseLocked(), "a stranger's forged perform does not lock the house");
		ctx.False(doorLock.IsLocked(), "a stranger's forged perform does not lock the door");

		// The key holder locks, then unlocks, through the house manager.
		string name;
		action.GetActionNameScript(name);
		ctx.EqualStr("#LF-HouseLock_Lock", name, "an unlocked house offers 'Lock house'");
		action.PerformAction(door, holder);
		ctx.True(house.IsHouseLocked(), "the key holder locks the house");
		ctx.True(doorLock.IsLocked(), "the door follows the house lock");
		action.GetActionNameScript(name);
		ctx.EqualStr("#LF-HouseLock_Unlock", name, "a locked house offers 'Unlock house'");
		ctx.False(action.CanBePerformedScript(stranger), "a stranger still cannot unlock it");
		action.PerformAction(door, stranger);
		ctx.True(house.IsHouseLocked(), "a stranger's forged perform does not unlock the house");
		action.PerformAction(door, holder);
		ctx.False(house.IsHouseLocked(), "the key holder unlocks the house");
		ctx.False(doorLock.IsLocked(), "the door follows the unlock");

		// The owner needs no key.
		action.PerformAction(door, owner);
		ctx.True(house.IsHouseLocked(), "the owner locks the house without a key");
		action.PerformAction(door, owner);
		ctx.False(house.IsHouseLocked(), "the owner unlocks the house without a key");
	}
}
