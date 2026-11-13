// red-proof: Observed 2026-11-08 with `tools\cli test --no-build --tier all`: commenting out
// `state.m_sOrganizationId = organizationId;` in LF_Hideout.Assign failed all three tests here:
// `[LFTEST] FAIL roadmap/hideout` (`- ownership is the organization id (expected '<org id>', got '')`),
// `[LFTEST] FAIL e2e/roadmap-hideout` (`- the lock identifier is the organization id`), and
// `[LFTEST] FAIL roadmap/hideout-save` (`- one hideout record is exported (expected 1, got 0)`).
// Restored; green.

// tier: LOGIC
class LF_Test_Roadmap_Hideout : LF_Test
{
	override string GetName()
	{
		return "roadmap/hideout";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_OrganizationManager.Reset();
		LF_Hideout.Reset();

		string founder = "roadmap-hideout-founder";
		string member = "roadmap-hideout-member";
		string outsider = "roadmap-hideout-outsider";
		string hideoutId = LF_Hideout.HIDEOUT_ID;

		LF_Organization org = LF_OrganizationManager.Create(founder, "Hideout Outfit");
		ctx.NotNull(org, "founder creates the owning organization");
		if (!org)
			return;

		ctx.True(LF_OrganizationManager.Invite(org.GetId(), founder, member), "founder adds a member");
		ctx.True(LF_Hideout.Assign(hideoutId, org.GetId()), "the organization owns the hideout");
		ctx.EqualStr(org.GetId(), LF_Hideout.GetOwner(hideoutId), "ownership is the organization id");
		ctx.False(LF_Hideout.Assign(hideoutId, org.GetId()) == false, "assigning the same organization again is kept");

		ctx.False(LF_Hideout.TryUnlock(outsider, hideoutId), "a non-member is refused at the door");
		ctx.EqualStr("door refused: not a member", LF_Hideout.GetLastRefusal(), "the non-member refusal is logged");
		ctx.True(LF_Hideout.IsLocked(hideoutId), "a refused unlock leaves the door locked");

		ctx.True(LF_Hideout.TryUnlock(member, hideoutId), "a member unlocks the door");
		ctx.False(LF_Hideout.IsLocked(hideoutId), "the member unlock opens the lock");
		ctx.True(LF_Hideout.LockDoor(member, hideoutId), "a member locks the door");
		ctx.True(LF_Hideout.IsLocked(hideoutId), "the member lock closes the door");

		ctx.False(LF_Hideout.CanProcess(outsider, hideoutId), "a non-member cannot run the machine");
		ctx.EqualStr("process refused: organization does not own the hideout", LF_Hideout.GetLastRefusal(), "the non-member machine refusal is logged");
		ctx.False(LF_Hideout.CanProcess(member, hideoutId), "the machine refuses without the processing upgrade");
		ctx.EqualStr("process refused: no processing upgrade", LF_Hideout.GetLastRefusal(), "the missing upgrade is logged");

		int price = LF_OrgConfig.PriceProcessingSlot();
		ctx.True(LF_OrganizationManager.Deposit(org.GetId(), founder, price), "founder deposits the upgrade price");
		ctx.Equal(price, org.GetFund(), "fund holds the processing price");
		ctx.True(LF_OrganizationManager.BuyUpgrade(org.GetId(), founder, LF_EOrganizationUpgrade.PROCESSING_SLOT), "founder buys the processing slot");
		ctx.True(LF_Hideout.CanProcess(member, hideoutId), "a member processes once the organization owns the upgrade");
		ctx.False(LF_Hideout.CanProcess(outsider, hideoutId), "a non-member is still refused after the upgrade");
		ctx.EqualStr("process refused: organization does not own the hideout", LF_Hideout.GetLastRefusal(), "the upgraded machine still logs a non-member");

		LF_OrganizationManager.Reset();
		LF_Hideout.Reset();
	}
}

