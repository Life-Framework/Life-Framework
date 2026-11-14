// red-proof: 2026-11-14, with LF_Breach.FindLock back on FindEntityByName(targetId),
//   cli test --no-build --only LF_Test_PlacesUnnamedDoor:
//   [LFTEST]   - LF_Breach locks the unnamed door it found by component
// Restored; green.
// tier: WORLD
//! places/unnamed-door: systems find their places by component, never by entity name (#961). The
//! annex places are found under their ids by kind; a door spawned with no name, placed as a breach
//! target, is locked and opened by LF_Breach; a second door cannot take a held id; a deleted place
//! is not found. Under the old FindEntityByName lookup the unnamed door never locked.
class LF_Test_PlacesUnnamedDoor : LF_Test
{
	protected static const string DOOR_ID = "places-unnamed-door";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "places/unnamed-door";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		BaseWorld world = GetGame().GetWorld();
		IEntity breachDoor = world.FindEntityByName(LF_TestAnnex.BREACH_DOOR);
		IEntity vault = world.FindEntityByName(LF_TestAnnex.BANK_VAULT);
		IEntity till = world.FindEntityByName(LF_TestAnnex.HOSPITAL_TILL);
		IEntity hideoutDoor = world.FindEntityByName(LF_TestAnnex.HIDEOUT_DOOR);
		ctx.True(breachDoor && vault && till && hideoutDoor, "the annex breach door, vault, till and hideout door are placed");
		ctx.True(LF_Places.Find(LF_Breach.PLACE_KIND, LF_TestAnnex.BREACH_DOOR) == breachDoor, "the breach door is found by kind and id");
		ctx.True(LF_Places.Find(LF_Bank.PLACE_KIND, LF_TestAnnex.BANK_VAULT) == vault, "the vault is found as a vault");
		ctx.True(LF_Places.Find(LF_Breach.PLACE_KIND, LF_TestAnnex.BANK_VAULT) == vault, "the vault is found as a breach target");
		ctx.True(LF_Places.Find(LF_Hospital.PLACE_KIND, LF_TestAnnex.HOSPITAL_TILL) == till, "the hospital till is found");
		ctx.True(LF_Places.Find(LF_Hideout.PLACE_KIND_DOOR, LF_TestAnnex.HIDEOUT_ID) == hideoutDoor, "the hideout door is found under its hideout id");
		if (!breachDoor)
			return;

		vector at = breachDoor.GetOrigin() + "0 -20 0";
		IEntity door = LF_Utils.SpawnEntityPrefab(LF_Bank.DOOR_PREFAB, at, "0 0 0", false);
		IEntity other = LF_Utils.SpawnEntityPrefab(LF_Bank.DOOR_PREFAB, at + "3 0 0", "0 0 0", false);
		ctx.True(door && other, "two doors spawn");
		if (!door || !other)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(door);
			SCR_EntityHelper.DeleteEntityAndChildren(other);
			return;
		}

		ctx.EqualStr("", door.GetName(), "the spawned door has no name");
		LF_LockComponent doorLock = LF_Component<LF_LockComponent>.Find(door);
		ctx.NotNull(doorLock, "the door carries LF_LockComponent");

		ctx.True(LF_Places.Register(LF_Breach.PLACE_KIND, DOOR_ID, door), "the unnamed door is placed as a breach target");
		ctx.True(LF_Places.Register(LF_Breach.PLACE_KIND, DOOR_ID, door), "placing the same door again is a no-op");
		ctx.False(LF_Places.Register(LF_Breach.PLACE_KIND, DOOR_ID, other), "a second door cannot take a held id");
		ctx.True(LF_Places.Find(LF_Breach.PLACE_KIND, DOOR_ID) == door, "the id still finds the first door");

		vector origin;
		ctx.True(LF_Places.FindOrigin(LF_Breach.PLACE_KIND, DOOR_ID, origin), "the door's origin is found");
		ctx.True(vector.Distance(origin, door.GetOrigin()) < 0.01, "the origin is the door's");

		if (doorLock)
		{
			doorLock.Unlock();
			LF_Breach.Configure(DOOR_ID, 0, "");
			LF_Breach.Reset(DOOR_ID);
			ctx.True(doorLock.IsLocked(), "LF_Breach locks the unnamed door it found by component");
			ctx.True(LF_Breach.Arm(DOOR_ID, "places-unnamed-door-robber"), "the zero-fuse arm runs");
			ctx.False(doorLock.IsLocked(), "LF_Breach opens the unnamed door");
			LF_Breach.Reset(DOOR_ID);
		}

		SCR_EntityHelper.DeleteEntityAndChildren(door);
		ctx.True(LF_Places.Find(LF_Breach.PLACE_KIND, DOOR_ID) == null, "a deleted place is not found");
		ctx.True(LF_Places.Register(LF_Breach.PLACE_KIND, DOOR_ID, other), "the freed id takes the second door");
		SCR_EntityHelper.DeleteEntityAndChildren(other);
	}
}
