// red-proof: observed 2026-11-18 (#2373): `toolscli test --no-build --red LF_Test_EveronPoliceCellDoor` printed `RED-OK everon/police-cell-door (fails with the hook on, 4 assertions)` (RedProof() skips the station spawn, so there is no door); green without it.
//! #2373: the police villa's own barred cell door is the jail's breach target. A station prefab
//! (`PoliceStation.et`, the pack's building) spawns with the cell door leaf placed as the breach target
//! `PoliceStation_CellDoor`, locked at spawn and carrying the door the jail opens on release. With that id
//! as the cell's door target (what `fitout.mjs --with-cell` writes) a prisoner is held while the door is
//! locked, a breach with a charge opens it and the prisoner may walk out, and a police repair holds them again:
//! the `Roadmap_Jail_Door` path of #235 with no `EL_JailDoor` hung in the doorway.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronPoliceCellDoor : LF_Test
{
	static const ResourceName STATION = "{F10B7BFC04480ACF}Prefabs/Buildings/PoliceStation/PoliceStation.et";
	static const string DOOR_ID = "PoliceStation_CellDoor";
	protected static const string OFFICER_UID = "everon-celldoor-officer";
	protected static const string PRISONER_UID = "everon-celldoor-prisoner";
	protected static const string BREAKER_UID = "everon-celldoor-breaker";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/police-cell-door";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		IEntity anchor = GetGame().GetWorld().FindEntityByName(LF_TestAnnex.BREACH_DOOR);
		ctx.NotNull(anchor, "the annex is placed (the spawn anchor)");
		if (!anchor)
			return;

		// Well below the ground: a station needs no room and the gate's leftover check never sees it.
		vector at = anchor.GetOrigin() + "0 -60 0";
		IEntity station;
		if (!RedProof())
			station = LF_Utils.SpawnEntityPrefab(STATION, at, "0 0 0", false);

		ctx.NotNull(station, "the police station prefab spawns");
		IEntity door = LF_Places.Find(LF_Breach.PLACE_KIND, DOOR_ID);
		ctx.NotNull(door, "the villa's cell door is placed as the breach target " + DOOR_ID);
		if (!station || !door)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(station);
			return;
		}

		ctx.True(door.FindComponent(DoorComponent) != null, "the placed entity is the door leaf the jail opens on release");
		LF_LockComponent doorLock = LF_Component<LF_LockComponent>.Find(door);
		ctx.NotNull(doorLock, "the cell door carries LF_LockComponent");
		if (doorLock)
			ctx.EqualStr("#LF-Cell_Locked", doorLock.GetLockedReason(), "a locked cell gate refuses with the cell wording, not 'House is locked'");
		ctx.True(LF_Component<LF_BreachTargetComponent>.Find(door) != null, "and LF_BreachTargetComponent");
		ctx.True(door.GetParent() != null, "it is a child of the villa, not a second door beside it");

		// The jail, the way a cell with m_sDoorTargetId = PoliceStation_CellDoor sets it.
		LF_Jail.Configure(40, 200, 1, 4);
		LF_Jail.SetCellOrigin("48 0 64");
		LF_Jail.SetDoorTarget(DOOR_ID);
		LF_Breach.Reset(DOOR_ID);

		LF_PlayerAccount officer = LF_PlayerAccountManager.GetOrCreate(OFFICER_UID);
		officer.SetFaction(LF_Faction.POLICE);
		officer.SetOnDuty(true);
		LF_PlayerAccount prisoner = LF_PlayerAccountManager.GetOrCreate(PRISONER_UID);
		prisoner.SetWantedLevel(2);
		LF_PlayerAccount breaker = LF_PlayerAccountManager.GetOrCreate(BREAKER_UID);
		breaker.SetWantedLevel(0);

		ctx.True(LF_Breach.IsLocked(DOOR_ID), "the cell door starts locked");
		ctx.True(LF_Jail.Arrest(OFFICER_UID, PRISONER_UID, null), "arrest starts the sentence");
		ctx.True(LF_Jail.Confines(PRISONER_UID), "a locked cell door confines the prisoner");

		ctx.True(LF_Breach.Arm(DOOR_ID, BREAKER_UID), "the cell door takes a breach");
		ctx.False(LF_Breach.IsLocked(DOOR_ID), "the breach opened the villa's cell door");
		if (doorLock)
			ctx.False(doorLock.IsLocked(), "the door's own lock is open");

		ctx.Equal(1, breaker.GetWantedLevel(), "the breaker is wanted");
		ctx.False(LF_Jail.Confines(PRISONER_UID), "an open door does not confine the prisoner");

		ctx.True(LF_Breach.Repair(DOOR_ID, OFFICER_UID), "police repair the cell door");
		ctx.True(LF_Breach.IsLocked(DOOR_ID), "the repaired door is locked");
		ctx.True(LF_Jail.Confines(PRISONER_UID), "a repaired door holds the prisoner again");

		LF_Breach.Reset(DOOR_ID);
		// The leaf goes first: a place is gone once its entity is, and the station's own delete is not instant.
		SCR_EntityHelper.DeleteEntityAndChildren(door);
		ctx.True(LF_Places.Find(LF_Breach.PLACE_KIND, DOOR_ID) == null, "a deleted door is no longer placed");
		SCR_EntityHelper.DeleteEntityAndChildren(station);
		LF_Jail.SyncPlacedCell();
	}
}
