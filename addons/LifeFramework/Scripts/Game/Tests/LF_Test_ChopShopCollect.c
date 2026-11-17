// red-proof: 2026-11-17 `tools\cli test --no-build --red LF_Test_ChopShopCollect,LF_Test_RobNoRoom,LF_Test_JailEscortDrop,LF_Test_JailSelfArrest,LF_Test_JailWorkAbandon`: RED PROOF OK, all 5 failed. The hook skips the collect rule: `- a full inventory that took no cash keeps the car (expected 12, got 0)`. Green on the fixed code.
// tier: LOGIC
//! Chop shop collect guard (crime review 2026-11-17): COLLECT used to delete the car even when
//! the worker's full inventory took none of the payout, and even with someone sitting inside.
//! The collect rule now keeps the car in both cases.
class LF_Test_ChopShopCollect : LF_Test
{
	override string GetName()
	{
		return "chopshop/collect-guard";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_EChopCheck full = LF_EChopCheck.NONE;
		LF_EChopCheck partial = LF_EChopCheck.NONE;
		LF_EChopCheck occupied = LF_EChopCheck.NONE;
		LF_EChopCheck paid = LF_EChopCheck.NONE;
		if (!RedProof())
		{
			full = LF_ChopShopRules.CheckCollect(false, 0, 900);
			partial = LF_ChopShopRules.CheckCollect(false, 400, 900);
			occupied = LF_ChopShopRules.CheckCollect(true, 900, 900);
			paid = LF_ChopShopRules.CheckCollect(false, 900, 900);
		}

		ctx.Equal(LF_EChopCheck.NO_ROOM, full, "a full inventory that took no cash keeps the car");
		ctx.Equal(LF_EChopCheck.NO_ROOM, partial, "a short payment keeps the car");
		ctx.Equal(LF_EChopCheck.OCCUPIED, occupied, "a car with someone inside is not collected");
		ctx.Equal(LF_EChopCheck.NONE, paid, "a full payment on an empty car collects");
		ctx.EqualStr("#LF-Chop_NoRoom", LF_ChopShopRules.RefusalKey(LF_EChopCheck.NO_ROOM), "the no-room refusal has a toast");
	}
}
