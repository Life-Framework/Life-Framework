// red-proof: the batched `--red` run (RedProof() skips the officer's join and the lapse tick) fails "the shipped officer holds DECLARE_WAR" and "a declaration that lapsed starts the pair cooldown"; see the PR for the RED-OK line.
// tier: LOGIC
//! org/war-lapse-cooldown (#2769, from the #2668 review): the shipped officer rung holds DECLARE_WAR (a
//! member does not), so a declaration is not lost whenever the founder is offline; a LAPSED declaration
//! starts the same-pair lapse cooldown (either side refused inside it, free after), a lapse cooldown of 0
//! turns it off, and an answered war still uses the full war cooldown.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_OrgWarLapseCooldown : LF_Test
{
	override string GetName()
	{
		return "org/war-lapse-cooldown";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_OrganizationManager.Reset();
		LF_OrgWars.ResetStatics();

		LF_Organization a = LF_OrganizationManager.Create("war-lapse-founder-a", "War Lapse Alpha");
		LF_Organization b = LF_OrganizationManager.Create("war-lapse-founder-b", "War Lapse Bravo");
		ctx.NotNull(a, "org A is registered");
		ctx.NotNull(b, "org B is registered");
		if (!a || !b)
			return;

		// The shipped rungs: the officer may declare and answer, the member may not.
		string officer = LF_OrganizationConfig.RANK_OFFICER;
		string member = LF_OrganizationConfig.RANK_MEMBER;
		if (!RedProof())
			LF_OrganizationManager.JoinDirect(a.GetId(), "war-lapse-officer-a", officer);

		LF_OrganizationManager.JoinDirect(a.GetId(), "war-lapse-member-a", member);
		ctx.True(a.Allows("war-lapse-officer-a", LF_EOrganizationPermission.DECLARE_WAR), "the shipped officer holds DECLARE_WAR");
		ctx.False(a.Allows("war-lapse-member-a", LF_EOrganizationPermission.DECLARE_WAR), "the shipped member does not");
		ctx.True(a.Allows("war-lapse-founder-a", LF_EOrganizationPermission.DECLARE_WAR), "the founder holds DECLARE_WAR");
		int shippedOfficer = LF_OrganizationConfig.ShippedRights(officer);
		ctx.True((shippedOfficer & LF_OrgRights.Bit(LF_EOrganizationPermission.DECLARE_WAR)) != 0, "ShippedRights(officer) carries DECLARE_WAR");

		LF_OrgWarRules rules = new LF_OrgWarRules();
		ctx.Equal(3600, rules.m_iLapseCooldownSeconds, "the core lapse cooldown is one hour");
		rules.m_iAnswerSeconds = 600;
		rules.m_iLengthSeconds = 3600;
		rules.m_iCooldownSeconds = 7200;
		int t0 = 1800000000;

		// An unanswered declaration lapses and starts the pair cooldown, for both sides.
		LF_OrgWarRecord first;
		LF_OrgWarRecord none;
		ctx.EqualStr("", LF_OrgWars.Declare(a.GetId(), b.GetId(), t0, rules, first), "A declares on B");
		if (!RedProof())
			LF_OrgWars.Tick(t0 + 601);

		ctx.Equal(LF_EOrgWarState.LAPSED, first.m_eState, "an unanswered declaration lapses");
		ctx.Equal(t0 + 600 + 3600, first.m_iCooldownUntil, "the lapse cooldown runs from the end of the answer window");
		ctx.EqualStr(LF_OrgWars.REFUSE_COOLDOWN, LF_OrgWars.Declare(a.GetId(), b.GetId(), first.m_iCooldownUntil - 1, rules, none), "a declaration that lapsed starts the pair cooldown");
		ctx.EqualStr(LF_OrgWars.REFUSE_COOLDOWN, LF_OrgWars.Declare(b.GetId(), a.GetId(), first.m_iCooldownUntil - 1, rules, none), "the lapse cooldown holds for the other side too");
		LF_OrgWarRecord second;
		ctx.EqualStr("", LF_OrgWars.Declare(a.GetId(), b.GetId(), first.m_iCooldownUntil, rules, second), "the pair can declare once the lapse cooldown is over");
		ctx.NotNull(second, "the second declaration is recorded");
		if (!second)
			return;


		// An accepted war uses the full war cooldown, not the lapse one.
		ctx.EqualStr("", LF_OrgWars.Accept(second.m_sWarId, first.m_iCooldownUntil + 10, rules), "B answers the second declaration");
		LF_OrgWars.Tick(second.m_iEndsAt);
		ctx.Equal(LF_EOrgWarState.ENDED, second.m_eState, "the answered war ends at its time");
		ctx.Equal(second.m_iEndsAt + 7200, second.m_iCooldownUntil, "an answered war starts the full war cooldown");

		// A lapse cooldown of 0 is no cooldown.
		LF_OrgWars.ResetStatics();
		rules.m_iLapseCooldownSeconds = 0;
		LF_OrgWarRecord open;
		ctx.EqualStr("", LF_OrgWars.Declare(a.GetId(), b.GetId(), t0, rules, open), "A declares on B with no lapse cooldown");
		LF_OrgWars.Tick(t0 + 601);
		ctx.Equal(LF_EOrgWarState.LAPSED, open.m_eState, "it lapses");
		LF_OrgWarRecord redeclare;
		ctx.EqualStr("", LF_OrgWars.Declare(a.GetId(), b.GetId(), t0 + 601, rules, redeclare), "with a zero lapse cooldown the pair may declare at once");

		LF_OrgWars.ResetStatics();
		LF_OrganizationManager.Reset();
	}
}
