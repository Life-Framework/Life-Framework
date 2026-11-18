// red-proof: 2026-11-18 `tools\cli test --no-build --red LF_Test_MarkedCash`: RED PROOF OK; the hook adds 0 instead of 300: `Take returns the 120 taken (expected 120, got 0)`, `180 are left (expected 180, got 0)`. Green: 13 assertions.
//! Marked notes (#2850): a stack of the `markedcash` slot prefab that sits in the inventory like
//! cash but is never counted as cash. Add, Count and Take work on it alone; a slot with no usable
//! prefab makes Add log a Laundering error and give nothing.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_MarkedCash : LF_Test
{
	protected static const string REASON = "test-robbery";

	override string GetName()
	{
		return "laundering/marked-cash";
	}

	override void Run(LF_TestContext ctx)
	{
		IEntity body = LF_ExploitGuardsWorldFixture.SpawnCharacter(ctx, "104 0 94", true);
		if (!body)
			return;

		LF_MarkedCash.ResetStatics();
		ctx.False(LF_MarkedCash.Prefab().IsEmpty(), "the markedcash slot resolves a prefab");
		ctx.False(LF_Utils.SamePrefab(LF_MarkedCash.Prefab(), LF_MoneyUtils.CashPrefab()), "marked notes are not the cash prefab");

		int cashBefore = LF_MoneyUtils.GetCash(body);
		int add = 300;
		if (RedProof())
			add = 0;

		ctx.Equal(300, LF_MarkedCash.Add(body, add, REASON), "Add gives a 300 stack");
		ctx.Equal(300, LF_MarkedCash.Count(body), "Count reads 300");
		ctx.Equal(cashBefore, LF_MoneyUtils.GetCash(body), "GetCash ignores the marked notes");

		ctx.Equal(120, LF_MarkedCash.Take(body, 120, REASON), "Take returns the 120 taken");
		ctx.Equal(180, LF_MarkedCash.Count(body), "180 are left");
		ctx.Equal(180, LF_MarkedCash.Take(body, 500, REASON), "a take larger than held takes what is there");
		ctx.Equal(0, LF_MarkedCash.Count(body), "nothing is left");
		ctx.Equal(cashBefore, LF_MoneyUtils.GetCash(body), "cash never moved");

		LF_MarkedCash.SetSlotForTests("markedcash-missing-for-test");
		ctx.Equal(0, LF_MarkedCash.Add(body, 300, REASON), "with no resolvable slot prefab Add gives nothing");
		ctx.True(LF_MarkedCash.GetLastError().Contains("no usable prefab"), "the refusal names the missing prefab");
		ctx.Equal(cashBefore, LF_MoneyUtils.GetCash(body), "the refused add left cash alone");

		LF_MarkedCash.ResetStatics();
		SCR_EntityHelper.DeleteEntityAndChildren(body);
	}
}
