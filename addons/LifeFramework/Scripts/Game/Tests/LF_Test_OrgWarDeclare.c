// red-proof: 2026-11-18 `tools\cli test --no-build --red LF_Test_OrgWarDeclare` (RedProof() skips the declaration and the answer): RED PROOF OK, "the target's holder accepts inside the window (expected '', got 'unknown_war')".
// tier: WORLD
//! org/war-declare (#2668, M7 gang war W3): the desk verbs behind the war desk. An officer whose rank
//! holds DECLARE_WAR declares and the fund drops by the fee; a member without the right, a fund short
//! of the fee and a target below the member minimum are each refused with the fund unchanged; a second
//! declaration on the same pair is refused by the record and the fee goes back; the target's holder
//! accepts and the two are at war. The fee is published as an LF_OnMoneyMoved sink.
class LF_TestOrgWarFeeRecorder : LF_EventSubscriber
{
	int m_iCount;
	int m_iAmount;
	string m_sSource;
	string m_sPlayerUid;

	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnMoneyMoved moved = LF_OnMoneyMoved.Cast(busEvent);
		if (!moved || moved.m_sSource != LF_OrgWarDesk.FEE_SOURCE)
			return true;

		m_iCount++;
		m_iAmount = moved.m_iAmount;
		m_sSource = moved.m_sSource;
		m_sPlayerUid = moved.m_sPlayerUid;
		return true;
	}
}

