// red-proof: 2026-11-19 `tools\cli test --no-build --only LF_Test_ResidentLockedDoor` before the fix: `a resident on a walk passes the door of a locked unowned house` failed (the door action refused the walking body, the same as a stranger); green once ReasonFor lets a walking resident through.
//! residents/locked-door: an ownerless house marker (the Saint-Philippe resident homes, #3233) shuts every door
//! of its building to players, but a resident on a walk opens the same doors to leave or enter its home. A stranger
//! stays out, and a resident that is not walking gets no pass.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_ResidentLockedDoor : LF_Test
{
	protected static const ResourceName HOUSE = "{EDBC0E94793BA9F1}Prefabs/Structures/Houses/Village/House_Village_E_1I01/House_Village_E_1I01.et";
	protected static const ResourceName MARKER = "{4D1B7E92A6C03F58}Prefabs/Systems/Houses/LF_HouseMarker.et";
	protected static const string HOUSE_ID = "resident-door-house";
	protected static const string RESIDENT_ID = "lf_test_locked_door_resident";
	protected static const string STRANGER_UID = "lf-test-locked-door-stranger";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "residents/locked-door";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		BaseWorld world = GetGame().GetWorld();
		vector at = "60 0 140";
		at[1] = world.GetSurfaceY(at[0], at[2]);
		IEntity house = LF_Utils.SpawnEntityPrefab(HOUSE, at);
		ctx.NotNull(house, "a vanilla house spawns");
		IEntity resident = LF_TestCharacterFixture.Spawn(ctx);
		IEntity stranger = LF_TestCharacterFixture.Spawn(ctx);
		IEntity marker;
		if (house && resident && stranger)
			RunHouse(ctx, house, resident, stranger, marker);

		LF_ResidentWalk.ResetStatics();
		LF_TestCharacterFixture.Delete(resident);
		LF_TestCharacterFixture.Delete(stranger);
		if (marker)
			SCR_EntityHelper.DeleteEntityAndChildren(marker);

		if (house)
			SCR_EntityHelper.DeleteEntityAndChildren(house);

		LF_Houses.ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunHouse(LF_TestContext ctx, IEntity house, IEntity resident, IEntity stranger, out IEntity marker)
	{
		array<IEntity> doors = {};
		CollectDoors(house, doors);
		ctx.True(doors.Count() > 0, "the house has child doors with a door action");
		if (doors.Count() == 0)
			return;

		IEntity door = doors[0];
		SCR_DoorUserAction action = DoorAction(door);
		vector mins;
		vector maxs;
		house.GetWorldBounds(mins, maxs);
		marker = LF_Utils.SpawnEntityPrefab(MARKER, (mins + maxs) * 0.5);
		ctx.NotNull(marker, "the house marker spawns");
		if (!marker)
			return;

		LF_BuildingLockComponent lock = LF_Component<LF_BuildingLockComponent>.Find(marker);
		LF_HouseManagerComponent manager = LF_Component<LF_HouseManagerComponent>.Find(marker);
		ctx.NotNull(lock, "the marker carries the lock component");
		ctx.NotNull(manager, "and a house manager");
		if (!lock || !manager)
			return;

		ctx.True(manager.BindIdentifier(HOUSE_ID), "the house takes its identifier");
		ctx.True(lock.RegisterNow(), "the marker finds and locks its building");
		LF_Houses.BindCharacter(stranger, STRANGER_UID);

		ctx.False(action.CanBePerformedScript(stranger), "a stranger is refused at the door of the ownerless house");
		ctx.False(action.CanBePerformedScript(resident), "a resident that is not walking gets no pass");

		if (!RedProof())
			LF_ResidentWalk.MarkWalkingForTest(RESIDENT_ID, resident);

		ctx.True(action.CanBePerformedScript(resident), "a resident on a walk passes the door of a locked unowned house");
		ctx.EqualStr("", LF_BuildingLocks.ReasonFor(door, resident), "with no reason shown");
		ctx.False(action.CanBePerformedScript(stranger), "a stranger is still refused while the resident walks");

		LF_ResidentWalk.ResetStatics();
		ctx.False(action.CanBePerformedScript(resident), "the pass ends with the walk");
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
