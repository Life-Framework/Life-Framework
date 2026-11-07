// red-proof: PENDING - not yet observed. The Arma Reforger Server install was being restored
// when the silent skip was removed, so `tools\cli test --no-build --tier all` could not boot.
// Planned perturbation: expect GetCash 51 after TakeCash(150). Before this change an unresolved
// storage manager returned ctx.Pass and skipped every cash assertion while the test read green;
// the fixture now records a FAIL with its reason instead.
// tier: WORLD
class EL_Test_MoneyCash : EL_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "money/cash-roundtrip";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(EL_TestContext ctx)
	{
		// An unresolved character, storage manager, or jacket is a FAIL with its reason
		// (EL_TestCharacterFixture), never a pass that skips the cash assertions.
		IEntity character = EL_TestCharacterFixture.Spawn(ctx, true);
		if (!character)
			return;

		ctx.Equal(0, EL_MoneyUtils.GetCash(character), "a fresh character carries no cash");

		ctx.Equal(200, EL_MoneyUtils.AddCash(character, 200), "AddCash(200) reports 200 added");
		ctx.Equal(200, EL_MoneyUtils.GetCash(character), "GetCash reads 200 after AddCash");

		ctx.Equal(150, EL_MoneyUtils.TakeCash(character, 150), "TakeCash(150) reports 150 taken");
		ctx.Equal(50, EL_MoneyUtils.GetCash(character), "GetCash reads 50 after TakeCash(150)");

		ctx.Equal(50, EL_MoneyUtils.TakeCash(character, 999), "overdraw TakeCash(999) takes only the remaining 50");
		ctx.Equal(0, EL_MoneyUtils.GetCash(character), "GetCash reads 0 after overdraw");

		ctx.Equal(0, EL_MoneyUtils.AddCash(character, -5), "negative AddCash is rejected");
		ctx.Equal(0, EL_MoneyUtils.AddCash(character, 0), "zero AddCash is rejected");
		ctx.Equal(0, EL_MoneyUtils.GetCash(character), "GetCash unaffected by rejected AddCash");

		EL_TestCharacterFixture.Delete(character);
	}
}
