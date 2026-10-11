// red-proof: expected fund 0 after a 100 deposit and ran `tools\cli test --no-build --tier fast`
// on 2026-10-11. The suite reported
// `[ELTEST] - deposit of 100 reaches the fund (expected 0, got 100)`.
// Restored the expected value to 100.
// The same command on 2026-10-11 with OpensThroughMenuManager forced true reported
// `[ELTEST] - organization menu opens on the workspace` and `failed=1`. Restored it to false.

// tier: LOGIC
class EL_Test_Roadmap_Organizations : EL_Test
{
	override string GetName()
	{
		return "roadmap/organizations";
	}

	override void Run(EL_TestContext ctx)
	{
		EL_OrganizationManager.Reset();

		string founder = "roadmap-org-founder";
		string member = "roadmap-org-member";
		string outsider = "roadmap-org-outsider";
		string secondFounder = "roadmap-org-second-founder";

		ctx.True(EL_OrganizationManager.Create("", "North Dock") == null, "empty character cannot found an organization");
		ctx.True(EL_OrganizationManager.Create(founder, "") == null, "empty name is refused");

		EL_Organization org = EL_OrganizationManager.Create(founder, "North Dock");
		ctx.NotNull(org, "founder creates an organization");
		if (!org)
			return;

		ctx.False(org.GetId().IsEmpty(), "organization id is assigned");
		ctx.EqualStr("North Dock", org.GetName(), "organization keeps its name");
		ctx.Equal(1, org.GetMemberCount(), "founder is the first member");
		ctx.True(org.IsMember(founder), "founder is a member");
		ctx.Equal(0, org.GetFund(), "fund starts empty");
		ctx.False(org.HasProcessingSlot(), "processing slot starts off");

		ctx.True(EL_OrganizationManager.Create(founder, "Second Outfit") == null, "one membership per character");

		ctx.True(EL_OrganizationManager.Invite(org.GetId(), founder, member), "founder adds a member");
		ctx.Equal(2, org.GetMemberCount(), "member was added");
		ctx.True(org.IsMember(member), "added character is a member");
		ctx.False(EL_OrganizationManager.Invite(org.GetId(), member, outsider), "member rank cannot invite");

		EL_Organization other = EL_OrganizationManager.Create(secondFounder, "South Dock");
		ctx.NotNull(other, "a second founder creates a separate organization");
		if (!other)
			return;

		ctx.False(EL_OrganizationManager.Invite(other.GetId(), secondFounder, member), "a character cannot join a second organization");
		ctx.True(EL_OrganizationManager.GetByCharacter(member) == org, "membership stays on the first organization");

		EL_BankAccount personal = EL_ATMManager.GetOrCreate("roadmap-org-atm");
		ctx.NotNull(personal, "personal ATM account exists");
		if (!personal)
			return;

		personal.SetBalance(900);

		ctx.True(EL_OrganizationManager.Deposit(org.GetId(), member, 100), "member deposits cash into the fund");
		ctx.Equal(100, org.GetFund(), "deposit of 100 reaches the fund");
		ctx.Equal(900, personal.GetBalance(), "deposit does not change the personal ATM balance");

		ctx.False(EL_OrganizationManager.Withdraw(org.GetId(), member, 50), "withdraw without rank refuses");
		ctx.Equal(100, org.GetFund(), "refused withdraw leaves the fund");
		ctx.Equal(900, personal.GetBalance(), "refused withdraw leaves the personal ATM balance");

		ctx.False(EL_OrganizationManager.BuyUpgrade(org.GetId(), founder, EL_EOrganizationUpgrade.PROCESSING_SLOT), "upgrade price is all-or-nothing");
		ctx.Equal(100, org.GetFund(), "short fund is not charged");
		ctx.False(org.HasProcessingSlot(), "short fund does not set the flag");
		ctx.Equal(900, personal.GetBalance(), "a refused upgrade leaves the personal ATM balance");

		ctx.True(EL_OrganizationManager.Deposit(org.GetId(), founder, 150), "founder deposits the rest of the price");
		ctx.Equal(250, org.GetFund(), "fund holds the full price");
		ctx.False(EL_OrganizationManager.BuyUpgrade(org.GetId(), member, EL_EOrganizationUpgrade.PROCESSING_SLOT), "member rank cannot buy an upgrade");
		ctx.Equal(250, org.GetFund(), "refused buy leaves the fund");

		ctx.True(EL_OrganizationManager.BuyUpgrade(org.GetId(), founder, EL_EOrganizationUpgrade.PROCESSING_SLOT), "founder buys the processing slot");
		ctx.True(org.HasProcessingSlot(), "processing slot flag is set");
		ctx.Equal(0, org.GetFund(), "the whole price left the fund");
		ctx.False(org.HasExtraStorage(), "extra storage was not bought");
		ctx.False(org.HasParkedVehicle(), "parked vehicle was not bought");
		ctx.Equal(900, personal.GetBalance(), "the fund pays the upgrade and the personal ATM balance stays");

		ctx.False(EL_OrganizationManager.BuyUpgrade(org.GetId(), founder, EL_EOrganizationUpgrade.PROCESSING_SLOT), "the same upgrade is not bought twice");
		ctx.Equal(0, org.GetFund(), "a second buy does not charge the fund");

		ctx.True(EL_OrganizationManager.Deposit(org.GetId(), founder, 40), "founder deposits a withdraw amount");
		ctx.False(EL_OrganizationManager.Withdraw(org.GetId(), member, 40), "member still cannot withdraw");
		ctx.Equal(40, org.GetFund(), "member withdraw did not take the fund");
		ctx.True(EL_OrganizationManager.Withdraw(org.GetId(), founder, 40), "founder rank can withdraw");
		ctx.Equal(0, org.GetFund(), "founder withdraw takes the fund");
		ctx.Equal(900, personal.GetBalance(), "fund withdraw does not change the personal ATM balance");

		ctx.False(EL_OrganizationManager.Leave(founder), "founder cannot leave");
		ctx.True(org.IsMember(founder), "founder is still a member");
		ctx.True(EL_OrganizationManager.Leave(member), "member can leave");
		ctx.False(org.IsMember(member), "left character is no longer a member");
		ctx.True(EL_OrganizationManager.GetByCharacter(member) == null, "leaving clears the membership");

		ctx.True(EL_OrganizationManager.Invite(org.GetId(), founder, member), "founder can invite the member back");
		ctx.False(EL_OrganizationManager.Kick(org.GetId(), member, founder), "member cannot kick the founder");
		ctx.True(org.IsMember(founder), "founder remains after the refused kick");
		ctx.True(EL_OrganizationManager.Kick(org.GetId(), founder, member), "founder can kick a member");
		ctx.False(org.IsMember(member), "kicked character is gone");
		ctx.False(EL_OrganizationManager.Kick(org.GetId(), founder, founder), "founder cannot be kicked");
		ctx.True(org.IsMember(founder), "founder remains in the organization");
		ctx.EqualStr("{977FCAE08556886B}UI/Layouts/OrganizationMenu.layout", EL_OrganizationMenu.LayoutResource(), "organization menu keeps its layout guid");
		ctx.False(EL_OrganizationMenu.OpensThroughMenuManager(), "organization menu opens on the workspace");

		EL_OrganizationManager.Reset();
	}
}

