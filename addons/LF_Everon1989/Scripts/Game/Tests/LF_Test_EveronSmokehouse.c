// red-proof: before the card the pack had no smokehouse recipes: the first run found no "smokehouse-<species>" machine
// for any species and no smoked-fish catalogue row, so every check below failed. RedProof() also skips the machine
// lookup, so `tools\cli test --no-build --red LF_Test_EveronSmokehouse` fails the same way.

//! everon/smokehouse (#3450): under the Everon 1989 header (the pack leg), every species of the fishing config has a
//! smokehouse recipe (a fresh fish and a firewood bundle in, a smoked fish out), and the fish counter pays for the
//! smoked fish more than for the fresh one plus the firewood it took.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronSmokehouse : LF_Test
{
	//! The firewood bundle's sell price under the core `Prefabs/Resources/WoodCutting/` rule of ShopPrices.conf.
	protected static const int FIREWOOD_SELL = 5;
	protected static const ResourceName SMOKEHOUSE = "Prefabs/Props/Fishing/LF_Smokehouse.et";
	protected static const ResourceName FIREWOOD = "{F96D821A8E47FBBA}Prefabs/Resources/WoodCutting/FirewoodBundle.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/smokehouse";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		Resource house = Resource.Load(SMOKEHOUSE);
		ctx.True(house && house.IsValid(), "the smokehouse prefab loads");

		LF_FishingConfig fishing = LF_FishingConfig.Get();
		LF_ItemCatalogueConfig catalogue = LF_ItemCatalogueConfig.Get();
		ctx.NotNull(fishing, "the fishing config loads");
		ctx.NotNull(catalogue, "the item catalogue loads");
		if (!fishing || !catalogue)
			return;

		ctx.True(fishing.m_aSpecies.Count() > 0, "the fishing config has species");
		foreach (LF_FishSpeciesDef species : fishing.m_aSpecies)
			CheckSpecies(ctx, catalogue, species);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckSpecies(LF_TestContext ctx, LF_ItemCatalogueConfig catalogue, LF_FishSpeciesDef species)
	{
		string id = species.m_sId;
		LF_ResourceMachineDef machine;
		if (!RedProof())
			machine = LF_ResourceChainCatalog.FindMachine("smokehouse-" + id);

		ctx.NotNull(machine, id + ": the smokehouse has a recipe");
		if (!machine)
			return;

		ctx.True(LF_ResourceChainCatalog.IsMachineUsable(machine), id + ": the recipe resolves");
		ctx.Equal(2, machine.m_aInputs.Count(), id + ": the recipe takes two inputs");
		ctx.Equal(1, machine.m_aOutputs.Count(), id + ": the recipe gives one output");
		if (machine.m_aInputs.Count() != 2 || machine.m_aOutputs.Count() != 1)
			return;

		ctx.EqualStr(species.m_aSizePrefabs[1], machine.m_aInputs[0].m_InputPrefab, id + ": the fish in is the medium fresh fish");
		ctx.EqualStr(FIREWOOD, machine.m_aInputs[1].m_InputPrefab, id + ": the second input is a firewood bundle");

		LF_CatalogueItem fresh = catalogue.FindItem("fish_" + id);
		LF_CatalogueItem smoked = catalogue.FindItem("fish_smoked_" + id);
		ctx.NotNull(fresh, id + ": the fresh fish has a catalogue row");
		ctx.NotNull(smoked, id + ": the smoked fish has a catalogue row");
		if (!fresh || !smoked)
			return;

		ctx.EqualStr(machine.m_aOutputs[0].m_OutputPrefab, smoked.m_Prefab, id + ": the recipe gives the catalogue's smoked fish");
		ctx.True(smoked.m_iSellPrice > fresh.m_iSellPrice + FIREWOOD_SELL, id + ": smoked sells above fresh plus the firewood");
		ctx.NotNull(catalogue.FindOffer("fish_counter", "fish_smoked_" + id), id + ": the fish counter handles the smoked fish");
	}
}
