// red-proof: Observed 2026-11-17 `tools/cli test --no-build --only LF_Test_ContrabandCooldown` with the driven check and the cooldown check disabled failed `crime/contraband-cooldown` with `- a crate walked to the drop pays nothing (expected 0, got 200)` and `- a paid character cannot restart inside the cooldown`. Restored: green.
// tier: WORLD (needs DebugWorld's placed organization hideout door, like roadmap/contraband)
//! #2419: a contraband run is paid only for a crate that was driven (a run begun on foot arms a
//! vehicle watch, so a walk to the drop never fails), and a character that was paid waits out a
//! cooldown before the next run.
class LF_Test_ContrabandCooldown : LF_Test
{
	override string GetName()
	{
		return "crime/contraband-cooldown";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_OrganizationManager.Reset();
		LF_Hideout.Reset();
		LF_Contraband.Reset();

		string founder = "contraband-cooldown-founder";
		string member = "contraband-cooldown-member";
		LF_Organization org = LF_OrganizationManager.Create(founder, "Cooldown Outfit");
		ctx.NotNull(org, "founder creates the owning organization");
		if (!org)
			return;

		ctx.True(LF_OrganizationManager.Invite(org.GetId(), founder, member), "founder adds the courier");
		ctx.True(LF_Hideout.Assign(LF_Hideout.OrgHideoutId(), org.GetId()), "the organization owns the hideout");

		ctx.True(LF_Contraband.Start(member, null), "a member starts the convoy");
		ctx.Equal(0, LF_Contraband.Deliver(member, 1, null), "a crate walked to the drop pays nothing");
		ctx.EqualStr("delivery refused: the crate was not driven", LF_Contraband.GetLastRefusal(), "the walk is logged");
		ctx.Equal(0, LF_Contraband.GetCashPaid(member), "the walk records no cash");

		LF_Contraband.MarkDriven(member);
		ctx.Equal(LF_Contraband.PAYOUT, LF_Contraband.Deliver(member, 1, null), "a driven crate is paid");
		int paidAt = System.GetTickCount();

		ctx.True(LF_Contraband.CooldownLeftMs(member, paidAt + 1000) > 0, "the cooldown runs after the payout");
		ctx.False(LF_Contraband.Start(member, null, paidAt + LF_Contraband.COOLDOWN_MS - 5000), "a paid character cannot restart inside the cooldown");
		ctx.True(LF_Contraband.GetLastRefusal().StartsWith("convoy refused: cooldown"), "the cooldown refusal is logged");
		ctx.EqualStr(LF_Contraband.COOLDOWN_KEY, LF_Contraband.StartRefusalKey(member, paidAt + 1000), "the player is told why");
		ctx.Equal(0, LF_Contraband.Deliver(member, 1, null), "a paid run pays nothing twice");

		int after = paidAt + LF_Contraband.COOLDOWN_MS + 5000;
		ctx.EqualStr("", LF_Contraband.StartRefusalKey(member, after), "the cooldown ends");
		ctx.True(LF_Contraband.Start(member, null, after), "a paid character starts again once the cooldown ends");
		ctx.Equal(0, LF_Contraband.Deliver(member, 1, null), "the new run starts undriven: walking it pays nothing");
		LF_Contraband.MarkDriven(member);
		ctx.Equal(LF_Contraband.PAYOUT, LF_Contraband.Deliver(member, 1, null), "the new run pays once it was driven");
		ctx.Equal(2 * LF_Contraband.PAYOUT, LF_Contraband.GetCashPaid(member), "two paid runs, two payouts");

		LF_OrganizationManager.Reset();
		LF_Hideout.Reset();
		LF_Contraband.Reset();
	}
}