// tier: WORLD
class EL_Test_Roadmap_OrganizationsWorld : EL_Test
{
	override string GetName()
	{
		return "e2e/roadmap-organizations";
	}

	override void Run(EL_TestContext ctx)
	{
		Resource deskPrefab = Resource.Load("{6B7B7CCC74AACA67}Prefabs/Props/Furniture/Desk_01_short.et");
		ctx.True(deskPrefab != null && deskPrefab.IsValid(), "Desk_01_short.et loads");

		Resource cash = Resource.Load(EL_MoneyUtils.PREFAB_CASH);
		ctx.True(cash != null && cash.IsValid(), "MoneyStack loads");

		Resource layout = Resource.Load("{977FCAE08556886B}UI/Layouts/OrganizationMenu.layout");
		ctx.True(layout != null && layout.IsValid(), "organization menu layout loads");

		IEntity desk = GetGame().GetWorld().FindEntityByName("Roadmap_Org_Desk");
		ctx.True(desk != null, "Roadmap_Org_Desk is placed");
		if (ctx.FailureCount() > 0)
			return;

		ActionsManagerComponent actions = EL_Component<ActionsManagerComponent>.Find(desk);
		ctx.NotNull(actions, "desk carries ActionsManagerComponent");
		if (ctx.FailureCount() > 0)
			return;

		array<BaseUserAction> actionList = {};
		actions.GetActionsList(actionList);
		bool deskAction = false;
		foreach (BaseUserAction action : actionList)
		{
			if (EL_OrganizationDeskAction.Cast(action))
				deskAction = true;
		}

		ctx.True(deskAction, "organization desk action is on the desk");

		EL_OrganizationManager.Reset();
		string founder = "roadmap-org-e2e-founder";
		string member = "roadmap-org-e2e-member";
		EL_Organization org = EL_OrganizationManager.Create(founder, "Roadmap Outfit");
		ctx.NotNull(org, "create an organization at the desk");
		if (!org)
			return;

		ctx.True(EL_OrganizationManager.Invite(org.GetId(), founder, member), "add a member");
		int amount = EL_OrganizationManager.PRICE_PROCESSING_SLOT;
		ctx.True(EL_OrganizationManager.Deposit(org.GetId(), founder, amount), "deposit a fixed cash amount");
		ctx.Equal(amount, org.GetFund(), "fund holds the fixed deposit");
		ctx.True(EL_OrganizationManager.BuyUpgrade(org.GetId(), founder, EL_EOrganizationUpgrade.PROCESSING_SLOT), "buy the processing-slot flag");
		ctx.True(org.HasProcessingSlot(), "processing slot is owned");
		ctx.Equal(0, org.GetFund(), "the fixed deposit paid the upgrade");
		EL_OrganizationManager.Reset();
	}
}

