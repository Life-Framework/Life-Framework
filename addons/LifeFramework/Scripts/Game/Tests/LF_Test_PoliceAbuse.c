// red-proof: observed 2026-11-08 (tools/cli test --no-build --tier all) for the armory part, which
// moved to LF_Test_ArmoryKitCap and LF_Test_ArmoryCooldownPersist (#2416). The reach checks are pure.

// tier: LOGIC
//! Police menu reach. The armory's one-kit rule is LF_Test_ArmoryKitCap. Fines are paper tickets (LF_Test_FineTicket*).
class LF_Test_PoliceAbuse : LF_Test
{
	override string GetName()
	{
		return "security/police-abuse";
	}

	override void Run(LF_TestContext ctx)
	{
		vector officer = "100 0 100";
		vector close = "104.9 0 100";
		vector justOut = "105.1 0 100";
		vector acrossMap = "2100 0 100";
		ctx.True(LF_PoliceUtils.IsWithinReach(officer, close), "a suspect 4.9 m away is within reach");
		ctx.False(LF_PoliceUtils.IsWithinReach(officer, justOut), "a suspect 5.1 m away is out of reach");
		ctx.False(LF_PoliceUtils.IsWithinReach(officer, acrossMap), "a suspect across the map is out of reach");
		ctx.False(LF_PoliceUtils.IsTargetInReach(null, null), "a suspect with no body is out of reach");
	}
}
