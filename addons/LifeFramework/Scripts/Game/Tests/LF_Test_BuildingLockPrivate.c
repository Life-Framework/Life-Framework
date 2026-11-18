// red-proof: observed 2026-11-18 with `--red LF_Test_BuildingLockPrivate` (GridWorld): `RED PROOF OK`; with the registration skipped, `the reason is the marker's sign key (expected '#LF-BuildingLock_Private', got '')`, `the server does not move a door of a locked building` and `without the open marker the building is locked again` failed. Green: 12 assertions.
//! houses/building-lock-private (#3203): a lock marker dropped inside a vanilla house shuts every door
//! of that building. The door prompt carries the marker's sign key as the reason; a door that is already
//! open may still be closed; the server refuses a forged open; removing the marker opens the doors again;
//! an open marker (a fitted place) in the same building overrides the lock.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_BuildingLockPrivate : LF_Test
{
	protected static const ResourceName HOUSE = "{EDBC0E94793BA9F1}Prefabs/Structures/Houses/Village/House_Village_E_1I01/House_Village_E_1I01.et";
	protected static const ResourceName MARKER = "{9D4A2E6B1F83C570}Prefabs/Systems/Houses/LF_BuildingLock.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "houses/building-lock-private";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		BaseWorld world = GetGame().GetWorld();
		vector at = "60 0 140";
		at[1] = world.GetSurfaceY(at[0], at[2]);
		IEntity house = LF_Utils.SpawnEntityPrefab(HOUSE, at);
		ctx.NotNull(house, "a vanilla house spawns");
		IEntity character = LF_TestCharacterFixture.Spawn(ctx);
		if (house && character)
			RunHouse(ctx, house, character);

		LF_TestCharacterFixture.Delete(character);
		if (house)
			SCR_EntityHelper.DeleteEntityAndChildren(house);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunHouse(LF_TestContext ctx, IEntity house, IEntity character)
	{
		array<IEntity> doors = {};
		CollectDoors(house, doors);
		ctx.True(doors.Count() > 0, "the house has child doors with a door action");
		if (doors.Count() == 0)
			return;

		IEntity door = doors[0];
		SCR_DoorUserAction action = DoorAction(door);
		ctx.True(action.CanBePerformedScript(character), "a door of an unmarked house opens");

		vector mins;
		vector maxs;
		house.GetWorldBounds(mins, maxs);
		vector inside = (mins + maxs) * 0.5;
		IEntity marker = LF_Utils.SpawnEntityPrefab(MARKER, inside);
		ctx.NotNull(marker, "the lock marker spawns");
		if (!marker)
			return;

		LF_BuildingLockComponent lock = LF_Component<LF_BuildingLockComponent>.Find(marker);
		ctx.NotNull(lock, "the marker carries LF_BuildingLockComponent");
		if (!lock)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(marker);
			return;
		}

		if (!RedProof())
			ctx.True(lock.RegisterNow(), "the marker finds and locks its building");

		ctx.False(action.CanBePerformedScript(character), "a door of the locked building is refused");
		ctx.EqualStr(LF_BuildingLocks.KEY_PRIVATE, LF_BuildingLocks.ReasonFor(door), "the reason is the marker's sign key");

		// The server refuses a forged open.
		BaseDoorComponent doorComponent = BaseDoorComponent.Cast(door.FindComponent(BaseDoorComponent));
		float closed = doorComponent.GetControlValue();
		action.PerformAction(door, character);
		ctx.True(doorComponent.GetControlValue() == closed, "the server does not move a door of a locked building");

		// A door already open may be closed (a closing never traps anyone).
		doorComponent.SetControlValue(1, RplId.Invalid());
		ctx.True(action.CanBePerformedScript(character), "an open door of a locked building may still be closed");
		doorComponent.SetControlValue(0, RplId.Invalid());

		// An open marker in the same building (a fitted place) overrides the private lock.
		LF_BuildingLocks.RegisterOpenAt(marker.GetOrigin(), 25, "test-open-marker");
		ctx.True(action.CanBePerformedScript(character), "an open marker in the same building overrides the lock");
		LF_BuildingLocks.UnregisterOpen("test-open-marker");
		ctx.False(action.CanBePerformedScript(character), "without the open marker the building is locked again");

		SCR_EntityHelper.DeleteEntityAndChildren(marker);
		ctx.True(action.CanBePerformedScript(character), "after the marker is deleted the door opens again");
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
	protected void CollectDoors(IEntity entity, notnull array<IEntity> doors)
	{
		if (DoorAction(entity) && entity.FindComponent(BaseDoorComponent))
			doors.Insert(entity);

		IEntity child = entity.GetChildren();
		while (child)
		{
			CollectDoors(child, doors);
			child = child.GetSibling();
		}
	}
}
