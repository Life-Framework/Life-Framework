// red-proof: 2026-11-14 `tools\cli test --no-build --only LF_Test_TestOrgSign` with the expected entry rank changed to founder failed `later ones join at the entry rank (expected 'founder', got 'member')`. Restored; green.
// tier: LOGIC
//! Test org sign (#934): the gate, the founder rule, the direct join and leave the sign uses, and
//! the wording of the hideout door ("Members only (<org>)" for a stranger, "Open" for a member).
class LF_Test_TestOrgSign : LF_Test
{
	override string GetName()
	{
		return "organizations/test-org-sign";
	}

	override void Run(LF_TestContext ctx)
	{
		// The gate: playtest mode, dev gate, a body, in reach, and the right side of membership.
		ctx.Equal(LF_ETestOrgRefusal.NONE, LF_TestOrgRules.Check(true, true, true, true, false, true), "join: allowed in the playtest town");
		ctx.Equal(LF_ETestOrgRefusal.NOT_PLAYTEST, LF_TestOrgRules.Check(false, true, true, true, false, true), "join: refused outside a playtest mode");
		ctx.Equal(LF_ETestOrgRefusal.NOT_ALLOWED, LF_TestOrgRules.Check(true, false, true, true, false, true), "join: refused without the dev gate");
		ctx.Equal(LF_ETestOrgRefusal.NO_CHARACTER, LF_TestOrgRules.Check(true, true, false, true, false, true), "join: refused with no character");
		ctx.Equal(LF_ETestOrgRefusal.TOO_FAR, LF_TestOrgRules.Check(true, true, true, false, false, true), "join: refused from afar");
		ctx.Equal(LF_ETestOrgRefusal.ALREADY_IN, LF_TestOrgRules.Check(true, true, true, true, true, true), "join: refused when already in an organisation");
		ctx.Equal(LF_ETestOrgRefusal.NONE, LF_TestOrgRules.Check(true, true, true, true, true, false), "leave: allowed for a member");
		ctx.Equal(LF_ETestOrgRefusal.NOT_IN, LF_TestOrgRules.Check(true, true, true, true, false, false), "leave: refused with no organisation");
		ctx.EqualStr("#LF-TestOrgSign_Denied", LF_TestOrgRules.RefusalKey(LF_ETestOrgRefusal.NOT_PLAYTEST), "a closed gate reads as playtest only");
		ctx.EqualStr("", LF_TestOrgRules.RefusalKey(LF_ETestOrgRefusal.NONE), "no refusal has no line");

		ctx.EqualStr(LF_OrganizationConfig.RANK_FOUNDER, LF_TestOrgRules.RankFor(false, "member"), "the first one in is the founder");
		ctx.EqualStr("member", LF_TestOrgRules.RankFor(true, "member"), "later ones join at the entry rank");

		// What the sign does to the register: found, join, leave (founder included), rejoin.
		LF_OrganizationManager.Reset();
		LF_Hideout.Reset();
		string first = "test-org-sign-first";
		string second = "test-org-sign-second";

		ctx.True(LF_OrganizationManager.FindByName(LF_TestOrgRules.ORG_NAME) == null, "no test organisation before the first join");
		LF_Organization org = LF_OrganizationManager.Create(first, LF_TestOrgRules.ORG_NAME);
		ctx.NotNull(org, "the first joiner founds the test organisation");
		if (!org)
			return;

		ctx.True(LF_OrganizationManager.FindByName("  test ORGANISATION ") == org, "it is found by name, case and spaces aside");
		ctx.True(LF_OrgHideoutLease.StateFor(org.GetId(), LF_Hideout.HIDEOUT_ID) == LF_EOrgLease.FREE, "the yard is free before the lease");
		ctx.True(LF_OrgHideoutLease.Lease(first, LF_Hideout.HIDEOUT_ID), "the founder takes the yard");
		ctx.EqualStr(org.GetId(), LF_Hideout.GetOwner(LF_Hideout.HIDEOUT_ID), "the test organisation owns the yard");

		ctx.True(LF_OrganizationManager.JoinDirect(org.GetId(), second, org.EntryRankId()), "a second tester joins with no invite");
		ctx.False(LF_OrganizationManager.JoinDirect(org.GetId(), second, org.EntryRankId()), "joining twice is refused");
		ctx.True(org.IsMember(second), "the second tester is a member");
		ctx.True(LF_Hideout.AllowsUnlock(second, LF_Hideout.HIDEOUT_ID, false), "a member opens the hideout door");
		ctx.True(LF_Hideout.AllowsUnlock(first, LF_Hideout.HIDEOUT_ID, false), "the founder opens the hideout door");

		ctx.False(LF_OrganizationManager.Leave(first), "the normal leave keeps the founder in");
		ctx.True(LF_OrganizationManager.LeaveDirect(first), "the sign takes the founder out");
		ctx.False(LF_OrganizationManager.GetByCharacter(first) != null, "the founder belongs to nothing now");
		ctx.False(LF_Hideout.AllowsUnlock(first, LF_Hideout.HIDEOUT_ID, false), "a leaver is shut out of the door");
		ctx.False(LF_OrganizationManager.LeaveDirect(first), "leaving twice is refused");
		ctx.True(LF_OrganizationManager.JoinDirect(org.GetId(), first, LF_TestOrgRules.RankFor(false, org.EntryRankId())), "the leaver can join again");

		LF_OrganizationManager.Reset();
		LF_Hideout.Reset();

		// The door's words.
		LF_EHideoutDoorView mine = LF_HideoutDoorRules.View("org-a", "org-a");
		LF_EHideoutDoorView theirs = LF_HideoutDoorRules.View("org-a", "org-b");
		LF_EHideoutDoorView nobodys = LF_HideoutDoorRules.View("", "org-b");
		ctx.Equal(LF_EHideoutDoorView.MEMBER, mine, "a member of the owner sees their own door");
		ctx.Equal(LF_EHideoutDoorView.MEMBERS_ONLY, theirs, "a stranger sees a members-only door");
		ctx.Equal(LF_EHideoutDoorView.MEMBERS_ONLY, LF_HideoutDoorRules.View("org-a", ""), "someone with no organisation is a stranger");
		ctx.Equal(LF_EHideoutDoorView.UNOWNED, nobodys, "a yard nobody holds reads as unowned");
		ctx.EqualStr("#LF-Hideout_Open", LF_HideoutDoorRules.LabelKey(mine, "North Dock"), "a member's prompt is Open");
		ctx.EqualStr("#LF-Hideout_MembersOnly", LF_HideoutDoorRules.LabelKey(theirs, "North Dock"), "a stranger's prompt names the organisation");
		ctx.EqualStr("#LF-Hideout_MembersOnlyPlain", LF_HideoutDoorRules.LabelKey(theirs, ""), "no name yet: plain members only");
		ctx.EqualStr("#LF-Hideout_Unowned", LF_HideoutDoorRules.LabelKey(nobodys, ""), "an unowned door says so");
		ctx.EqualStr("", LF_HideoutDoorRules.ReasonKey(mine, "North Dock"), "a member has no refusal");
		ctx.EqualStr("#LF-Hideout_MembersOnlyReason", LF_HideoutDoorRules.ReasonKey(theirs, "North Dock"), "a stranger is told whose door it is");
		ctx.EqualStr("#LF-Hideout_UnownedReason", LF_HideoutDoorRules.ReasonKey(nobodys, ""), "an unowned door says how to get it");
	}
}
