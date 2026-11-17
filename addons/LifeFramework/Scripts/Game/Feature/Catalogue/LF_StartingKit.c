// LF_StartingKit.c — the catalogue's `starting_kit` outlet (#1767): what a fresh civilian spawns
// wearing and carrying. A server that lists offers under that outlet replaces the game mode's default
// civilian loadout with them; a server that lists none keeps the loadout (LF_FactionLoadout.WithStartingKit).

class LF_StartingKit
{
	//! The outlet id whose offers are the kit. No counter sells it; the spawn reads it.
	static const string OUTLET = "starting_kit";

	//------------------------------------------------------------------------------------------------
	//! The prefabs of the kit, in catalogue order, split by how they reach the body: clothing goes into
	//! a loadout slot (shirt, trousers, hat), everything else is inserted by best-slot search (a pistol,
	//! a bag, food). An offer's gate is not read: nobody buys the kit, it is handed over.
	//! \return True when the outlet has at least one live offer, i.e. a kit is configured.
	static bool Resolve(LF_ItemCatalogueConfig config, notnull array<ResourceName> outClothing, notnull array<ResourceName> outDirect)
	{
		outClothing.Clear();
		outDirect.Clear();
		array<LF_OutletOffer> offers = {};
		LF_ItemCatalogue.Offers(config, OUTLET, offers);
		foreach (LF_OutletOffer offer : offers)
		{
			LF_CatalogueItem item = LF_ItemCatalogue.Item(config, offer.m_sItem);
			if (!item || item.m_Prefab.IsEmpty())
				continue;

			if (item.GetCategory() == LF_EItemCategory.CLOTHING)
				outClothing.Insert(item.m_Prefab);
			else
				outDirect.Insert(item.m_Prefab);
		}

		return !outClothing.IsEmpty() || !outDirect.IsEmpty();
	}
}
