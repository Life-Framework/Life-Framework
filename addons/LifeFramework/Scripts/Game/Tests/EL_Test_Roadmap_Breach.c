// red-proof: expected wanted 0 after Arm and ran `tools\cli test --no-build --tier fast`
// on 2026-10-11. The suite reported
// `[ELTEST] - wanted rises once at arm time (expected 0, got 1)`.
// Restored the expected value to 1.

// tier: LOGIC
class EL_Test_Roadmap_Breach : EL_Test
{
	override string GetName()
	{
		return "roadmap/breach";
	}

	override void Run(EL_TestContext ctx)
	{
		string uid = "roadmap-breach-logic";
		string policeUid = "roadmap-breach-police";
		string orgUid = "roadmap-breach-org";
		string targetId = "logic-breach-door";
		string fuseTarget = "logic-breach-fuse";
		string orgTarget = "logic-breach-hideout";

		ctx.False(LF_Breach.Arm("missing-breach-target", uid), "unconfigured target refuses");
		ctx.Equal(0, LF_Breach.GetPoliceNotifyCount("missing-breach-target"), "unconfigured arm does not notify");

		LF_Breach.Configure(targetId, 0, "");
		ctx.True(LF_Breach.CanArm(targetId), "configured closed target can be armed");
		ctx.True(LF_Breach.CanArm(targetId), "offer refresh can be asked again");
		ctx.Equal(0, LF_Breach.GetPoliceNotifyCount(targetId), "offer refresh does not notify");

		EL_PlayerAccount account = EL_PlayerAccountManager.GetOrCreate(uid);
		ctx.NotNull(account, "breacher account exists");
		ctx.Equal(0, account.GetWantedLevel(), "account starts clean");

		ctx.True(LF_Breach.Arm(targetId, uid), "zero fuse arm succeeds");
		ctx.False(LF_Breach.IsLocked(targetId), "zero fuse opens the lock");
		ctx.Equal(1, account.GetWantedLevel(), "wanted rises once at arm time");
		ctx.Equal(1, LF_Breach.GetPoliceNotifyCount(targetId), "police notify fires once at arm time");

		ctx.False(LF_Breach.CanArm(targetId), "open target is not offered");
		LF_Breach.CanArm(targetId);
		ctx.Equal(1, LF_Breach.GetPoliceNotifyCount(targetId), "offer refresh after open does not notify again");
		ctx.False(LF_Breach.Arm(targetId, uid), "second charge on an open target refuses");
		ctx.Equal(1, account.GetWantedLevel(), "refused second charge does not raise wanted");
		ctx.Equal(1, LF_Breach.GetPoliceNotifyCount(targetId), "refused second charge does not notify");

		LF_BreachRecord saved = LF_Breach.Export(targetId);
		ctx.NotNull(saved, "export returns the open record");
		if (ctx.FailureCount() > 0)
			return;

		ctx.True(saved.m_bOpen, "exported record is open");
		ctx.False(LF_Breach.Apply(LF_BreachRecord.Create("other-breach", true)), "a record for an unknown target is rejected");

		ctx.False(LF_Breach.Repair(targetId, uid), "civilian repair refuses");
		ctx.False(LF_Breach.IsLocked(targetId), "refused repair leaves the lock open");

		EL_PlayerAccount police = EL_PlayerAccountManager.GetOrCreate(policeUid);
		police.SetFaction(EL_Faction.POLICE);
		ctx.True(LF_Breach.Repair(targetId, policeUid), "police repair succeeds");
		ctx.True(LF_Breach.IsLocked(targetId), "repair sets the lock");

		ctx.True(LF_Breach.Apply(saved), "apply restores the saved open lock");
		ctx.False(LF_Breach.IsLocked(targetId), "applied record opens the lock again");

		LF_Breach.Configure(fuseTarget, 5, "");
		ctx.True(LF_Breach.FuseElapsed(0, 0), "a zero fuse is already elapsed");
		ctx.False(LF_Breach.FuseElapsed(5, 4), "four seconds is short of a five second fuse");
		ctx.True(LF_Breach.FuseElapsed(5, 5), "five seconds elapses a five second fuse");
		ctx.True(LF_Breach.Arm(fuseTarget, uid), "positive fuse arm succeeds");
		ctx.True(LF_Breach.IsLocked(fuseTarget), "lock stays shut until the fuse elapses");
		ctx.Equal(1, LF_Breach.GetPoliceNotifyCount(fuseTarget), "notify happened at arm, not when the fuse elapses");
		ctx.False(LF_Breach.Advance(fuseTarget, 4), "four seconds of a five second fuse stays locked");
		ctx.True(LF_Breach.IsLocked(fuseTarget), "lock still shut before the fuse");
		ctx.True(LF_Breach.Advance(fuseTarget, 1), "the fifth second opens the lock");
		ctx.False(LF_Breach.IsLocked(fuseTarget), "fuse elapsed opens the lock");
		ctx.Equal(1, LF_Breach.GetPoliceNotifyCount(fuseTarget), "opening the lock does not notify again");

		LF_Breach.Configure(orgTarget, 0, "org-hideout");
		ctx.True(LF_Breach.Arm(orgTarget, uid), "hideout door arms");
		EL_PlayerAccount member = EL_PlayerAccountManager.GetOrCreate(orgUid);
		member.SetFaction(EL_Faction.CIVILIAN);
		ctx.False(LF_Breach.Repair(orgTarget, orgUid), "non-member repair refuses");
		LF_Breach.SetOrganizationMember(orgUid, "org-hideout");
		ctx.True(LF_Breach.Repair(orgTarget, orgUid), "owning organization can repair its door");
		ctx.True(LF_Breach.IsLocked(orgTarget), "organization repair sets the lock");
	}
}

