// red-proof: Observed 2026-11-17 (bug fix) with `tools\cli test --no-build --only LF_Test_HouseKeyedOpen` on the unfixed LF_DoorUserAction.c (keyed open calls Unlock, gate checks the key only): `[LFTEST] FAIL houses/keyed-open-keeps-lock`, `- the door is still locked after the key holder opened it`, `- a stranger after the key holder is still refused`, `- a character with no key and no ownership is refused`. Green with the fix. The server re-check (review finding): with it removed the run failed `- the server does not move a locked door for a stranger`.
// tier: WORLD
//! houses/keyed-open-keeps-lock (#2410): a key holder opening a locked house door passes the gate and
//! the door stays locked, so a stranger after them is still refused. The owner passes without a key;
//! a cleared owner does not. Before the fix the vanilla door action called Unlock() on every keyed
//! open, which opened the house to everyone until a restart.
class LF_Test_HouseKeyedOpen : LF_Test
{
	protected static const ResourceName DOOR = "{99D61E9EDBDF3CC8}Prefabs/Structures/BuildingParts/Doors/EL_Door_Base.et";
	protected static const ResourceName KEY = "{3735389521255DBE}Prefabs/Items/HouseKeyItem.et";
	protected static const string HOUSE = "keyed-open-house";
	protected static const string OWNER = "keyed-open-owner";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "houses/keyed-open-keeps-lock";
	}

	//------------------------------------------------------------------------------------------------
	protected SCR_DoorUserAction DoorAction(IEntity door)
	{
		BaseActionsManagerComponent manager = BaseActionsManagerComponent.Cast(door.FindComponent(BaseActionsManagerComponent));
		if (!manager)
			return null;

		array<BaseUserAction> actions = {};
		manager.GetActionsList(actions);
		foreach (BaseUserAction action : actions)
		{
			SCR_DoorUserAction doorAction = SCR_DoorUserAction.Cast(action);
			if (doorAction)
				return doorAction;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		IEntity door = LF_Utils.SpawnEntityPrefab(DOOR, "64 0 64");
		IEntity holder = LF_TestCharacterFixture.Spawn(ctx);
		IEntity stranger = LF_TestCharacterFixture.Spawn(ctx);
		IEntity owner = LF_TestCharacterFixture.Spawn(ctx);
		IEntity key = LF_Utils.SpawnEntityPrefab(KEY, "64 0 65");
		ctx.NotNull(door, "a house door spawns");
		ctx.NotNull(key, "a house key spawns");
		if (door && holder && stranger && owner && key)
			RunDoor(ctx, door, holder, stranger, owner, key);

		LF_TestCharacterFixture.Delete(holder);
		LF_TestCharacterFixture.Delete(stranger);
		LF_TestCharacterFixture.Delete(owner);
		LF_TestCharacterFixture.Delete(door);
		LF_TestCharacterFixture.Delete(key);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunDoor(LF_TestContext ctx, IEntity door, IEntity holder, IEntity stranger, IEntity owner, IEntity key)
	{
		LF_LockComponent doorLock = LF_Component<LF_LockComponent>.Find(door);
		LF_KeyComponent keyComponent = LF_Component<LF_KeyComponent>.Find(key);
		SCR_DoorUserAction action = DoorAction(door);
		ctx.NotNull(doorLock, "the door carries a lock");
		ctx.NotNull(keyComponent, "the key carries LF_KeyComponent");
		ctx.NotNull(action, "the door has its door action");
		if (!doorLock || !keyComponent || !action)
			return;

		InventoryStorageManagerComponent storage = LF_InventoryUtils.GetResponsibleStorageManager(holder);
		ctx.True(storage && storage.TryInsertItem(key, EStoragePurpose.PURPOSE_DEPOSIT), "the key goes into the holder's pockets");
		keyComponent.SetHouseIdentifier(HOUSE);
		doorLock.SetHouseIdentifier(HOUSE);
		doorLock.Lock();
		LF_Houses.BindCharacter(owner, OWNER);

		ctx.False(action.CanBePerformedScript(stranger), "a stranger cannot open the locked door");
		ctx.True(action.CanBePerformedScript(holder), "the key holder may open the locked door");

		// The server refuses a stranger even when the client skipped its gate.
		BaseDoorComponent doorComponent = BaseDoorComponent.Cast(door.FindComponent(BaseDoorComponent));
		ctx.NotNull(doorComponent, "the door has a door component");
		if (!doorComponent)
			return;

		float closed = doorComponent.GetControlValue();
		action.PerformAction(door, stranger);
		ctx.True(doorComponent.GetControlValue() == closed, "the server does not move a locked door for a stranger");

		action.PerformAction(door, holder);
		ctx.False(doorComponent.GetControlValue() == closed, "the key holder's open moves the door");
		ctx.True(doorLock.IsLocked(), "the door is still locked after the key holder opened it");
		ctx.False(action.CanBePerformedScript(stranger), "a stranger after the key holder is still refused");

		ctx.False(action.CanBePerformedScript(owner), "a character with no key and no ownership is refused");
		doorLock.SetOwnerKey(OWNER);
		ctx.True(action.CanBePerformedScript(owner), "the owner passes the locked door without a key");
		ctx.False(action.CanBePerformedScript(stranger), "owning the house lets nobody else in");
		doorLock.SetOwnerKey("");
		ctx.False(action.CanBePerformedScript(owner), "a cleared owner is refused again");
		doorLock.Unlock();
	}
}
