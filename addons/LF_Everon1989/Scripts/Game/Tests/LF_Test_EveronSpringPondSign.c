// red-proof: RedProof() skips the sign lookup, so the plate reads as missing and the checks below fail.
//! everon/spring-pond-sign (#3667): under the Everon 1989 header (the pack leg), the Spring Pond plate is in the sign catalog (its own folder
//! file) with a title and text that resolve to the pack's strings.
[LF_TestInfo(LF_TestTier.LOGIC, pack: true)]
class LF_Test_EveronSpringPondSign : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/spring-pond-sign";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_SignDef sign;
		if (!RedProof())
			sign = LF_SignCatalog.Find("spring-pond");

		ctx.NotNull(sign, "the spring-pond plate is in the sign catalog");
		if (!sign)
			return;

		ctx.True(LF_SignCatalog.ResolveTitle(sign.m_sTitle) != "", "the plate has a title to print");
		ctx.True(LF_SignCatalog.Resolve(sign.m_sWhat) != sign.m_sWhat, "its text resolves to the pack's string");
		ctx.True(LF_SignCatalog.Resolve(sign.m_sHow) != sign.m_sHow, "its how line resolves to the pack's string");
	}
}
