// red-proof: Observed 2026-11-17 `tools/cli test --no-build --only LF_Test_ChopShopDropPick` with the stolen-first pick disabled failed `crime/chop-drop-pick` with `- a stolen car further out beats an unstolen one at the bench (expected 1, got 0)`. Restored: green.
// tier: LOGIC
//! #2419: the bench takes the nearest STOLEN car in its drop area, so an unstolen car parked beside
//! it cannot block the yard. With no stolen car it still names the nearest one, so the worker reads
//! why the bench refuses.
class LF_Test_ChopShopDropPick : LF_Test
{
	override string GetName()
	{
		return "crime/chop-drop-pick";
	}

	override void Run(LF_TestContext ctx)
	{
		array<float> distances = {3, 5};
		array<bool> stolen = {false, true};
		ctx.Equal(1, LF_ChopShopRules.PickDropIndex(distances, stolen), "a stolen car further out beats an unstolen one at the bench");

		stolen = {false, false};
		ctx.Equal(0, LF_ChopShopRules.PickDropIndex(distances, stolen), "with no stolen car the nearest car is named");

		distances = {5, 2, 4};
		stolen = {true, false, true};
		ctx.Equal(2, LF_ChopShopRules.PickDropIndex(distances, stolen), "the nearest of several stolen cars wins");

		distances = {};
		stolen = {};
		ctx.Equal(-1, LF_ChopShopRules.PickDropIndex(distances, stolen), "an empty drop area has no car");
	}
}
