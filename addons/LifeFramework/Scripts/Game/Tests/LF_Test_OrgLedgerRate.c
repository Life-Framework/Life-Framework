// red-proof: 2026-11-17, `tools\cli test --no-build --red LF_Test_OrgLedgerRate`: RED-OK, "leave is refused from the spent bucket" and "the sign's leave refuses a member of another organisation (expected 7, got 0)" with the hook on. Green with the gate and the OTHER_ORG rule.
// tier: LOGIC
//------------------------------------------------------------------------------------------------
//! organizations/ledger-rate (review 2026-11-17, items 30 and 31, #2415, slice #2536): the View,
//! Deposit, Withdraw, Leave and Lease asks go through LF_RateLimit (View walks every member), and
//! the dev-gated test sign's Leave refuses a player whose organisation is not the sign's own, so it
//! can never strip a real organisation of its founder.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_OrgLedgerRate : LF_Test
{
	protected static const int PLAYER = 992236;

	override string GetName()
	{
		return "organizations/ledger-rate";
	}

	//------------------------------------------------------------------------------------------------
	protected bool Ask(string request)
	{
		if (RedProof())
			return true; // the old behaviour: no limit on the ledger asks

		return LF_OrgLedgerGate.Admit(PLAYER, request, false);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		int admitted = 0;
		for (int i = 0; i < LF_RateLimit.PURCHASE_BURST + 6; i++)
		{
			if (Ask("view"))
				admitted++;
		}

		ctx.Equal(LF_RateLimit.PURCHASE_BURST, admitted, "a view spam is admitted up to the bucket, then refused");
		ctx.False(Ask("deposit"), "the other ledger asks share the spent bucket");
		ctx.False(Ask("withdraw"), "withdraw is refused from the spent bucket");
		ctx.False(Ask("leave"), "leave is refused from the spent bucket");
		ctx.False(Ask("lease"), "lease is refused from the spent bucket");
		ctx.True(LF_OrgLedgerGate.Admit(PLAYER + 1, "view", false), "another player has their own bucket");

		// Test sign: Leave refuses for a player in another organisation, and still works in its own.
		LF_ETestOrgRefusal other = LF_ETestOrgRefusal.NONE;
		if (RedProof())
			other = LF_TestOrgRules.Check(true, true, true, true, true, false);
		else
			other = LF_TestOrgRules.Check(true, true, true, true, true, false, false);

		ctx.Equal(LF_ETestOrgRefusal.OTHER_ORG, other, "the sign's leave refuses a member of another organisation");
		ctx.EqualStr("#LF-TestOrgSign_OtherOrg", LF_TestOrgRules.RefusalKey(LF_ETestOrgRefusal.OTHER_ORG), "the refusal has its own line");
		ctx.Equal(LF_ETestOrgRefusal.NONE, LF_TestOrgRules.Check(true, true, true, true, true, false, true), "leave still works for a member of the test organisation");
		ctx.Equal(LF_ETestOrgRefusal.NOT_IN, LF_TestOrgRules.Check(true, true, true, true, false, false, false), "no organisation still reads as not in one");
		ctx.Equal(LF_ETestOrgRefusal.ALREADY_IN, LF_TestOrgRules.Check(true, true, true, true, true, true, false), "join is unchanged");
	}
}
