// red-proof: RedProof() skips the catalog lookup of the folder file's sign, so the sign reads as missing and every check below fails.
//! everon/sign-folder (#3555): under the Everon 1989 header (the pack leg), a sign that lives in its own file
//! under Configs/Signs/LF_Signs_Everon1989/ loads beside the signs of the listed overlay, with no line added
//! to the mission header or to the shared signs file.
[LF_TestInfo(LF_TestTier.LOGIC, pack: true)]
class LF_Test_EveronSignFolder : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/sign-folder";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_SignDef folderSign;
		if (!RedProof())
			folderSign = LF_SignCatalog.Find("pharmacy");

		ctx.NotNull(folderSign, "the pharmacy sign in its own folder file is in the catalog");
		if (folderSign)
		{
			ctx.EqualStr("pharmacy", folderSign.m_sId, "the folder file's sign keeps its id");
			ctx.True(LF_SignCatalog.ResolveTitle(folderSign.m_sTitle) != "", "the folder file's sign has a title to print");
			ctx.True(LF_SignCatalog.Resolve(folderSign.m_sWhat) != folderSign.m_sWhat, "its text resolves to the pack's string");
		}

		ctx.NotNull(LF_SignCatalog.Find("butcher"), "a sign in the listed overlay itself still loads");
	}
}