// tier: PERSISTENCE
class EL_Test_Roadmap_OrganizationsPersistence : EL_Test
{
	override string GetName()
	{
		return "roadmap/organizations-save";
	}

	override void Run(EL_TestContext ctx)
	{
		EL_OrganizationManager.Reset();

		string founder = "roadmap-org-save-founder";
		string member = "roadmap-org-save-member";
		EL_Organization org = EL_OrganizationManager.Create(founder, "Saved Outfit");
		ctx.NotNull(org, "organization exists before the save");
		if (!org)
			return;

		ctx.True(EL_OrganizationManager.Invite(org.GetId(), founder, member), "member is added before the save");
		ctx.True(EL_OrganizationManager.Deposit(org.GetId(), founder, 400), "fund is filled before the save");
		ctx.True(EL_OrganizationManager.BuyUpgrade(org.GetId(), founder, EL_EOrganizationUpgrade.PROCESSING_SLOT), "processing slot is bought before the save");
		ctx.Equal(150, org.GetFund(), "fund is 150 before the save");

		string orgId = org.GetId();
		ref array<ref EL_OrganizationRecord> records = EL_OrganizationManager.ExportAll();
		ctx.NotNull(records, "export returns records");
		if (!records)
			return;

		EL_OrganizationManager.Reset();
		ctx.True(EL_OrganizationManager.Get(orgId) == null, "reset drops the live organization");
		EL_OrganizationManager.ApplyAll(records);

		EL_Organization restored = EL_OrganizationManager.Get(orgId);
		ctx.NotNull(restored, "apply restores the organization");
		if (!restored)
			return;

		ctx.EqualStr("Saved Outfit", restored.GetName(), "name survives apply-save");
		ctx.True(restored.IsMember(founder), "founder survives apply-save");
		ctx.True(restored.IsMember(member), "member survives apply-save");
		ctx.Equal(2, restored.GetMemberCount(), "member count survives apply-save");
		ctx.Equal(150, restored.GetFund(), "fund survives apply-save");
		ctx.True(restored.HasProcessingSlot(), "processing slot survives apply-save");
		ctx.False(restored.HasExtraStorage(), "unset extra storage stays unset");
		ctx.False(EL_OrganizationManager.Withdraw(orgId, member, 10), "member rank still cannot withdraw after apply");
		ctx.Equal(150, restored.GetFund(), "refused withdraw after apply leaves the fund");

		EL_OrganizationManager.ApplyAll(records);
		EL_Organization again = EL_OrganizationManager.Get(orgId);
		ctx.NotNull(again, "second apply still finds the organization");
		if (!again)
			return;

		ctx.Equal(150, again.GetFund(), "second apply does not compound the fund");
		ctx.Equal(2, again.GetMemberCount(), "second apply does not duplicate members");
		ctx.True(again.HasProcessingSlot(), "second apply keeps the flag");

		EL_OrganizationManager.Reset();
	}
}
