//------------------------------------------------------------------------------------------------
//! Officer searches for chain goods (#419). The goods are the outputs of the last step of every
//! enabled chain flagged m_bContraband; nothing else is ever found. One roll decides a search:
//! the bigger the load (units times the chain's bulk), the harder it is to hide, so harder tiers
//! with bulky goods are found more often. Server only. A find seizes the goods and files the
//! offence through LF_WantedReports.Report, which publishes LF_OnCrimeCommitted on the bus.
class LF_ChainSearch
{
	static const string FEATURE = "Chains";
	static const float BASE_CHANCE = 0.3;
	static const float CHANCE_PER_LOAD = 0.2;
	//! Furthest an officer may stand from a searched vehicle (its origin is mid-body).
	static const float VEHICLE_REACH = 8.0;

	//! World time of the last search per target key. Server only; one int per searched target.
	protected static ref map<string, int> s_mLastSearch = new map<string, int>();

	//------------------------------------------------------------------------------------------------
	//! Chance a search finds a load. Zero for no goods, one at a load of four.
	static float FindChance(int load)
	{
		if (load <= 0)
			return 0;

		return Math.Clamp(BASE_CHANCE + CHANCE_PER_LOAD * load, 0, 1);
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a target searched at lastMs (negative: never) may be searched again at nowMs. Pure.
	static bool MaySearch(int lastMs, int nowMs, int cooldownMs)
	{
		if (lastMs < 0 || cooldownMs <= 0)
			return true;

		return nowMs - lastMs >= cooldownMs;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: claims a search of the target (key "p:<uid>" or "v:<rplId>") at the world time. False when
	//! it was searched inside the configured cooldown; the caller then rolls nothing and reports clean (#472).
	static bool TryBeginSearch(string key, int nowMs)
	{
		int cooldownMs = LF_PoliceConfig.SearchCooldownSeconds() * 1000;
		int lastMs = -1;
		if (s_mLastSearch.Find(key, lastMs) && !MaySearch(lastMs, nowMs, cooldownMs))
		{
			LF_Debug.Info(FEATURE, string.Format("search of %1 refused: searched %2 ms ago, cooldown %3 ms", key, nowMs - lastMs, cooldownMs));
			return false;
		}

		s_mLastSearch.Set(key, nowMs);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetSearchCooldowns()
	{
		s_mLastSearch.Clear();
	}

	//------------------------------------------------------------------------------------------------
	static string PersonKey(IEntity target)
	{
		return "p:" + LF_Utils.GetPlayerUID(LF_PlayerInteraction.PlayerIdOf(target));
	}

	//------------------------------------------------------------------------------------------------
	static string VehicleKey(IEntity vehicle)
	{
		return string.Format("v:%1", LF_NetworkUtils.GetRplId(vehicle));
	}

	//------------------------------------------------------------------------------------------------
	static bool Finds(int load, float roll)
	{
		return roll < FindChance(load);
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the prefab is flagged chain goods, and how bulky one unit is.
	static bool IsGood(ResourceName prefab, out int bulk)
	{
		bulk = 0;
		if (prefab.IsEmpty())
			return false;

		array<LF_ChainDef> chains = {};
		LF_ChainCatalog.GetEnabled(chains);
		foreach (LF_ChainDef chain : chains)
		{
			if (!chain.m_bContraband || !chain.m_aSteps || chain.m_aSteps.IsEmpty())
				continue;

			LF_ChainStepDef last = chain.m_aSteps[chain.m_aSteps.Count() - 1];
			if (!last || !last.m_aOutputs)
				continue;

			foreach (LF_ProcessingOutput output : last.m_aOutputs)
			{
				if (output && LF_Utils.SamePrefab(output.m_OutputPrefab, prefab))
				{
					bulk = Math.Max(chain.m_iBulk, 1);
					return true;
				}
			}
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Sums a carried list: how many units are flagged goods and how heavy that load is. Pure.
	//! \param indices Receives the index of each flagged entry.
	static void Tally(notnull array<ResourceName> prefabs, notnull array<int> quantities, out int units, out int load, notnull array<int> indices)
	{
		units = 0;
		load = 0;
		indices.Clear();
		for (int i = 0; i < prefabs.Count(); i++)
		{
			int bulk;
			if (!IsGood(prefabs[i], bulk))
				continue;

			int quantity = 1;
			if (i < quantities.Count())
				quantity = Math.Max(quantities[i], 1);

			units += quantity;
			load += quantity * bulk;
			indices.Insert(i);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Every flagged-goods item in the inventory tree under root (a character or a vehicle).
	static void Collect(IEntity root, notnull array<IEntity> outGoods, out int units, out int load)
	{
		units = 0;
		load = 0;
		outGoods.Clear();
		if (!root)
			return;

		array<IEntity> visited = {};
		CollectFrom(root, outGoods, visited, units, load);
	}

	//------------------------------------------------------------------------------------------------
	protected static void CollectFrom(IEntity owner, notnull array<IEntity> goods, notnull array<IEntity> visited, out int units, out int load)
	{
		array<Managed> components = {};
		owner.FindComponents(BaseInventoryStorageComponent, components);
		foreach (Managed component : components)
		{
			BaseInventoryStorageComponent storage = BaseInventoryStorageComponent.Cast(component);
			if (!storage)
				continue;

			int slots = storage.GetSlotsCount();
			for (int slot = 0; slot < slots; slot++)
			{
				IEntity item = storage.Get(slot);
				if (!item || visited.Find(item) != -1)
					continue;

				visited.Insert(item);
				int bulk;
				if (IsGood(LF_Utils.GetPrefabName(item), bulk))
				{
					int quantity = 1;
					LF_QuantityComponent quantityComponent = LF_QuantityComponent.Cast(item.FindComponent(LF_QuantityComponent));
					if (quantityComponent)
						quantity = Math.Max(quantityComponent.GetQuantity(), 1);

					goods.Insert(item);
					units += quantity;
					load += quantity * bulk;
					continue;
				}

				CollectFrom(item, goods, visited, units, load);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: an officer searches a player. A find seizes the goods and files the offence against
	//! the player. \return Units seized; 0 when the search found nothing (clean or well hidden).
	static int SearchPerson(IEntity target, float roll)
	{
		int units;
		int load;
		array<IEntity> goods = {};
		Collect(target, goods, units, load);
		if (goods.IsEmpty() || !Finds(load, roll))
		{
			LF_Debug.Info(FEATURE, string.Format("person search found nothing (carrying %1 flagged unit(s), roll %2)", units, roll));
			return 0;
		}

		string uid = LF_Utils.GetPlayerUID(LF_PlayerInteraction.PlayerIdOf(target));
		Seize(target, goods, true);
		LF_WantedReports.Report(uid, target, LF_WantedReports.OFFENCE_CHAIN_GOODS);
		LF_Debug.Info(FEATURE, string.Format("person search seized %1 flagged unit(s) from %2", units, uid));
		return units;
	}

	//------------------------------------------------------------------------------------------------
	//! Who answers for what is found in a vehicle. Today the people sitting in it.
	//! HOOK (#414): the plate-check API adds the registered owner of the plate here, so goods in a
	//! parked car still file against someone.
	static void SuspectsOf(IEntity vehicle, notnull array<IEntity> outSuspects)
	{
		outSuspects.Clear();
		if (!vehicle)
			return;

		SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
		if (!compartments)
			return;

		array<IEntity> occupants = {};
		compartments.GetOccupants(occupants);
		foreach (IEntity occupant : occupants)
		{
			if (occupant && LF_PlayerInteraction.PlayerIdOf(occupant) > 0)
				outSuspects.Insert(occupant);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: an officer searches a vehicle's storage. A find seizes the goods and files the
	//! offence against each suspect (SuspectsOf); with none the goods are still taken and logged.
	//! \return Units seized.
	static int SearchVehicle(IEntity vehicle, float roll)
	{
		int units;
		int load;
		array<IEntity> goods = {};
		Collect(vehicle, goods, units, load);
		if (goods.IsEmpty() || !Finds(load, roll))
		{
			LF_Debug.Info(FEATURE, string.Format("vehicle search found nothing (carrying %1 flagged unit(s), roll %2)", units, roll));
			return 0;
		}

		array<IEntity> suspects = {};
		SuspectsOf(vehicle, suspects);
		Seize(null, goods, false);
		foreach (IEntity suspect : suspects)
		{
			LF_WantedReports.Report(LF_Utils.GetPlayerUID(LF_PlayerInteraction.PlayerIdOf(suspect)), suspect, LF_WantedReports.OFFENCE_CHAIN_GOODS);
		}

		LF_Debug.Info(FEATURE, string.Format("vehicle search seized %1 flagged unit(s); %2 suspect(s) charged", units, suspects.Count()));
		return units;
	}

	//------------------------------------------------------------------------------------------------
	//! Collect first, then delete: deleting while walking a live list skips items.
	protected static void Seize(IEntity carrier, notnull array<IEntity> goods, bool dropFirst)
	{
		foreach (IEntity item : goods)
		{
			if (!item)
				continue;

			if (dropFirst && carrier)
				LF_InventoryUtils.DropItem(carrier, item);

			SCR_EntityHelper.DeleteEntityAndChildren(item);
		}
	}
}
