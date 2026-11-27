// red-proof: with `RedProof()` on the jail doors are never set and the rules are not asked, so every fuse stays -1 and every
// offence stays "?", which fails the outer fuse, cell fuse, fallback and offence checks.

//! police/jail-break-fuse: what a charge on a jail door burns and reports (LF_JailBreak.FuseFor and OffenceFor). A charge on
//! the outer door burns the config's outer fuse, one on a cell door the cell fuse, any other target (a bank vault, a
//! safe) keeps the fuse the caller passed, and the empty id is never a jail door. A charge on a cell reports the jailbreak
//! offence, every other target reports the offence the caller named (breaking the outer door is the same offence a
//! vault charge would be).
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_JailBreakFuse : LF_Test
{
	protected static const string CELL = "test-fuse-cell-door";
	protected static const string OUTER = "test-fuse-outer-door";

	override string GetName()
	{
		return "police/jail-break-fuse";
	}

	override void Run(LF_TestContext ctx)
	{
		float outerFuse = -1;
		float cellFuse = -1;
		float otherFuse = -1;
		float emptyFuse = -1;
		float configOuter = -2;
		float configCell = -2;
		string cellOffence = "?";
		string outerOffence = "?";
		string otherOffence = "?";
		string emptyOffence = "?";
		if (!RedProof())
		{
			LF_Jail.SetOuterDoorTarget(OUTER);
			LF_Jail.SetDoorTarget(CELL);
			configOuter = LF_JailBreakConfig.Get().OuterFuseSeconds();
			configCell = LF_JailBreakConfig.Get().CellFuseSeconds();
			outerFuse = LF_JailBreak.FuseFor(OUTER, 99);
			cellFuse = LF_JailBreak.FuseFor(CELL, 99);
			otherFuse = LF_JailBreak.FuseFor("some-vault-door", 7.5);
			emptyFuse = LF_JailBreak.FuseFor("", 7.5);
			cellOffence = LF_JailBreak.OffenceFor(CELL, "VAULT");
			outerOffence = LF_JailBreak.OffenceFor(OUTER, "VAULT");
			otherOffence = LF_JailBreak.OffenceFor("some-vault-door", "VAULT");
			emptyOffence = LF_JailBreak.OffenceFor("", "VAULT");
			LF_Jail.SyncPlacedCell();
		}

		ctx.True(configOuter > 0, "the shipped config gives the outer door a fuse");
		ctx.True(configCell > 0, "the shipped config gives the cell door a fuse");
		ctx.EqualFloat(configOuter, outerFuse, 0.001, "a charge on the outer door burns the outer fuse, not the caller's");
		ctx.EqualFloat(configCell, cellFuse, 0.001, "a charge on a cell door burns the cell fuse, not the caller's");
		ctx.EqualFloat(7.5, otherFuse, 0.001, "a charge on any other target keeps the caller's fuse");
		ctx.EqualFloat(7.5, emptyFuse, 0.001, "the empty id is not a jail door");
		ctx.EqualStr(LF_JailBreak.OFFENCE_PRISON_BREACH, cellOffence, "a charge on a cell reports the jailbreak offence");
		ctx.EqualStr("VAULT", outerOffence, "a charge on the outer door reports the caller's offence");
		ctx.EqualStr("VAULT", otherOffence, "a charge on any other target reports the caller's offence");
		ctx.EqualStr("VAULT", emptyOffence, "the empty id reports the caller's offence");
	}
}
