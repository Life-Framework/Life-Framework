// red-proof: PENDING TODO(scaffold): break one assertion below, run `tools\cli test --no-build --only LF_Test_Chain`, paste the observed [LFTEST] FAIL line here, restore it.
// tier: LOGIC
//! Chain (#794): the rule, with no world.
class LF_Test_Chain : LF_Test
{
	override string GetName()
	{
		return "onboarding/chain";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_Chain.ResetStatics();

		ctx.False(LF_Chain.Use(false), "no character is refused");
		ctx.True(LF_Chain.GetUses() == 0, "a refused use changes nothing");
		ctx.True(LF_Chain.Use(true), "a character is allowed");
		ctx.True(LF_Chain.GetUses() == 1, "an allowed use is counted once");

		LF_Chain.ResetStatics();
		ctx.True(LF_Chain.GetUses() == 0, "the static reset clears the count");
	}
}
