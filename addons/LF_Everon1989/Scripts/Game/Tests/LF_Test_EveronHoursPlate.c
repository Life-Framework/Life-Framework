// red-proof: RedProof() looks the sign up under an id that does not exist, so the row, its title and the card text are all missing and every check below fails.
//! everon/hours-plate (#4834): under the Everon 1989 header (the pack leg) the `hours_plate` sign the shop door plates
//! name resolves: a row in the pack's sign overlay with a title in US English ("Opening Hours") and a What section, and
//! the live row source the plate prints its hours from is registered. Where the plates stand is the audit's and the
//! look's business, never a test's (docs/architecture/world-zones.md).
[LF_TestInfo(LF_TestTier.LOGIC, pack: true)]
class LF_Test_EveronHoursPlate : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/hours-plate";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		string id = "hours_plate";
		if (RedProof())
			id = "no_such_hours_plate";

		LF_SignDef sign = LF_SignCatalog.Find(id);
		ctx.NotNull(sign, "the hours_plate sign row exists in the pack's sign overlay");
		if (!sign)
			return;

		ctx.True(LF_SignCatalog.IsUsable(sign), "the sign has a title");
		ctx.EqualStr("Opening Hours", LF_SignCatalog.ResolveTitle(sign.m_sTitle), "the title is US English");
		ctx.True(!LF_SignCatalog.Resolve(sign.m_sWhat).IsEmpty(), "the Read card has a What section");
		ctx.EqualStr("Read: Opening Hours", LF_SignCatalog.ReadName(sign), "the prompt reads Read: Opening Hours");
		ctx.NotNull(LF_SignRowSources.Find(LF_HoursPlate.SOURCE_KIND), "the live hours row source is registered");
	}
}
