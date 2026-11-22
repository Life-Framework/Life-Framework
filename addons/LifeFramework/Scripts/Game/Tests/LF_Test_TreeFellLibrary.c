// red-proof: 2026-11-22, `--red LF_Test_TreeFellLibrary` printed "RED PROOF OK (all 1 listed test(s) fail with the hook on)" (the hook reads the placed prefab path, the old rule: "a library pine is softwood (expected 1, got 0)", "every rule passes for a library birch ... (expected 0, got 2)", "the census counts the library birch as fellable by ancestry"). Then `--only` passed (27 assertions).
//! lumber/tree-fell-library: a map tree placed from the editor's PrefabLibrary can be felled. The world
//! places trees such as PrefabLibrary/DefaultLibrary/Vegetation/Trees/t_betula_pendula_2s.et, whose own
//! path names no species folder; the rules read the species prefab it inherits
//! (Prefabs/Vegetation/Tree/<species>/...) so the species gives the log type (a birch is hardwood, a
//! pine softwood) and a stump, fallen trunk or sapling from the same library stays unfellable. The
//! ledger key still uses the placed prefab, so no existing key moves.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_TreeFellLibrary : LF_Test
{
	protected static const ResourceName BIRCH_2S = "{DB49D8B142D18E86}PrefabLibrary/DefaultLibrary/Vegetation/Trees/t_betula_pendula_2s.et";
	protected static const ResourceName BIRCH_AUT = "{35A00F5C3C6CE2FB}PrefabLibrary/CainLibrary/Vegetation/Trees/t_betula_pendula_2s_aut.et";
	protected static const ResourceName PINE_V2 = "{0572596E23DDDC10}PrefabLibrary/DefaultLibrary/Vegetation/Trees/t_pinus_sylvestris_2f.et";
	protected static const ResourceName BIRCH_STUMP = "{D9D5932A42E3288B}PrefabLibrary/DefaultLibrary/Vegetation/Trees/t_betula_pendula_stump_01.et";
	protected static const ResourceName SPRUCE_FALLEN = "{AA14F25DFBDF4945}PrefabLibrary/DefaultLibrary/Vegetation/Trees/t_picea_abies_3d_fallen.et";
	protected static const ResourceName BIRCH_SAPLING = "{531BB93086610CA1}PrefabLibrary/DefaultLibrary/Vegetation/Trees/t_betula_pendula_0.et";
	protected static const string SPECIES_BIRCH = "{5C594649DBDD5EAE}Prefabs/Vegetation/Tree/t_betula_pendula/t_betula_pendula_2s.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "lumber/tree-fell-library";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_TreeFellingConfig config = LF_TreeFellingConfig.Get();
		ctx.NotNull(config, "the tree species config loads");
		if (!config)
			return;

		CheckChains(ctx, config);
		CheckEntities(ctx, config);
		CheckFamilies(ctx);
	}

	//------------------------------------------------------------------------------------------------
	//! The pure half: a chain is the placed prefab, then its ancestors.
	protected void CheckChains(LF_TestContext ctx, LF_TreeFellingConfig config)
	{
		ctx.False(LF_TreeFelling.IsSpeciesPath("{DB49D8B142D18E86}PrefabLibrary/DefaultLibrary/Vegetation/Trees/t_betula_pendula_2s.et"), "a PrefabLibrary path names no species folder");
		ctx.True(LF_TreeFelling.IsSpeciesPath(SPECIES_BIRCH), "a species prefab path, GUID and all, is a species path");

		array<string> chain = { BIRCH_2S, SPECIES_BIRCH, "Prefabs/Vegetation/Tree/t_betula_pendula/t_betula_pendula_base.et" };
		ResourceName rules = LF_TreeFelling.RulesPrefabOf(chain);
		if (RedProof())
			rules = BIRCH_2S;

		LF_TreeKind kind = LF_TreeFelling.KindOf(rules, config);
		ctx.Equal(LF_TreeWoodClass.HARDWOOD, kind.m_eClass, "a library birch reads as hardwood through its species prefab");
		ctx.Equal(2, kind.m_iSize, "the library birch keeps its size 2");

		array<string> own = { SPECIES_BIRCH };
		ctx.EqualStr(SPECIES_BIRCH, LF_TreeFelling.RulesPrefabOf(own), "a tree placed from the species prefab reads that prefab");

		array<string> none = { BIRCH_2S, "Prefabs/Props/Crates/Crate_01.et" };
		ctx.EqualStr(BIRCH_2S, LF_TreeFelling.RulesPrefabOf(none), "a chain with no species prefab returns the placed prefab");
		ctx.Equal(LF_TreeWoodClass.NONE, LF_TreeFelling.KindOf(LF_TreeFelling.RulesPrefabOf(none), config).m_eClass, "and that is no tree");

		array<string> empty = {};
		ctx.True(LF_TreeFelling.RulesPrefabOf(empty).IsEmpty(), "an empty chain has no prefab");
	}

	//------------------------------------------------------------------------------------------------
	//! The engine half: real PrefabLibrary prefabs, read through their real ancestry.
	protected void CheckEntities(LF_TestContext ctx, LF_TreeFellingConfig config)
	{
		BaseWorld world = GetGame().GetWorld();
		vector spot = LF_TestDebrisProbe.Gate() + "60 0 60";
		spot[1] = world.GetSurfaceY(spot[0], spot[2]);

		IEntity birch = LF_Utils.SpawnEntityPrefab(BIRCH_2S, spot);
		IEntity birchAut = LF_Utils.SpawnEntityPrefab(BIRCH_AUT, spot + "6 0 0");
		IEntity pine = LF_Utils.SpawnEntityPrefab(PINE_V2, spot + "12 0 0");
		IEntity stump = LF_Utils.SpawnEntityPrefab(BIRCH_STUMP, spot + "18 0 0");
		IEntity fallen = LF_Utils.SpawnEntityPrefab(SPRUCE_FALLEN, spot + "24 0 0");
		IEntity sapling = LF_Utils.SpawnEntityPrefab(BIRCH_SAPLING, spot + "30 0 0");
		ctx.NotNull(birch, "the PrefabLibrary birch spawns");
		ctx.NotNull(pine, "the PrefabLibrary pine spawns");
		if (birch && birchAut && pine && stump && fallen && sapling)
		{
			ctx.True(LF_TreeFelling.IsSpeciesPath(Rules(birch)), "the library birch resolves to its species prefab");
			ctx.False(LF_TreeFelling.IsSpeciesPath(LF_FellAction.PrefabOf(birch)), "the placed prefab (the ledger key's) is still the library one");
			ctx.True(LF_FellAction.KeyOfTree(birch).Contains("prefablibrary/defaultlibrary"), "the ledger key keeps the placed prefab path, so no key moves");

			ctx.Equal(LF_TreeWoodClass.HARDWOOD, LF_TreeFelling.KindOf(Rules(birch), config).m_eClass, "a library birch is hardwood");
			ctx.Equal(LF_TreeWoodClass.HARDWOOD, LF_TreeFelling.KindOf(Rules(birchAut), config).m_eClass, "a library _aut birch is hardwood");
			ctx.Equal(LF_TreeWoodClass.SOFTWOOD, LF_TreeFelling.KindOf(Rules(pine), config).m_eClass, "a library pine is softwood");

			ctx.True(LF_FellPrompt.IsFellable(birch, config), "the prompt offers the library birch");
			ctx.True(LF_FellPrompt.IsFellable(pine, config), "the prompt offers the library pine");
			ctx.False(LF_FellPrompt.IsFellable(stump, config), "a library stump is not a tree to fell");
			ctx.False(LF_FellPrompt.IsFellable(fallen, config), "a library fallen trunk is not a tree to fell");
			ctx.False(LF_FellPrompt.IsFellable(sapling, config), "a library sapling is not a tree to fell");

			LF_FellFacts facts = new LF_FellFacts();
			facts.m_bHasAxe = true;
			facts.m_sPrefab = Rules(birch);
			ctx.Equal(LF_FellRefusal.OK, LF_TreeFelling.Check(facts, config), "every rule passes for a library birch with an axe in an open spot");
			ctx.True(LF_TreeCensus.IsFellablePrefab(Rules(birch), config), "the census counts the library birch as fellable by ancestry");
			ctx.False(LF_TreeCensus.IsFellablePrefab(LF_FellAction.PrefabOf(birch), config), "and not by the placed path alone, which is the bug");
		}

		LF_TestCharacterFixture.Delete(birch);
		LF_TestCharacterFixture.Delete(birchAut);
		LF_TestCharacterFixture.Delete(pine);
		LF_TestCharacterFixture.Delete(stump);
		LF_TestCharacterFixture.Delete(fallen);
		LF_TestCharacterFixture.Delete(sapling);
	}

	//------------------------------------------------------------------------------------------------
	//! The prefab the rules read for a tree; the red-proof hook reads the placed path alone, the old rule.
	protected ResourceName Rules(IEntity tree)
	{
		if (RedProof())
			return LF_FellAction.PrefabOf(tree);

		return LF_FellAction.TreePrefabOf(tree);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckFamilies(LF_TestContext ctx)
	{
		ctx.EqualStr("prefablibrary/defaultlibrary/vegetation/trees", LF_TreeCensus.FamilyOf(BIRCH_2S), "the census family is the folder, lower case");
		ctx.EqualStr("prefabs/vegetation/tree/t_betula_pendula", LF_TreeCensus.FamilyOf(SPECIES_BIRCH), "a GUID is dropped from the family");
	}
}