// tier: WORLD
class EL_Test_Roadmap_BreachWorld : EL_Test
{
	override string GetName()
	{
		return "e2e/roadmap-breach";
	}

	override void Run(EL_TestContext ctx)
	{
		Resource charge = Resource.Load(LF_Breach.CHARGE_PREFAB);
		ctx.True(charge.IsValid(), "DemoBlock_M112.et loads");

		IEntity door = GetGame().GetWorld().FindEntityByName("Roadmap_Breach_Door");
		ctx.True(door != null, "Roadmap_Breach_Door is placed");
		if (ctx.FailureCount() > 0)
			return;

		IEntity chargeEntity = GetGame().GetWorld().FindEntityByName("Roadmap_Breach_Charge");
		ctx.True(chargeEntity != null, "Roadmap_Breach_Charge is placed");

		EL_LockComponent doorLock = EL_Component<EL_LockComponent>.Find(door);
		ctx.NotNull(doorLock, "door carries EL_LockComponent");
		if (ctx.FailureCount() > 0)
			return;

		ActionsManagerComponent actions = EL_Component<ActionsManagerComponent>.Find(door);
		ctx.NotNull(actions, "door carries ActionsManagerComponent");
		if (ctx.FailureCount() > 0)
			return;

		array<BaseUserAction> actionList = {};
		actions.GetActionsList(actionList);
		bool armWired = false;
		bool repairWired = false;
		LF_BreachArmAction armAction;
		foreach (BaseUserAction action : actionList)
		{
			if (LF_BreachArmAction.Cast(action))
			{
				armWired = true;
				armAction = LF_BreachArmAction.Cast(action);
			}

			if (LF_BreachRepairAction.Cast(action))
				repairWired = true;
		}

		ctx.True(armWired, "arm action is on the door");
		ctx.True(repairWired, "repair action is on the door");

		string uid = "roadmap-breach-e2e";
		string policeUid = "roadmap-breach-e2e-police";
		EL_PlayerAccountManager.GetOrCreate(uid);
		EL_PlayerAccount police = EL_PlayerAccountManager.GetOrCreate(policeUid);
		police.SetFaction(EL_Faction.POLICE);

		if (!doorLock.IsLocked())
			LF_Breach.Repair("Roadmap_Breach_Door", policeUid);

		ctx.True(doorLock.IsLocked(), "door is locked before the zero-fuse arm");

		int notifiesBefore = LF_Breach.GetPoliceNotifyCount("Roadmap_Breach_Door");
		if (armAction)
			armAction.CanBePerformedScript(door);

		ctx.Equal(notifiesBefore, LF_Breach.GetPoliceNotifyCount("Roadmap_Breach_Door"), "live offer refresh does not notify");
		ctx.True(LF_Breach.Arm("Roadmap_Breach_Door", uid), "zero-fuse arm opens the world door");
		ctx.False(doorLock.IsLocked(), "door IsLocked() is false after the fuse");
		ctx.True(LF_Breach.Repair("Roadmap_Breach_Door", policeUid), "police repair runs");
		ctx.True(doorLock.IsLocked(), "repair sets the lock");
	}
}

// tier: PERSISTENCE
class EL_Test_Roadmap_BreachPersistence : EL_Test
{
	override string GetName()
	{
		return "roadmap/breach-save";
	}

	override void Run(EL_TestContext ctx)
	{
		IEntity door = GetGame().GetWorld().FindEntityByName("Roadmap_Breach_Door");
		ctx.True(door != null, "Roadmap_Breach_Door is placed for the save round trip");
		if (ctx.FailureCount() > 0)
			return;

		EL_LockComponent doorLock = EL_Component<EL_LockComponent>.Find(door);
		ctx.NotNull(doorLock, "door lock is present for the save round trip");
		if (ctx.FailureCount() > 0)
			return;

		string uid = "roadmap-breach-save";
		string policeUid = "roadmap-breach-save-police";
		EL_PlayerAccountManager.GetOrCreate(uid);
		EL_PlayerAccount police = EL_PlayerAccountManager.GetOrCreate(policeUid);
		police.SetFaction(EL_Faction.POLICE);

		if (!doorLock.IsLocked())
			LF_Breach.Repair("Roadmap_Breach_Door", policeUid);

		ctx.True(doorLock.IsLocked(), "door is locked before the save arm");
		ctx.True(LF_Breach.Arm("Roadmap_Breach_Door", uid), "arm opens the door before the save");
		ctx.False(doorLock.IsLocked(), "door is open in the live session");

		LF_BreachRecord saved = LF_Breach.Export("Roadmap_Breach_Door");
		ctx.NotNull(saved, "export returns the open record");
		if (ctx.FailureCount() > 0)
			return;

		ctx.True(saved.m_bOpen, "saved record is open");
		ctx.True(LF_Breach.Repair("Roadmap_Breach_Door", policeUid), "live state is locked before apply");
		ctx.True(doorLock.IsLocked(), "live lock is shut before the saved open state is applied");
		ctx.True(LF_Breach.Apply(saved), "apply accepts the saved record");
		ctx.False(doorLock.IsLocked(), "open lock survives apply-save");
		ctx.True(LF_Breach.Apply(saved), "apply is idempotent");
		ctx.False(doorLock.IsLocked(), "a second apply stays open");
	}
}
