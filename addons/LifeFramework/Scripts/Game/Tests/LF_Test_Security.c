// red-proof: perturb LF_PlayerJobComponent.LF_GetFruitCatcherRewardCount to
// return the raw score (drop the > MAX clamp), then run the fast tier; the
// forged-score assertion goes red.

// tier: LOGIC
class LF_Test_FruitCatcherRewardClamp : LF_Test
{
	override string GetName()
	{
		return "security/fruitcatcher-reward-clamp";
	}

	override void Run(LF_TestContext ctx)
	{
		ctx.Equal(0, LF_PlayerJobComponent.LF_GetFruitCatcherRewardCount(-1), "negative forged score yields nothing");
		ctx.Equal(0, LF_PlayerJobComponent.LF_GetFruitCatcherRewardCount(0), "zero score yields nothing");
		ctx.Equal(10, LF_PlayerJobComponent.LF_GetFruitCatcherRewardCount(10), "in-range score passes through");
		ctx.Equal(LF_PlayerJobComponent.LF_FRUIT_CATCHER_MAX_SCORE, LF_PlayerJobComponent.LF_GetFruitCatcherRewardCount(LF_PlayerJobComponent.LF_FRUIT_CATCHER_MAX_SCORE), "max score is a valid claim");
		ctx.Equal(LF_PlayerJobComponent.LF_FRUIT_CATCHER_MAX_SCORE, LF_PlayerJobComponent.LF_GetFruitCatcherRewardCount(999999), "forged huge score clamps to the max");
	}
};