//------------------------------------------------------------------------------------------------
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_OrgWarDeclare : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "org/war-declare";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_OrganizationManager.Reset();
		LF_OrgWars.ResetStatics();

		LF_TestOrgWarFeeRecorder fees = new LF_TestOrgWarFeeRecorder();
		LF_EventBus.Subscribe(LF_OnMoneyMoved, fees);

		RunCases(ctx, fees);

		LF_EventBus.Unsubscribe(LF_OnMoneyMoved, fees);
		LF_OrgWars.ResetStatics();
		LF_OrganizationManager.Reset();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunCases(LF_TestContext ctx, LF_TestOrgWarFeeRecorder fees)
	{
		LF_Organization a = LF_OrganizationManager.Create("war-desk-founder-a", "War Desk Alpha");
		LF_Organization b = LF_OrganizationManager.Create("war-desk-founder-b", "War Desk Bravo");
		LF_Organization c = LF_OrganizationManager.Create("war-desk-founder-c", "War Desk Charlie");
		ctx.NotNull(a, "org A is registered");
		ctx.NotNull(b, "org B is registered");
		ctx.NotNull(c, "org C is registered");
		if (!a || !b || !c)
			return;

		string officer = LF_OrganizationConfig.RANK_OFFICER;
		string member = LF_OrganizationConfig.RANK_MEMBER;
		ctx.True(LF_OrganizationManager.JoinDirect(a.GetId(), "war-desk-officer-a", officer), "A has an officer");
		ctx.True(LF_OrganizationManager.JoinDirect(a.GetId(), "war-desk-member-a", member), "A has a member");
		ctx.True(LF_OrganizationManager.JoinDirect(b.GetId(), "war-desk-officer-b", officer), "B has an officer");
		ctx.True(LF_OrganizationManager.JoinDirect(b.GetId(), "war-desk-member-b", member), "B has a member");

		// The officer rung holds the war right here, whatever the shipped default is.
		int officerRights = a.GetRankRights(officer) | LF_OrgRights.Bit(LF_EOrganizationPermission.DECLARE_WAR);
		ctx.True(a.ApplyRankRow(LF_OrganizationRankRow.Create(a.GetId(), officer, 1, officerRights)), "A's officers may declare war");
		ctx.True(a.Allows("war-desk-officer-a", LF_EOrganizationPermission.DECLARE_WAR), "A's officer holds DECLARE_WAR");
		ctx.False(a.Allows("war-desk-member-a", LF_EOrganizationPermission.DECLARE_WAR), "A's member does not");

		LF_OrgWarRules rules = new LF_OrgWarRules();
		rules.m_iFee = 500;
		rules.m_iAnswerSeconds = 600;
		rules.m_iLengthSeconds = 3600;
		rules.m_iCooldownSeconds = 7200;
		rules.m_iMinMembers = 3;
		int t0 = 1800000000;

		a.SetFund(1000);
		string warId;

		// A member without the right.
		string refusal = LF_OrgWarDesk.DeclareAt("war-desk-member-a", "uid-war-desk-member-a", b.GetId(), t0, rules, warId);
		ctx.EqualStr(LF_OrgWarDesk.REFUSE_NO_RIGHT, refusal, "a member without the right is refused");
		ctx.Equal(1000, a.GetFund(), "the fund is unchanged after the rights refusal");

		// A target below the member minimum (C has its founder only).
		refusal = LF_OrgWarDesk.DeclareAt("war-desk-officer-a", "uid-war-desk-officer-a", c.GetId(), t0, rules, warId);
		ctx.EqualStr(LF_OrgWarDesk.REFUSE_TARGET_TOO_FEW, refusal, "a target below the member minimum is refused");
		ctx.Equal(1000, a.GetFund(), "the fund is unchanged after the minimum refusal");

		// A fund short of the fee.
		a.SetFund(400);
		refusal = LF_OrgWarDesk.DeclareAt("war-desk-officer-a", "uid-war-desk-officer-a", b.GetId(), t0, rules, warId);
		ctx.EqualStr(LF_OrgWarDesk.REFUSE_FUND, refusal, "a fund short of the fee is refused");
		ctx.Equal(400, a.GetFund(), "the short fund is unchanged");
		ctx.Equal(0, fees.m_iCount, "no fee event before a declaration goes through");
		ctx.Equal(0, LF_OrgWars.Count(), "no war is recorded by a refusal");

		// The officer declares: the fee leaves the fund once.
		a.SetFund(1000);
		if (!RedProof())
			refusal = LF_OrgWarDesk.DeclareAt("war-desk-officer-a", "uid-war-desk-officer-a", b.GetId(), t0, rules, warId);

		ctx.EqualStr("", refusal, "the officer with the right declares");
		ctx.Equal(500, a.GetFund(), "the fund drops by the fee");
		ctx.Equal(1, fees.m_iCount, "the fee is published once as a money sink");
		ctx.Equal(500, fees.m_iAmount, "the sink carries the fee");
		ctx.EqualStr("uid-war-desk-officer-a", fees.m_sPlayerUid, "the sink names the declaring player uid");
		LF_OrgWarRecord war = LF_OrgWars.Get(warId);
		ctx.NotNull(war, "the declaration is recorded");

		// The same pair again: the record refuses it and the fee goes back.
		string again;
		refusal = LF_OrgWarDesk.DeclareAt("war-desk-officer-a", "uid-war-desk-officer-a", b.GetId(), t0 + 5, rules, again);
		ctx.EqualStr(LF_OrgWars.REFUSE_PENDING, refusal, "a second declaration on the pair is refused");
		ctx.Equal(500, a.GetFund(), "the refused second declaration costs nothing");
		ctx.Equal(1, fees.m_iCount, "no fee event for the refused declaration");

		// The target answers: a member without the right is refused, the founder accepts.
		refusal = LF_OrgWarDesk.AnswerAt("war-desk-member-b", warId, t0 + 30, rules);
		ctx.EqualStr(LF_OrgWarDesk.REFUSE_NO_RIGHT, refusal, "the target's member without the right cannot answer");
		refusal = LF_OrgWarDesk.AnswerAt("war-desk-officer-a", warId, t0 + 30, rules);
		ctx.EqualStr(LF_OrgWarDesk.REFUSE_NOT_TARGET, refusal, "the declarer cannot answer its own declaration");
		ctx.False(LF_OrgWars.IsAtWar(a.GetId(), b.GetId()), "no war before the target answers");

		if (!RedProof())
			refusal = LF_OrgWarDesk.AnswerAt("war-desk-founder-b", warId, t0 + 60, rules);

		ctx.EqualStr("", refusal, "the target's holder accepts inside the window");
		ctx.True(LF_OrgWars.IsAtWar(a.GetId(), b.GetId()), "the two organizations are at war");
		ctx.Equal(500, a.GetFund(), "accepting does not touch the declarer's fund");
	}
}
