// red-proof: observed 2026-11-23 with `--only LF_Test_GoodsClass` before LF_GoodsClass existed: the run failed to compile; the `--red` run on the finished test is recorded in the PR.

//! Amount class on goods (#4201, CR-F1): units map to A (1 to 2), B (3 to 10) and C (11 and up) at the boundaries.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_GoodsClass : LF_Test
{
	override string GetName()
	{
		return "chains/goods-class";
	}

	//------------------------------------------------------------------------------------------------
	protected string ClassOf(int units)
	{
		LF_EGoodsClass c = LF_EGoodsClass.NONE;
		if (!RedProof())
			c = LF_GoodsClass.FromUnits(units);
		return LF_GoodsClass.Letter(c);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		ctx.EqualStr("", ClassOf(0), "no units carries no class");
		ctx.EqualStr("", ClassOf(-3), "a negative count carries no class");
		ctx.EqualStr("A", ClassOf(1), "1 unit is class A");
		ctx.EqualStr("A", ClassOf(2), "2 units is still class A");
		ctx.EqualStr("B", ClassOf(3), "3 units is class B");
		ctx.EqualStr("B", ClassOf(10), "10 units is still class B");
		ctx.EqualStr("C", ClassOf(11), "11 units is class C");
		ctx.EqualStr("C", ClassOf(500), "a large load is class C");

		// A count above 2^24 must not round (the float Math trap); the class is a plain integer compare.
		ctx.EqualStr("C", ClassOf(1800000480), "a very large int stays class C");

		if (!RedProof())
		{
			ctx.True(LF_GoodsClass.FromUnits(2) == LF_EGoodsClass.A, "the enum is A at 2");
			ctx.True(LF_GoodsClass.FromUnits(3) == LF_EGoodsClass.B, "the enum is B at 3");
			ctx.True(LF_GoodsClass.FromUnits(11) == LF_EGoodsClass.C, "the enum is C at 11");
			ctx.True(LF_GoodsClass.FromUnits(0) == LF_EGoodsClass.NONE, "the enum is NONE at 0");
		}
		else
		{
			ctx.True(false, "red proof: the class lookup is skipped");
		}
	}
}
