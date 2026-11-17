// red-proof: observed 2026-11-16 (#2152): before the prefab, `tools\cli test --no-build --only LF_Test_EveronArmorySerials` failed `the armory's issued rifle_aks74u carries a serial`; with LF_ArmoryAKS74U and m_IssuePrefab it passes. `--red LF_Test_EveronArmorySerials` (RedProof() drops m_IssuePrefab): RED PROOF OK.
// tier: WORLD
//! #2152 in game: under the Everon 1989 header (the pack leg of `tools\cli test`), every weapon the
//! police armory issues by rank carries a serial. For each live issue offer of the `police_armory`
//! outlet, the prefab the armory spawns (`m_IssuePrefab`, else the item's own prefab) has an
//! `LF_SerialComponent`, so `LF_PoliceRanks.IssueOffer` books the weapon instead of logging "untracked".
class LF_Test_EveronArmorySerials : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/armory-serials";
	}

	//------------------------------------------------------------------------------------------------
	//! True when the prefab spawns an entity that carries a serial component.
	protected static bool Serialled(ResourceName prefab)
	{
		IEntity spawned = LF_Utils.SpawnEntityPrefab(prefab, Vector(0, 500, 0));
		if (!spawned)
			return false;

		bool found = spawned.FindComponent(LF_SerialComponent) != null;
		SCR_EntityHelper.DeleteEntityAndChildren(spawned);
		return found;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// The leg boots under the pack header; layer the catalogue again so its weapons overlay applies.
		LF_ItemCatalogueConfig.Reload();
		LF_ItemCatalogueConfig config = LF_ItemCatalogueConfig.Get();
		ctx.NotNull(config, "the item catalogue loads under the pack header");
		if (!config)
			return;

		array<LF_OutletOffer> offers = {};
		LF_ItemCatalogue.Offers(config, LF_PoliceRanks.ARMORY_OUTLET, offers);

		int issued;
		foreach (LF_OutletOffer offer : offers)
		{
			if (!offer.m_bIssue)
				continue;

			LF_CatalogueItem item = LF_ItemCatalogue.Item(config, offer.m_sItem);
			if (!item)
				continue;

			ResourceName prefab = offer.m_IssuePrefab;
			if (prefab.IsEmpty() || RedProof())
				prefab = item.m_Prefab;

			// A catalogue item that is not a weapon (a kit, a magazine) is issued as itself and needs no serial.
			if (item.m_sCategory != "weapon")
				continue;

			issued++;
			ctx.True(Serialled(prefab), string.Format("the armory's issued %1 carries a serial (%2)", offer.m_sItem, prefab));
		}

		ctx.True(issued > 0, "the armory issues at least one weapon");
		LF_ItemCatalogueConfig.Reload();
	}
}
