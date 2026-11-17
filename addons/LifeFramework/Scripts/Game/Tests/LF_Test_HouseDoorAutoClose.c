// red-proof: Observed 2026-11-18 (new feature) with `tools\cli test --no-build --only LF_Test_HouseDoorAutoClose` against a no-op LF_HouseDoorClose stub and the unchanged gate: `[LFTEST] FAIL houses/door-auto-close`, `- a stranger may close the locked house's open door`, `- the close tick shuts a locked house's open door`, `- an unlocked house's open door stays open`. Green with the gate and the tick.
// tier: WORLD
//! houses/door-auto-close (#2472): on a locked house, anyone may close an open door but only the owner
//! and key holders may open it, and a door left open closes itself once the delay has run. An unlocked
//! house's open door stays open. The close tick is called directly; nothing waits.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_HouseDoorAutoClose : LF_Test
{
	protected static const ResourceName DOOR = "{99D61E9EDBDF3CC8}Prefabs/Structures/BuildingParts/Doors/EL_Door_Base.et";
	protected static const string HOUSE = "auto-close-house";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "houses/door-auto-close";
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
		IEntity stranger = LF_TestCharacterFixture.Spawn(ctx);
		ctx.NotNull(door, "a house door spawns");
		if (door && stranger)
			RunDoor(ctx, door, stranger);

		LF_TestCharacterFixture.Delete(stranger);
		LF_TestCharacterFixture.Delete(door);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunDoor(LF_TestContext ctx, IEntity door, IEntity stranger)
	{
		LF_LockComponent doorLock = LF_Component<LF_LockComponent>.Find(door);
		BaseDoorComponent doorComponent = BaseDoorComponent.Cast(door.FindComponent(BaseDoorComponent));
		SCR_DoorUserAction action = DoorAction(door);
		ctx.NotNull(doorLock, "the door carries a lock");
		ctx.NotNull(doorComponent, "the door has a door component");
		ctx.NotNull(action, "the door has its door action");
		if (!doorLock || !doorComponent || !action)
			return;

		ctx.EqualFloat(20, LF_HouseDoorClose.GetDelaySeconds(), 0.001, "a door left open closes after 20 seconds unless the server says otherwise");
		doorLock.SetHouseIdentifier(HOUSE);
		doorLock.Lock();

		// Closed and locked: a stranger cannot open it.
		doorComponent.SetControlValue(0, RplId.Invalid());
		ctx.False(LF_HouseDoorClose.IsOpen(door), "the door starts closed");
		ctx.False(action.CanBePerformedScript(stranger), "a stranger cannot open the locked door");

		// Left open: a stranger may close it, and the server lets the close through.
		doorComponent.SetControlValue(1, RplId.Invalid());
		ctx.True(LF_HouseDoorClose.IsOpen(door), "the door is open");
		ctx.True(action.CanBePerformedScript(stranger), "a stranger may close the locked house's open door");
		action.PerformAction(door, stranger);
		ctx.False(LF_HouseDoorClose.IsOpen(door), "the stranger's close shuts the door");
		ctx.True(doorLock.IsLocked(), "closing it keeps the lock");
		action.PerformAction(door, stranger);
		ctx.False(LF_HouseDoorClose.IsOpen(door), "the server still refuses a stranger's open");

		// Left open and nobody closes it: the tick shuts it.
		doorComponent.SetControlValue(1, RplId.Invalid());
		ctx.True(LF_HouseDoorClose.IsOpen(door), "the door is open again");
		ctx.True(LF_HouseDoorClose.Tick(door), "the close tick shuts a locked house's open door");
		ctx.False(LF_HouseDoorClose.IsOpen(door), "the door closed itself");
		ctx.False(LF_HouseDoorClose.Tick(door), "the tick on a closed door does nothing");

		// An unlocked house's door stays where people left it.
		doorLock.Unlock();
		doorComponent.SetControlValue(1, RplId.Invalid());
		ctx.False(LF_HouseDoorClose.Tick(door), "the tick leaves an unlocked house's door alone");
		ctx.True(LF_HouseDoorClose.IsOpen(door), "an unlocked house's open door stays open");
		doorComponent.SetControlValue(0, RplId.Invalid());
	}
}
