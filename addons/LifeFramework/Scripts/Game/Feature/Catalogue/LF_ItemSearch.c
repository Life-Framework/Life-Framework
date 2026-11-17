//------------------------------------------------------------------------------------------------
//! A police search reads item legality (#1764, #1067): each carried item the catalogue names is
//! checked against the carrier (LF_ItemCatalogue.OffenceFor); an item that is an offence for them
//! is listed, seized and its offence filed once per kind on the crime bus (LF_WantedReports.Report).
//! LF_ConfiscateAction calls it on the server; an item the catalogue does not list is left alone.
class LF_ItemSearch
{
	//! Kinds the officer's toast names before it folds the rest into "and N more".
	protected static const int SEIZED_NAMED = 3;

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
	//! The name an officer reads for a seized item: its inventory display name, translated; the prefab
	//! file name when the prefab has none.
	protected static string SeizedName(IEntity item)
	{
		UIInfo info = LF_UIInfoUtils.GetInfo(item);
		if (info && !info.GetName().IsEmpty())
			return WidgetManager.Translate(info.GetName());

		string prefab = LF_Utils.GetPrefabName(item);
		int slash = prefab.LastIndexOf("/");
		if (slash >= 0)
			prefab = prefab.Substring(slash + 1, prefab.Length() - slash - 1);

		return prefab;
	}

	//------------------------------------------------------------------------------------------------
	//! One line per seized kind, in the order found: the display name, with its count in front when
	//! more than one of it went ("2x 30-round magazine"). Reads the items before they are deleted.
	protected static void SeizedLines(notnull array<IEntity> items, notnull array<string> outLines)
	{
		outLines.Clear();
		array<string> names = {};
		array<int> counts = {};
		foreach (IEntity item : items)
		{
			if (!item)
				continue;

			string name = SeizedName(item);
			int at = names.Find(name);
			if (at < 0)
			{
				names.Insert(name);
				counts.Insert(1);
			}
			else
			{
				counts[at] = counts[at] + 1;
			}
		}

		for (int i = 0; i < names.Count(); i++)
		{
			string line = names[i];
			if (counts[i] > 1)
				line = WidgetManager.Translate("#LF-Search_SeizedCount", counts[i].ToString(), names[i]);

			outLines.Insert(line);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The officer's toast text for a search: "Seized from <suspect>: A, B, C and 2 more". Three kinds
	//! are named, the rest fold into the count. Empty when nothing was seized.
	static string SeizedText(string suspectName, notnull array<string> lines)
	{
		if (lines.IsEmpty())
			return string.Empty;

		string list;
		for (int i = 0; i < lines.Count() && i < SEIZED_NAMED; i++)
		{
			if (i > 0)
				list += ", ";

			list += lines[i];
		}

		if (lines.Count() > SEIZED_NAMED)
			list = WidgetManager.Translate("#LF-Search_SeizedMore", list, (lines.Count() - SEIZED_NAMED).ToString());

		return WidgetManager.Translate("#LF-Search_SeizedFrom", suspectName, list);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: searches a character, seizes every item that is an offence for them and files each
	//! offence kind once against their uid. Collects first, then deletes (deleting while walking the
	//! live inventory skips items).
	//! \param[out] outOffences The distinct offences filed, in the order found.
	//! \param[out] outSeized Optional: what was seized, one display line per kind (SeizedText builds the toast from it).
	//! \return How many items were seized.
	static int SearchAndSeize(IEntity suspect, string suspectUid, LF_BuyerFacts carrier, LF_ItemCatalogueConfig config, notnull array<string> outOffences, array<string> outSeized = null)
	{
		outOffences.Clear();
		if (outSeized)
			outSeized.Clear();

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
		if (outSeized)
			SeizedLines(unlawful, outSeized);

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
