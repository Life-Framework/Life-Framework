// red-proof: `tools\cli test --no-build --red LF_Test_QualificationGate` (RedProof() skips the grant, so the qualified officer is refused).
//! The qualification gate (#3427): an officer without the detective unlock is refused the full
//! vehicle search and opening a case on the case board; one who holds the detective course is
//! allowed; the quick vehicle search needs nothing, qualified or not.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_QualificationGate : LF_Test
{
	static const string UNQUALIFIED = "test-gate-unqualified";
	static const string QUALIFIED = "test-gate-qualified";

	override string GetName()
	{
		return "qualifications/gate";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		ctx.NotNull(LF_QualificationConfig.Get(), "the course table resolves");
		if (!LF_QualificationConfig.Get())
			return;

		// The live set before the test, put back at the end.
		array<ref LF_QualificationRow> before = LF_Qualifications.ExportAll();

		// RedProof(): the course is never granted, so the qualified officer is refused (`--red`).
		if (!RedProof())
			ctx.True(LF_Qualifications.Grant(QUALIFIED, "detective_course"), "the detective course is granted");

		ctx.False(LF_QualificationGate.Allows(UNQUALIFIED, LF_QualificationGate.ACT_FULL_SEARCH), "an unqualified officer is refused the full search");
		ctx.False(LF_QualificationGate.Allows(UNQUALIFIED, LF_QualificationGate.ACT_OPEN_CASE), "an unqualified officer is refused opening a case");
		ctx.True(LF_QualificationGate.Allows(QUALIFIED, LF_QualificationGate.ACT_FULL_SEARCH), "a detective is allowed the full search");
		ctx.True(LF_QualificationGate.Allows(QUALIFIED, LF_QualificationGate.ACT_OPEN_CASE), "a detective is allowed to open a case");

		// The quick search is open to every officer, qualified or not.
		ctx.True(LF_QualificationGate.Allows(UNQUALIFIED, LF_QualificationGate.ACT_QUICK_SEARCH), "the quick search stays open to an unqualified officer");
		ctx.True(LF_QualificationGate.Allows(QUALIFIED, LF_QualificationGate.ACT_QUICK_SEARCH), "the quick search stays open to a detective");
		ctx.True(LF_QualificationGate.UnlockFor(LF_QualificationGate.ACT_QUICK_SEARCH).IsEmpty(), "the quick search needs no unlock");
		ctx.True(LF_QualificationGate.UnlockFor(LF_QualificationGate.ACT_FULL_SEARCH) == "detective", "the full search needs detective");

		// A course that does not grant detective does not open the gate; no key holds nothing.
		LF_Qualifications.Grant(UNQUALIFIED, "traffic_course");
		ctx.False(LF_QualificationGate.Allows(UNQUALIFIED, LF_QualificationGate.ACT_FULL_SEARCH), "a traffic course does not open the full search");
		ctx.False(LF_QualificationGate.Allows("", LF_QualificationGate.ACT_OPEN_CASE), "a body with no character key is refused");
		ctx.False(LF_QualificationGate.AllowsUser(null, LF_QualificationGate.ACT_FULL_SEARCH), "no user is refused a gated action");
		ctx.True(LF_QualificationGate.AllowsUser(null, LF_QualificationGate.ACT_QUICK_SEARCH), "an ungated action needs no user lookup");

		LF_Qualifications.ApplyAll(before);
	}
}
