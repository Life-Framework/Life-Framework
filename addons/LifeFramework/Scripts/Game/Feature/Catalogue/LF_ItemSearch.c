//------------------------------------------------------------------------------------------------
//! A police search reads item legality (#1764, #1067): each carried item the catalogue names is
//! checked against the carrier (LF_ItemCatalogue.OffenceFor); an item that is an offence for them
//! is listed, seized and its offence filed once per kind on the crime bus (LF_WantedReports.Report).
//! LF_ConfiscateAction calls it on the server; an item the catalogue does not list is left alone.
class LF_ItemSearch
{
	//------------------------------------------------------------------------------------------------
	//! Every item a character carries: the inventory and the equipped-weapon storage, each item once.
	static void CarriedItems(IEntity carrier, notnull array<IEntity> outItems)
	{
		outItems.Clear();
		if (!carrier)
			return;

		array<IEntity> found = {};
		SCR_InventoryStorageManagerComponent manager = LF_Component<SCR_InventoryStorageManagerComponent>.Find(carrier);
		if (manager)
			manager.FindItems(found, null);

		EquipedWeaponStorageComponent weaponStorage = LF_Component<EquipedWeaponStorageComponent>.Find(carrier);
		if (weaponStorage)
		{
			array<IEntity> weapons = {};
			weaponStorage.GetAll(weapons);
			found.InsertAll(weapons);
		}

		foreach (IEntity item : found)
		{
			if (item && !outItems.Contains(item))
				outItems.Insert(item);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The carried items that are an offence for this carrier, with each item's offence at the same
	//! index. Reads only; nothing is moved.
	static void Unlawful(LF_ItemCatalogueConfig config, notnull array<IEntity> items, notnull LF_BuyerFacts carrier, notnull array<IEntity> outItems, notnull array<string> outOffences)
	{
		outItems.Clear();
		outOffences.Clear();
		if (!config)
			return;

		foreach (IEntity item : items)
		{
			if (!item)
				continue;

			string offence = LF_ItemCatalogue.OffenceFor(LF_ItemCatalogue.ItemByPrefab(config, LF_Utils.GetPrefabName(item)), carrier);
			if (offence.IsEmpty())
				continue;

			outItems.Insert(item);
			outOffences.Insert(offence);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: searches a character, seizes every item that is an offence for them and files each
	//! offence kind once against their uid. Collects first, then deletes (deleting while walking the
	//! live inventory skips items).
	//! \param[out] outOffences The distinct offences filed, in the order found.
	//! \return How many items were seized.
	static int SearchAndSeize(IEntity suspect, string suspectUid, LF_BuyerFacts carrier, LF_ItemCatalogueConfig config, notnull array<string> outOffences)
	{
		outOffences.Clear();
		if (!Replication.IsServer())
			return 0;

		if (!suspect || !carrier)
		{
			LF_Debug.Error(LF_ItemCatalogue.FEATURE, "search refused: no suspect or no carrier facts");
			return 0;
		}

		if (!config)
		{
			LF_Debug.Warn(LF_ItemCatalogue.FEATURE, "search skipped legality: the item catalogue did not load");
			return 0;
		}

		// The police kit issues catalogue prefabs (the M9) without the civilian permit: an on-duty
		// officer's issued kit is not an offence.
		if (carrier.JobRank(LF_EntityBuyerFacts.JOB_POLICE) >= 0)
		{
			LF_Debug.Info(LF_ItemCatalogue.FEATURE, string.Format("search of %1 skips legality: an on-duty officer", suspectUid));
			return 0;
		}

		array<IEntity> carried = {};
		CarriedItems(suspect, carried);
		array<IEntity> unlawful = {};
		array<string> offences = {};
		Unlawful(config, carried, carrier, unlawful, offences);

		for (int i = 0; i < unlawful.Count(); i++)
		{
			if (!outOffences.Contains(offences[i]))
				outOffences.Insert(offences[i]);

			// A flagged item inside a flagged container went with the container.
			IEntity item = unlawful[i];
			if (!item)
				continue;

			LF_Debug.Info(LF_ItemCatalogue.FEATURE, string.Format("search of %1 flags %2: %3", suspectUid, LF_Utils.GetPrefabName(item), offences[i]));
			LF_InventoryUtils.DropItem(suspect, item);
			SCR_EntityHelper.DeleteEntityAndChildren(item);
		}

		if (!outOffences.IsEmpty())
		{
			if (suspectUid.IsEmpty())
				LF_Debug.Warn(LF_ItemCatalogue.FEATURE, string.Format("search seized %1 item(s) but the suspect has no uid: no offence filed", unlawful.Count()));

			foreach (string offence : outOffences)
			{
				if (!suspectUid.IsEmpty())
					LF_WantedReports.Report(suspectUid, suspect, offence);
			}
		}

		LF_Debug.Info(LF_ItemCatalogue.FEATURE, string.Format("search of %1: %2 of %3 carried item(s) seized, %4 offence(s) filed", suspectUid, unlawful.Count(), carried.Count(), outOffences.Count()));
		return unlawful.Count();
	}
}
