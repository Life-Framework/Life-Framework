// red-proof: Observed 2026-11-08 with `tools\cli test --no-build --tier all`: expecting GetCash 51
// after TakeCash(150) failed it with `[LFTEST] FAIL money/cash-roundtrip`,
// `- GetCash reads 50 after TakeCash(150) (expected 51, got 50)`. Restored; green. Before this
// change an unresolved storage manager returned ctx.Pass and skipped every cash assertion; the
// fixture now records a FAIL with its reason instead.
// tier: WORLD
class LF_Test_MoneyCash : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "money/cash-roundtrip";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// An unresolved character, storage manager, or jacket is a FAIL with its reason
		// (LF_TestCharacterFixture), never a pass that skips the cash assertions.
		IEntity character = LF_TestCharacterFixture.Spawn(ctx);
		if (!character)
			return;

		ctx.Equal(0, LF_MoneyUtils.GetCash(character), "a fresh character carries no cash");

		ctx.Equal(200, LF_MoneyUtils.AddCash(character, 200), "AddCash(200) reports 200 added");
		ctx.Equal(200, LF_MoneyUtils.GetCash(character), "GetCash reads 200 after AddCash");

		ctx.Equal(150, LF_MoneyUtils.TakeCash(character, 150), "TakeCash(150) reports 150 taken");
		ctx.Equal(50, LF_MoneyUtils.GetCash(character), "GetCash reads 50 after TakeCash(150)");

		ctx.Equal(50, LF_MoneyUtils.TakeCash(character, 999), "overdraw TakeCash(999) takes only the remaining 50");
		ctx.Equal(0, LF_MoneyUtils.GetCash(character), "GetCash reads 0 after overdraw");

		ctx.Equal(0, LF_MoneyUtils.AddCash(character, -5), "negative AddCash is rejected");
		ctx.Equal(0, LF_MoneyUtils.AddCash(character, 0), "zero AddCash is rejected");
		ctx.Equal(0, LF_MoneyUtils.GetCash(character), "GetCash unaffected by rejected AddCash");

		LF_TestCharacterFixture.Delete(character);
	}
}