// tier: WORLD
class LF_Test_Roadmap_HideoutWorld : LF_Test
{
	override string GetName()
	{
		return "e2e/roadmap-hideout";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_OrganizationManager.Reset();
		LF_Hideout.Reset();

		IEntity door = GetGame().GetWorld().FindEntityByName(LF_TestAnnex.HIDEOUT_DOOR);
		ctx.True(door != null, "Annex_Hideout_Door is placed");
		IEntity machine = GetGame().GetWorld().FindEntityByName(LF_TestAnnex.HIDEOUT_MACHINE);
		ctx.True(machine != null, "Annex_Hideout_Machine is placed");
		IEntity stash = GetGame().GetWorld().FindEntityByName(LF_TestAnnex.HIDEOUT_STASH);
		ctx.True(stash != null, "Annex_Hideout_Stash is placed");
		if (ctx.FailureCount() > 0)
			return;

		ctx.EqualFloat(0, door.GetOrigin()[1], 0.1, "the door stands on the ground");
		ctx.EqualFloat(0, machine.GetOrigin()[1], 0.1, "the machine stands on the ground");
		ctx.EqualFloat(0, stash.GetOrigin()[1], 0.1, "the stash stands on the ground");

		ctx.True(LF_Component<LF_BreachTargetComponent>.Find(door) == null, "the hideout door is not a breach target");
		LF_LockComponent doorLock = LF_Component<LF_LockComponent>.Find(door);
		ctx.NotNull(doorLock, "the hideout door carries LF_LockComponent");
		LF_HideoutDoorComponent hideoutDoor = LF_Component<LF_HideoutDoorComponent>.Find(door);
		ctx.NotNull(hideoutDoor, "the hideout door carries LF_HideoutDoorComponent");
		if (ctx.FailureCount() > 0)
			return;

		ctx.EqualStr(LF_TestAnnex.HIDEOUT_ID, hideoutDoor.GetHideoutId(), "the door is the roadmap hideout");

		LF_HideoutMachineComponent hideoutMachine = LF_Component<LF_HideoutMachineComponent>.Find(machine);
		ctx.NotNull(hideoutMachine, "the machine carries LF_HideoutMachineComponent");
		ActionsManagerComponent actions = LF_Component<ActionsManagerComponent>.Find(machine);
		ctx.NotNull(actions, "the machine carries the processor action manager");
		if (ctx.FailureCount() > 0)
			return;

		ctx.EqualStr(LF_TestAnnex.HIDEOUT_ID, hideoutMachine.GetHideoutId(), "the machine belongs to the hideout");

		array<BaseUserAction> actionList = {};
		actions.GetActionsList(actionList);
		LF_ProcessAction process;
		foreach (BaseUserAction action : actionList)
		{
			LF_ProcessAction candidate = LF_ProcessAction.Cast(action);
			if (candidate)
				process = candidate;
		}

		ctx.NotNull(process, "the machine uses the existing process action");
		if (!process || !process.m_aProcessingOutputs || process.m_aProcessingOutputs.Count() < 1)
		{
			ctx.Fail("the machine recipe has a contraband output");
			return;
		}

		ctx.True(LF_Contraband.IsContraband(process.m_aProcessingOutputs[0].m_OutputPrefab), "the machine output is the configured contraband");
		ctx.True(stash.GetVObject() != null, "the stash is a crate mesh");

		string founder = "roadmap-hideout-e2e-founder";
		string member = "roadmap-hideout-e2e-member";
		string outsider = "roadmap-hideout-e2e-outsider";
		LF_Organization org = LF_OrganizationManager.Create(founder, "Placed Hideout");
		ctx.NotNull(org, "e2e organization exists");
		if (!org)
			return;

		ctx.True(LF_OrganizationManager.Invite(org.GetId(), founder, member), "e2e member is added");
		ctx.True(LF_Hideout.Assign(LF_TestAnnex.HIDEOUT_ID, org.GetId()), "the placed hideout is assigned");
		ctx.EqualStr(org.GetId(), doorLock.GetHouseIdentifier(), "the lock identifier is the organization id");

		ctx.True(LF_Hideout.LockDoor(member, LF_TestAnnex.HIDEOUT_ID), "member locks the placed door");
		ctx.True(doorLock.IsLocked(), "the placed door is locked");
		ctx.False(LF_Hideout.TryUnlock(outsider, LF_TestAnnex.HIDEOUT_ID), "a non-member does not unlock the placed door");
		ctx.True(doorLock.IsLocked(), "the placed door stays locked for a non-member");
		ctx.EqualStr("door refused: not a member", LF_Hideout.GetLastRefusal(), "the placed door logs the refusal");
		ctx.True(LF_Hideout.TryUnlock(member, LF_TestAnnex.HIDEOUT_ID), "a member unlocks the placed door");
		ctx.False(doorLock.IsLocked(), "the placed lock opens for a member");

		ctx.False(LF_Hideout.CanProcess(member, LF_TestAnnex.HIDEOUT_ID), "the placed machine refuses without the upgrade");
		int price = LF_OrgConfig.PriceProcessingSlot();
		ctx.True(LF_OrganizationManager.Deposit(org.GetId(), founder, price), "e2e fund receives the upgrade price");
		ctx.True(LF_OrganizationManager.BuyUpgrade(org.GetId(), founder, LF_EOrganizationUpgrade.PROCESSING_SLOT), "e2e founder buys the processing slot");
		ctx.True(LF_Hideout.CanProcess(member, LF_TestAnnex.HIDEOUT_ID), "the placed machine runs for a member with the upgrade");

		LF_OrganizationManager.Reset();
		LF_Hideout.Reset();
	}
}

