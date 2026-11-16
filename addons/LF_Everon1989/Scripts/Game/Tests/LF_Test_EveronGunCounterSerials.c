// red-proof: observed 2026-11-16 (#2166): `tools\cli test --no-build --red LF_Test_EveronGunCounterSerials` (RedProof() checks the plain vanilla PM instead of each offer prefab): RED-OK, fails the PM and M21 serial checks. Without the twins the same assertions fail on the vanilla catalogue prefabs.
// tier: WORLD
//! #2166 in game: under the Everon 1989 header (the pack leg of `tools\cli test`), every weapon the
//! licensed `gun_counter` sells resolves to a prefab with an `LF_SerialComponent`, so
//! `LF_FenceSerials.OnWeaponSale` stamps the sale instead of logging "a weapon prefab needs LF_SerialComponent".
class LF_Test_EveronGunCounterSerials : LF_Test
{
	protected static const string COUNTER = "gun_counter";
	protected static const ResourceName PLAIN_PM = "{C0F7DD85A86B2900}Prefabs/Weapons/Handguns/PM/Handgun_PM.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/gun-counter-serials";
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
		// The leg boots under the pack header; layer the catalogue again so the gun store overlay applies.
		LF_ItemCatalogueConfig.Reload();
		LF_ItemCatalogueConfig config = LF_ItemCatalogueConfig.Get();
		ctx.NotNull(config, "the item catalogue loads under the pack header");
		if (!config)
			return;

		array<LF_OutletOffer> offers = {};
		LF_ItemCatalogue.Offers(config, COUNTER, offers);

		int weapons;
		foreach (LF_OutletOffer offer : offers)
		{
			LF_CatalogueItem item = LF_ItemCatalogue.Item(config, offer.m_sItem);
			if (!item || item.m_sCategory != "weapon")
				continue;

			ResourceName prefab = item.m_Prefab;
			if (RedProof())
				prefab = PLAIN_PM;

			weapons++;
			ctx.True(Serialled(prefab), string.Format("the gun counter's %1 carries a serial (%2)", offer.m_sItem, prefab));
		}

		ctx.True(weapons >= 2, string.Format("the gun counter sells at least the PM and the M21 (found %1)", weapons));
		LF_ItemCatalogueConfig.Reload();
	}
}
