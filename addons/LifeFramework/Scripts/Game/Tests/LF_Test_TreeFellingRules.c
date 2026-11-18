// red-proof: 2026-11-18, `tools\cli test --no-build --red LF_Test_TreeFellingRules` printed "RED-OK lumber/felling-rules" (the hook skips loading the shipped species config; "the shipped species config loads" failed). Then `--only` passed.
//! lumber/felling-rules (#3259): which base-game map tree an axe can fell, what it yields, the
//! refusals in order, the stable tree key and the regrow ledger. Reads the shipped
//! Configs/Lumber/LF_TreeSpecies.conf (a resource load, hence WORLD), no entities.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_TreeFellingRules : LF_Test
{
	protected static const ResourceName PINE_2F = "{AAAAAAAAAAAAAAAA}Prefabs/Vegetation/Tree/t_pinus_sylvestris/t_pinus_sylvestris_2f.et";
	protected static const ResourceName PINE_V2_3D = "Prefabs/Vegetation/Tree/t_pinus_sylvestris/T_Pinus_sylvestris_02_3d.et";
	protected static const ResourceName PINE_V2_HERO = "Prefabs/Vegetation/Tree/t_pinus_sylvestris/T_Pinus_sylvestris_02_Hero_3s.et";
	protected static const ResourceName HORNBEAM_3S = "Prefabs/Vegetation/Tree/t_carpinus_betulus/t_carpinus_betulus_3s_aut.et";
	protected static const ResourceName SPRUCE_1F = "Prefabs/Vegetation/Tree/t_picea_abies/t_picea_abies_1f.et";
	protected static const ResourceName PINE_STUMP = "Prefabs/Vegetation/Tree/t_pinus_sylvestris/t_pinus_sylvestris_stump_01.et";
	protected static const ResourceName PINE_FALLEN = "Prefabs/Vegetation/Tree/t_pinus_sylvestris/T_Pinus_sylvestris_02_2d_fallen.et";
	protected static const ResourceName PINE_BASE = "Prefabs/Vegetation/Tree/t_pinus_sylvestris/t_pinus_sylvestris_base.et";
	protected static const ResourceName APPLE = "Prefabs/Vegetation/Tree/t_malus_domestica/t_malus_domestica_2f.et";
	protected static const ResourceName SAPLING = "Prefabs/Vegetation/Tree/t_pinus_sylvestris/t_pinus_sylvestris_0s.et";
	protected static const ResourceName NOT_TREE = "Prefabs/Props/Crates/Crate_01.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "lumber/felling-rules";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_TreeFellingConfig config;
		if (!RedProof())
			config = LF_TreeFellingConfig.Load(LF_TreeFellingConfig.CONFIG_PATH);

		ctx.NotNull(config, "the shipped species config loads");
		if (ctx.FailureCount() > 0)
			return;

		CheckConfig(ctx, config);
		CheckKinds(ctx, config);
		CheckYields(ctx, config);
		CheckRefusals(ctx, config);
		CheckKeys(ctx);
		CheckLedger(ctx);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckConfig(LF_TestContext ctx, LF_TreeFellingConfig config)
	{
		ctx.Equal(8, config.m_aSpecies.Count(), "eight species rows (three softwood, five hardwood)");
		ctx.Equal(6, config.m_aYields.Count(), "six yield rows (two classes, sizes 1 to 3)");
		ctx.Equal(2700, config.RegrowSecondsOf(LF_TreeWoodClass.SOFTWOOD), "softwood stands again after 45 min");
		ctx.Equal(5400, config.RegrowSecondsOf(LF_TreeWoodClass.HARDWOOD), "hardwood stands again after 90 min");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckKinds(LF_TestContext ctx, LF_TreeFellingConfig config)
	{
		LF_TreeKind pine = LF_TreeFelling.KindOf(PINE_2F, config);
		ctx.Equal(LF_TreeWoodClass.SOFTWOOD, pine.m_eClass, "a pine with its GUID is softwood");
		ctx.Equal(2, pine.m_iSize, "t_pinus_sylvestris_2f is size 2");
		ctx.False(pine.m_bExcluded, "a standing pine is not excluded");

		ctx.Equal(3, LF_TreeFelling.KindOf(PINE_V2_3D, config).m_iSize, "the 02_ model generation is not the size: _02_3d is size 3");
		ctx.Equal(3, LF_TreeFelling.KindOf(PINE_V2_HERO, config).m_iSize, "_02_Hero_3s is size 3");
		ctx.Equal(LF_TreeWoodClass.HARDWOOD, LF_TreeFelling.KindOf(HORNBEAM_3S, config).m_eClass, "hornbeam is hardwood");
		ctx.Equal(3, LF_TreeFelling.KindOf(HORNBEAM_3S, config).m_iSize, "t_carpinus_betulus_3s_aut is size 3");
		ctx.Equal(LF_TreeWoodClass.NONE, LF_TreeFelling.KindOf(APPLE, config).m_eClass, "an apple tree has no wood class (orchards stay)");
		ctx.True(LF_TreeFelling.KindOf(PINE_STUMP, config).m_bExcluded, "a stump is excluded");
		ctx.True(LF_TreeFelling.KindOf(PINE_FALLEN, config).m_bExcluded, "a fallen trunk is excluded");
		ctx.True(LF_TreeFelling.KindOf(PINE_BASE, config).m_bExcluded, "a base prefab is excluded");
		ctx.Equal(LF_TreeWoodClass.NONE, LF_TreeFelling.KindOf(NOT_TREE, config).m_eClass, "a crate is no tree");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckYields(LF_TestContext ctx, LF_TreeFellingConfig config)
	{
		LF_TreeYieldRow pine = LF_TreeFelling.YieldFor(PINE_2F, config);
		ctx.NotNull(pine, "a size 2 pine has a yield row");
		if (pine)
		{
			ctx.Equal(25, pine.m_iChopSeconds, "a size 2 pine takes 25 s");
			ctx.Equal(2, pine.m_iLogs, "a size 2 pine drops 2 logs");
			ctx.EqualStr("gather_logging", pine.m_sXpAction, "softwood pays the gather_logging award");
		}

		LF_TreeYieldRow hornbeam = LF_TreeFelling.YieldFor(HORNBEAM_3S, config);
		ctx.NotNull(hornbeam, "a size 3 hornbeam has a yield row");
		if (hornbeam)
		{
			ctx.Equal(60, hornbeam.m_iChopSeconds, "a size 3 hardwood takes 60 s");
			ctx.Equal(3, hornbeam.m_iLogs, "a size 3 hardwood drops 3 logs");
			ctx.EqualStr("fell_hardwood", hornbeam.m_sXpAction, "hardwood pays the fell_hardwood award");
			ctx.Equal(50, hornbeam.m_iStaminaPct, "a size 3 hardwood costs half the stamina");
		}

		LF_TreeYieldRow spruce = LF_TreeFelling.YieldFor(SPRUCE_1F, config);
		ctx.True(spruce && spruce.m_iLogs == 1 && spruce.m_iChopSeconds == 15, "a size 1 spruce: 1 log in 15 s");
		ctx.True(LF_TreeFelling.YieldFor(SAPLING, config) == null, "a sapling has no yield");
		ctx.True(LF_TreeFelling.YieldFor(PINE_STUMP, config) == null, "a stump has no yield");
	}

	//------------------------------------------------------------------------------------------------
	protected LF_FellFacts GoodFacts()
	{
		LF_FellFacts facts = new LF_FellFacts();
		facts.m_bHasAxe = true;
		facts.m_sPrefab = PINE_2F;
		facts.m_fDistanceM = 1.5;
		facts.m_fRoadDistanceM = 30;
		facts.m_fBuildingDistanceM = 40;
		facts.m_fStamina = 1;
		return facts;
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckRefusals(LF_TestContext ctx, LF_TreeFellingConfig config)
	{
		ctx.Equal(LF_FellRefusal.OK, LF_TreeFelling.Check(GoodFacts(), config), "a pine in open forest with the axe in hand is felled");

		LF_FellFacts facts = GoodFacts();
		facts.m_fRoadDistanceM = -1;
		facts.m_fBuildingDistanceM = -1;
		ctx.Equal(LF_FellRefusal.OK, LF_TreeFelling.Check(facts, config), "no road and no building in range is fine");

		// Each refusal on its own, then the order: a fact set breaking every rule gives the first one.
		facts = GoodFacts();
		facts.m_bHasAxe = false;
		ctx.Equal(LF_FellRefusal.NO_AXE, LF_TreeFelling.Check(facts, config), "no axe");

		facts = GoodFacts();
		facts.m_sPrefab = APPLE;
		ctx.Equal(LF_FellRefusal.NOT_A_TREE, LF_TreeFelling.Check(facts, config), "an apple tree is not a tree to fell");
		facts.m_sPrefab = PINE_STUMP;
		ctx.Equal(LF_FellRefusal.NOT_A_TREE, LF_TreeFelling.Check(facts, config), "a stump is not a tree to fell");

		facts = GoodFacts();
		facts.m_sPrefab = SAPLING;
		ctx.Equal(LF_FellRefusal.TOO_SMALL, LF_TreeFelling.Check(facts, config), "a sapling is too small");

		facts = GoodFacts();
		facts.m_bFelled = true;
		ctx.Equal(LF_FellRefusal.ALREADY_FELLED, LF_TreeFelling.Check(facts, config), "a felled tree");

		facts = GoodFacts();
		facts.m_fDistanceM = 3.5;
		ctx.Equal(LF_FellRefusal.TOO_FAR, LF_TreeFelling.Check(facts, config), "3.5 m from the trunk is too far");

		facts = GoodFacts();
		facts.m_bInSafeZone = true;
		ctx.Equal(LF_FellRefusal.SAFE_ZONE, LF_TreeFelling.Check(facts, config), "a safe zone");

		facts = GoodFacts();
		facts.m_fRoadDistanceM = 3;
		ctx.Equal(LF_FellRefusal.NEAR_ROAD, LF_TreeFelling.Check(facts, config), "a road 3 m away");

		facts = GoodFacts();
		facts.m_fBuildingDistanceM = 10;
		ctx.Equal(LF_FellRefusal.NEAR_BUILDING, LF_TreeFelling.Check(facts, config), "a building 10 m away");

		facts = GoodFacts();
		facts.m_fStamina = 0.2;
		ctx.Equal(LF_FellRefusal.TIRED, LF_TreeFelling.Check(facts, config), "20 % stamina is below the 25 % a size 2 pine costs");

		LF_FellFacts all = new LF_FellFacts();
		all.m_sPrefab = SAPLING;
		all.m_bFelled = true;
		all.m_fDistanceM = 9;
		all.m_bInSafeZone = true;
		all.m_fRoadDistanceM = 1;
		all.m_fBuildingDistanceM = 1;
		all.m_fStamina = 0;
		ctx.Equal(LF_FellRefusal.NO_AXE, LF_TreeFelling.Check(all, config), "every rule broken: no axe comes first");
		all.m_bHasAxe = true;
		ctx.Equal(LF_FellRefusal.TOO_SMALL, LF_TreeFelling.Check(all, config), "then a sapling is too small");
		all.m_sPrefab = PINE_2F;
		ctx.Equal(LF_FellRefusal.ALREADY_FELLED, LF_TreeFelling.Check(all, config), "then already felled");
		all.m_bFelled = false;
		ctx.Equal(LF_FellRefusal.TOO_FAR, LF_TreeFelling.Check(all, config), "then too far");
		all.m_fDistanceM = 1;
		ctx.Equal(LF_FellRefusal.SAFE_ZONE, LF_TreeFelling.Check(all, config), "then the safe zone");
		all.m_bInSafeZone = false;
		ctx.Equal(LF_FellRefusal.NEAR_ROAD, LF_TreeFelling.Check(all, config), "then the road");
		all.m_fRoadDistanceM = -1;
		ctx.Equal(LF_FellRefusal.NEAR_BUILDING, LF_TreeFelling.Check(all, config), "then the building");
		all.m_fBuildingDistanceM = -1;
		ctx.Equal(LF_FellRefusal.TIRED, LF_TreeFelling.Check(all, config), "then stamina");

		ctx.Equal(LF_FellRefusal.NOT_A_TREE, LF_TreeFelling.Check(GoodFacts(), null), "a missing config fells nothing");

		ctx.EqualStr("#LF-Lumber_NoAxe", LF_TreeFelling.RefusalKey(LF_FellRefusal.NO_AXE), "refusal key for NO_AXE");
		ctx.EqualStr("#LF-Lumber_NearBuilding", LF_TreeFelling.RefusalKey(LF_FellRefusal.NEAR_BUILDING), "refusal key for NEAR_BUILDING");
		ctx.EqualStr("", LF_TreeFelling.RefusalKey(LF_FellRefusal.OK), "OK has no refusal key");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckKeys(LF_TestContext ctx)
	{
		string a = LF_TreeFelling.KeyOf("4512.34 21.0 10801.26", PINE_2F);
		string b = LF_TreeFelling.KeyOf("4512.31 25.0 10801.29", PINE_2F);
		ctx.EqualStr(a, b, "the key ignores height and noise under 5 cm");
		ctx.True(a != LF_TreeFelling.KeyOf("4512.84 21.0 10801.26", PINE_2F), "a neighbour 0.5 m away has another key");
		ctx.True(a != LF_TreeFelling.KeyOf("4512.34 21.0 10801.26", SPRUCE_1F), "another prefab on the spot has another key");
		ctx.EqualStr("45123:108013:{aaaaaaaaaaaaaaaa}prefabs/vegetation/tree/t_pinus_sylvestris/t_pinus_sylvestris_2f.et", a, "the key is decimetres and the lower-case prefab");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckLedger(LF_TestContext ctx)
	{
		// Unix-scale seconds: the ledger must stay exact above 2^24 (#2787).
		int t0 = 1800000480;
		LF_TreeFellingLedger ledger = new LF_TreeFellingLedger();
		ctx.True(ledger.Fell("a", t0, 2700), "the first fell is recorded");
		ctx.False(ledger.Fell("a", t0 + 10, 2700), "a second fell of the same tree is refused");
		ctx.True(ledger.IsFelled("a", t0 + 2699), "down one second before its time");
		ctx.Equal(1, ledger.SecondsLeft("a", t0 + 2699), "one second left");
		ctx.True(ledger.Fell("b", t0 + 100, 5400), "another tree is recorded");
		ctx.Equal(2, ledger.Count(), "two trees down");

		array<string> none = ledger.TakeRegrown(t0 + 2699);
		ctx.Equal(0, none.Count(), "nothing regrows early");

		array<string> regrown = ledger.TakeRegrown(t0 + 2700);
		ctx.Equal(1, regrown.Count(), "one tree regrows on time");
		ctx.True(regrown.Count() == 1 && regrown[0] == "a", "the pine regrows first");
		ctx.False(ledger.IsFelled("a", t0 + 2700), "the pine stands again");
		ctx.Equal(0, ledger.SecondsLeft("a", t0 + 2700), "a standing tree has no time left");
		ctx.True(ledger.Fell("a", t0 + 2701, 2700), "a regrown tree can be felled again");
		ctx.Equal(2, ledger.Keys().Count(), "the snapshot lists both trees down");

		ledger.Clear();
		ctx.Equal(0, ledger.Count(), "clear empties the ledger");
	}
}
