// red-proof: perturb EL_PlayerJobComponent.EL_GetFruitCatcherRewardCount to
// return the raw score (drop the > MAX clamp), then run the fast tier; the
// forged-score assertion goes red.

// tier: LOGIC
class EL_Test_FruitCatcherRewardClamp : EL_Test
{
	override string GetName()
	{
		return "security/fruitcatcher-reward-clamp";
	}

	override void Run(EL_TestContext ctx)
	{
		ctx.Equal(0, EL_PlayerJobComponent.EL_GetFruitCatcherRewardCount(-1), "negative forged score yields nothing");
		ctx.Equal(0, EL_PlayerJobComponent.EL_GetFruitCatcherRewardCount(0), "zero score yields nothing");
		ctx.Equal(10, EL_PlayerJobComponent.EL_GetFruitCatcherRewardCount(10), "in-range score passes through");
		ctx.Equal(EL_PlayerJobComponent.EL_FRUIT_CATCHER_MAX_SCORE, EL_PlayerJobComponent.EL_GetFruitCatcherRewardCount(EL_PlayerJobComponent.EL_FRUIT_CATCHER_MAX_SCORE), "max score is a valid claim");
		ctx.Equal(EL_PlayerJobComponent.EL_FRUIT_CATCHER_MAX_SCORE, EL_PlayerJobComponent.EL_GetFruitCatcherRewardCount(999999), "forged huge score clamps to the max");
	}
};