// red-proof: 2026-11-22 `node tools/cli.mjs test --no-build --red LF_Test_MiningRockHint`: RED-OK mining/rock-hint. RedProof() calls the boulder ore, so the first check fails.
//! mining/rock-hint (first-2h review B8): a pickaxe swung at plain rock tells the miner, at most once a
//! while; an ore rock, another tool or a non-rock says nothing. An ore rock's own size multiplies the mined
//! shrink, and a loose ore piece glints only while it lies loose on a peer that renders.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_MiningRockHint : LF_Test
{
	protected const string PICKAXE = "{23341E2840F5ACB4}Prefabs/Tools/Pickaxe.et";
	protected const string BOULDER = "{735E970ACBB1B473}Prefabs/Rocks/Granite/Granite_Boulder_01.et";
	protected const string ORE = "{2C571D6F7C05E095}Prefabs/Resources/Mining/IronOre.et";
	protected const string AXE = "{6DE3E8F6CD1C3A5B}Prefabs/Tools/Axe.et";

	override string GetName()
	{
		return "mining/rock-hint";
	}

	override void Run(LF_TestContext ctx)
	{
		bool boulderIsOre = false;
		if (RedProof())
			boulderIsOre = true;

		ctx.True(LF_MiningRockHint.IsPlainRockHit(PICKAXE, BOULDER, boulderIsOre), "a pickaxe on a granite boulder is a plain-rock hit");
		ctx.False(LF_MiningRockHint.IsPlainRockHit(PICKAXE, ORE, true), "a pickaxe on an ore rock is mining, not a hint");
		ctx.False(LF_MiningRockHint.IsPlainRockHit(AXE, BOULDER, false), "another tool on a boulder says nothing");
		ctx.False(LF_MiningRockHint.IsPlainRockHit(PICKAXE, "{1111111111111111}Prefabs/Props/Crate.et", false), "a pickaxe on a crate says nothing");
		ctx.False(LF_MiningRockHint.IsPlainRockHit(PICKAXE, "", false), "a pickaxe on the terrain says nothing");

		ctx.True(LF_MiningRockHint.HintDue(-1, 5), "the first plain-rock hit hints");
		ctx.False(LF_MiningRockHint.HintDue(100, 110), "a swing 10 s later does not hint again");
		ctx.True(LF_MiningRockHint.HintDue(100, 100 + LF_MiningRockHint.HINT_GAP_SECONDS), "a swing after the gap hints again");

		ctx.EqualFloat(1.6, LF_OreNodeComponent.ScaleOf(1.6, 1), 0.001, "a whole big rock shows its own size");
		ctx.EqualFloat(0.72, LF_OreNodeComponent.ScaleOf(1.6, 0.45), 0.001, "a mined big rock shrinks from its own size");
		ctx.EqualFloat(0.45, LF_OreNodeComponent.ScaleOf(0, 0.45), 0.001, "an unset size reads as the mesh size");

		ctx.True(LF_LooseGlintComponent.ShouldGlint(false, true), "a loose piece glints on a peer that renders");
		ctx.False(LF_LooseGlintComponent.ShouldGlint(true, true), "a carried piece does not glint");
		ctx.False(LF_LooseGlintComponent.ShouldGlint(false, false), "a dedicated server shows no glint");
	}
}
