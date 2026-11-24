// red-proof: RedProof() reads the header flag and the alias rows as if the header set neither, so the top-up stays on and the key stays unaliased; the run names every row that depends on them.
//! host/mission-start-flags (#4678, #4676): a mission header closes only the money top-up
//! (m_bLF_MoneyTopUpOff, the playtest town opens a character with the island's money) and aliases one
//! string key to another (m_aLF_TextAliases, the playtest town's first hint). Both are pure resolvers;
//! a header with neither, a null header and a malformed row keep the shared behavior.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_MissionStartFlags : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "host/mission-start-flags";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_MissionHeader town = new LF_MissionHeader();
		town.m_bLF_MoneyTopUpOff = true;
		town.m_aLF_TextAliases = {"Alias-A_Desc=Alias-B_Desc", "Alias-Bad", "=Alias-Nothing", "Alias-Empty=", "Alias-C_Body=Alias-D_Body"};
		LF_MissionHeader plain = new LF_MissionHeader();

		bool townTopUp = true;
		array<string> rows = town.m_aLF_TextAliases;
		if (!RedProof())
			townTopUp = LF_MissionHeader.ResolveMoneyTopUp(true, town);
		else
			rows = null;

		ctx.False(townTopUp, "a header with m_bLF_MoneyTopUpOff closes the money top-up the game mode switches on");
		ctx.False(LF_MissionHeader.ResolveMoneyTopUp(false, town), "a closed top-up stays closed");
		ctx.True(LF_MissionHeader.ResolveMoneyTopUp(true, plain), "a header without the flag keeps the game mode's top-up");
		ctx.False(LF_MissionHeader.ResolveMoneyTopUp(false, plain), "a header without the flag does not switch the top-up on");
		ctx.True(LF_MissionHeader.ResolveMoneyTopUp(true, null), "no mission header (bare DebugWorld, tests) keeps the top-up");

		ctx.EqualStr("#Alias-B_Desc", LF_TextAliases.ResolveIn(rows, "#Alias-A_Desc"), "an aliased key with its # shows the target key");
		ctx.EqualStr("Alias-B_Desc", LF_TextAliases.ResolveIn(rows, "Alias-A_Desc"), "an aliased key without # keeps that spelling");
		ctx.EqualStr("#Alias-D_Body", LF_TextAliases.ResolveIn(rows, "#Alias-C_Body"), "a second row aliases its own key");
		ctx.EqualStr("#Alias-Other", LF_TextAliases.ResolveIn(rows, "#Alias-Other"), "a key with no row reads as before");
		ctx.EqualStr("#Alias-Bad", LF_TextAliases.ResolveIn(rows, "#Alias-Bad"), "a row with no = is ignored");
		ctx.EqualStr("#Alias-Empty", LF_TextAliases.ResolveIn(rows, "#Alias-Empty"), "a row with no target is ignored");
		ctx.EqualStr("#Alias-Nothing", LF_TextAliases.ResolveIn(rows, "#Alias-Nothing"), "a row with no source is ignored");
		ctx.EqualStr("#Alias-A_Desc", LF_TextAliases.ResolveIn(null, "#Alias-A_Desc"), "no rows (a header with no aliases) change nothing");
		ctx.EqualStr("", LF_TextAliases.ResolveIn(rows, ""), "an empty key stays empty");
	}
}