// tier: PERSISTENCE
class LF_Test_Roadmap_HideoutPersistence : LF_Test
{
	override string GetName()
	{
		return "roadmap/hideout-save";
	}

	override void Run(LF_TestContext ctx)
	{
		// The e2e save boot seeds an organization earlier in this tier; put it back at the end.
		LF_TestLegacySnapshot kept = LF_TestLegacySnapshot.Take();
		RunWiped(ctx);
		kept.Restore();
	}

	protected void RunWiped(LF_TestContext ctx)
	{
		LF_OrganizationManager.Reset();
		LF_Hideout.Reset();

		string founder = "roadmap-hideout-save-founder";
		string member = "roadmap-hideout-save-member";
		LF_Organization org = LF_OrganizationManager.Create(founder, "Saved Hideout");
		ctx.NotNull(org, "organization exists before the save");
		if (!org)
			return;

		ctx.True(LF_OrganizationManager.Invite(org.GetId(), founder, member), "member is added before the save");
		ctx.True(LF_Hideout.Assign(LF_TestAnnex.HIDEOUT_ID, org.GetId()), "hideout is assigned before the save");
		ctx.True(LF_Hideout.TryUnlock(member, LF_TestAnnex.HIDEOUT_ID), "member unlocks before the save");
		ctx.False(LF_Hideout.IsLocked(LF_TestAnnex.HIDEOUT_ID), "lock is open before the save");

		string orgId = org.GetId();
		ref array<ref LF_HideoutRecord> records = LF_Hideout.ExportAll();
		ctx.NotNull(records, "export returns hideout records");
		if (!records)
			return;

		ctx.Equal(1, records.Count(), "one hideout record is exported");
		ctx.True(LF_Hideout.LockDoor(member, LF_TestAnnex.HIDEOUT_ID), "live lock is closed after the export");
		ctx.True(LF_Hideout.IsLocked(LF_TestAnnex.HIDEOUT_ID), "live state differs from the snapshot");

		LF_Hideout.ApplyAll(records);
		ctx.EqualStr(orgId, LF_Hideout.GetOwner(LF_TestAnnex.HIDEOUT_ID), "apply restores the organization id");
		ctx.False(LF_Hideout.IsLocked(LF_TestAnnex.HIDEOUT_ID), "apply restores the open lock");

		IEntity door = GetGame().GetWorld().FindEntityByName(LF_TestAnnex.HIDEOUT_DOOR);
		if (door)
		{
			LF_LockComponent doorLock = LF_Component<LF_LockComponent>.Find(door);
			ctx.NotNull(doorLock, "placed door still has a lock after apply");
			if (doorLock)
			{
				ctx.EqualStr(orgId, doorLock.GetHouseIdentifier(), "the placed lock keeps the organization id");
				ctx.False(doorLock.IsLocked(), "the placed lock stays open after apply");
			}
		}

		LF_Hideout.ApplyAll(records);
		ctx.EqualStr(orgId, LF_Hideout.GetOwner(LF_TestAnnex.HIDEOUT_ID), "second apply keeps the same owner");
		ctx.False(LF_Hideout.IsLocked(LF_TestAnnex.HIDEOUT_ID), "second apply does not relock the door");

		LF_OrganizationManager.Reset();
		LF_Hideout.Reset();
	}
}
