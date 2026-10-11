// red-proof: expected wanted 0 after Arm and ran `tools\cli test --no-build --tier fast`
// on 2026-10-11. The suite reported
// `[ELTEST] - wanted on the breaker increases (expected 0, got 1)`.
// Restored the expected value to 1.

// tier: LOGIC
class EL_Test_Roadmap_Jailbreak : EL_Test
{
	override string GetName()
	{
		return "roadmap/jailbreak";
	}

	override void Run(EL_TestContext ctx)
	{
		string officerUid = "roadmap-jailbreak-officer";
		string prisonerUid = "roadmap-jailbreak-prisoner";
		string breakerUid = "roadmap-jailbreak-breaker";
		string doorId = "logic-jail-door";
		vector placed = "48 0 64";
		vector outside = "48 0 90";

		LF_Jail.Configure(40, 200, 1, 4);
		LF_Jail.SetCellOrigin(placed);
		LF_Jail.SetDoorTarget(doorId);
		LF_Breach.Configure(doorId, 0, "");

		EL_PlayerAccount officer = EL_PlayerAccountManager.GetOrCreate(officerUid);
		officer.SetFaction(EL_Faction.POLICE);
		officer.SetOnDuty(true);
		EL_PlayerAccount prisoner = EL_PlayerAccountManager.GetOrCreate(prisonerUid);
		prisoner.SetWantedLevel(2);
		EL_PlayerAccount breaker = EL_PlayerAccountManager.GetOrCreate(breakerUid);
		breaker.SetWantedLevel(0);

		ctx.True(LF_Jail.Arrest(officerUid, prisonerUid, null), "arrest starts the sentence");
		ctx.EqualFloat(80, LF_Jail.GetRemaining(prisonerUid), 0.01, "sentence is two wanted steps");
		ctx.Equal(2, prisoner.GetWantedLevel(), "arrest does not clear wanted");
		ctx.True(LF_Jail.Confines(prisonerUid), "locked door confines a prisoner with time left");
		ctx.True(SamePlace(LF_Jail.ResolvePosition(prisonerUid, outside), placed), "locked volume returns an escaped position to the cell");
		ctx.EqualFloat(80, LF_Jail.GetRemaining(prisonerUid), 0.01, "a pull-back does not change the remaining sentence");

		ctx.True(LF_Breach.Arm(doorId, breakerUid), "the jail door uses the shared breach");
		ctx.Equal(1, breaker.GetWantedLevel(), "wanted on the breaker increases");
		ctx.Equal(2, prisoner.GetWantedLevel(), "a prisoner who walks out stays wanted");
		ctx.False(LF_Jail.Confines(prisonerUid), "an open door does not confine");
		ctx.True(SamePlace(LF_Jail.ResolvePosition(prisonerUid, outside), outside), "an open door does not pull the prisoner back");
		ctx.EqualFloat(80, LF_Jail.GetRemaining(prisonerUid), 0.01, "escape does not zero the remaining sentence");

		ctx.True(LF_Breach.Repair(doorId, officerUid), "police repair the jail door");
		ctx.True(LF_Jail.Confines(prisonerUid), "repair holds the prisoner again");
		ctx.True(SamePlace(LF_Jail.ResolvePosition(prisonerUid, outside), placed), "a repaired door returns the next escape to the cell");
		ctx.EqualFloat(80, LF_Jail.GetRemaining(prisonerUid), 0.01, "repair does not zero the remaining sentence");

		LF_Jail.SyncPlacedCell();
	}

	protected bool SamePlace(vector a, vector b)
	{
		if (Math.AbsFloat(a[0] - b[0]) > 0.05)
			return false;
		if (Math.AbsFloat(a[1] - b[1]) > 0.05)
			return false;
		if (Math.AbsFloat(a[2] - b[2]) > 0.05)
			return false;
		return true;
	}
}

// tier: WORLD
class EL_Test_Roadmap_JailbreakWorld : EL_Test
{
	override string GetName()
	{
		return "e2e/roadmap-jailbreak";
	}

	override void Run(EL_TestContext ctx)
	{
		Resource doorPrefab = Resource.Load(LF_Jail.DOOR_PREFAB);
		ctx.True(doorPrefab.IsValid(), "prison door prefab loads");

		IEntity door = GetGame().GetWorld().FindEntityByName("Roadmap_Jail_Door");
		ctx.True(door != null, "Roadmap_Jail_Door is placed");
		if (ctx.FailureCount() > 0)
			return;

		EL_LockComponent doorLock = EL_Component<EL_LockComponent>.Find(door);
		ctx.NotNull(doorLock, "jail door carries EL_LockComponent");
		if (ctx.FailureCount() > 0)
			return;

		ActionsManagerComponent actions = EL_Component<ActionsManagerComponent>.Find(door);
		ctx.NotNull(actions, "jail door carries ActionsManagerComponent");
		if (ctx.FailureCount() > 0)
			return;

		array<BaseUserAction> actionList = {};
		actions.GetActionsList(actionList);
		bool armWired = false;
		bool repairWired = false;
		foreach (BaseUserAction action : actionList)
		{
			if (LF_BreachArmAction.Cast(action))
				armWired = true;
			if (LF_BreachRepairAction.Cast(action))
				repairWired = true;
		}

		ctx.True(armWired, "arm action is on the jail door");
		ctx.True(repairWired, "repair action is on the jail door");

		LF_Jail.SyncPlacedCell();
		LF_Jail.Configure(30, 120, 1, 6);

		string officerUid = "roadmap-jailbreak-e2e-officer";
		string prisonerUid = "roadmap-jailbreak-e2e-prisoner";
		string breakerUid = "roadmap-jailbreak-e2e-breaker";
		EL_PlayerAccount officer = EL_PlayerAccountManager.GetOrCreate(officerUid);
		officer.SetFaction(EL_Faction.POLICE);
		officer.SetOnDuty(true);
		EL_PlayerAccount prisoner = EL_PlayerAccountManager.GetOrCreate(prisonerUid);
		prisoner.SetWantedLevel(1);
		EL_PlayerAccountManager.GetOrCreate(breakerUid);

		if (!doorLock.IsLocked())
			LF_Breach.Repair("Roadmap_Jail_Door", officerUid);

		ctx.True(doorLock.IsLocked(), "jail door is locked before the zero-fuse arm");
		ctx.True(LF_Jail.Arrest(officerUid, prisonerUid, null), "a sentenced prisoner is in the locked jail");
		ctx.True(LF_Jail.Confines(prisonerUid), "confinement predicate is true while the door is locked");
		float remaining = LF_Jail.GetRemaining(prisonerUid);
		ctx.True(remaining > 0, "the prisoner still has time when the door opens");

		ctx.True(LF_Breach.Arm("Roadmap_Jail_Door", breakerUid), "zero-fuse arm opens the jail door");
		ctx.False(doorLock.IsLocked(), "jail door IsLocked() is false after the fuse");
		ctx.False(LF_Jail.Confines(prisonerUid), "confinement predicate is false while the door is open");
		ctx.True(LF_Jail.GetRemaining(prisonerUid) > 0, "opening the door does not zero the sentence");

		ctx.True(LF_Breach.Repair("Roadmap_Jail_Door", officerUid), "police repair runs");
		ctx.True(doorLock.IsLocked(), "repair sets the lock");
		ctx.True(LF_Jail.Confines(prisonerUid), "repair makes the confinement predicate true");
	}
}
