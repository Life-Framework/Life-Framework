//------------------------------------------------------------------------------------------------
//! What class of contraband an item is, for a stop search (crime B7, #4196). LEGAL is everything else.
enum LF_EVehicleSearchGoodsClass
{
	LEGAL,
	TOBACCO,
	ALCOHOL,
	NARCOTICS,
	UNTAXED
}

[ComponentEditorProps(category: "EveronLife/Feature/Police", description: "Marks an item as contraband of one class: a stop search lists it and an officer can seize it (#4196).")]
class LF_VehicleSearchGoodsComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! The marker a contraband item carries (a cigarette carton, a crate of vodka). The pack puts it on
//! its item prefabs; the core reads only the class.
class LF_VehicleSearchGoodsComponent : ScriptComponent
{
	[Attribute("1", UIWidgets.ComboBox, "Contraband class a stop search reports this item as", enums: ParamEnumArray.FromEnum(LF_EVehicleSearchGoodsClass))]
	protected LF_EVehicleSearchGoodsClass m_eGoodsClass;

	//------------------------------------------------------------------------------------------------
	LF_EVehicleSearchGoodsClass GetGoodsClass()
	{
		return m_eGoodsClass;
	}
}

//------------------------------------------------------------------------------------------------
//! A stop search for contraband in a car's storage (crime B7, #4196). Two steps on the server: the
//! search lists what it found by class and marks the car; the officer's next search of that car seizes
//! exactly those classed goods, once. Legal goods are never touched. The find uses the same chance as
//! chain goods (LF_ChainSearch.FindChance), so a bigger load is harder to hide.
class LF_VehicleSearchGoods
{
	static const string FEATURE = "Police";
	//! How long a find waits for the seizure; after that the car must be searched again.
	static const int PENDING_MS = 300000; // lf-fixed: a stale-state guard between two searches by one officer, not a server tunable

	//! Prefab -> class for items without the marker (a pack script, a test). Server only.
	protected static ref map<string, int> s_mClassByPrefab = new map<string, int>();
	//! Vehicle key -> world time of the find not yet seized. Server only.
	protected static ref map<string, int> s_mPending = new map<string, int>();

	//------------------------------------------------------------------------------------------------
	static void Register(ResourceName prefab, LF_EVehicleSearchGoodsClass goodsClass)
	{
		if (prefab.IsEmpty())
		{
			LF_Debug.Error(FEATURE, "contraband class register refused: empty prefab");
			return;
		}

		s_mClassByPrefab.Set(prefab, goodsClass);
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mClassByPrefab.Clear();
		s_mPending.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! The class of one item: its marker first, then the registered prefab table.
	static LF_EVehicleSearchGoodsClass ClassOf(IEntity item)
	{
		if (!item)
			return LF_EVehicleSearchGoodsClass.LEGAL;

		LF_VehicleSearchGoodsComponent marker = LF_VehicleSearchGoodsComponent.Cast(item.FindComponent(LF_VehicleSearchGoodsComponent));
		if (marker)
			return marker.GetGoodsClass();

		ResourceName prefab = LF_Utils.GetPrefabName(item);
		if (prefab.IsEmpty())
			return LF_EVehicleSearchGoodsClass.LEGAL;

		foreach (string registered, int goodsClass : s_mClassByPrefab)
		{
			if (LF_Utils.SamePrefab(registered, prefab))
				return goodsClass;
		}

		return LF_EVehicleSearchGoodsClass.LEGAL;
	}

	//------------------------------------------------------------------------------------------------
	//! The string key that names a class to the officer.
	static string ClassKey(LF_EVehicleSearchGoodsClass goodsClass)
	{
		switch (goodsClass)
		{
			case LF_EVehicleSearchGoodsClass.TOBACCO: return "#LF-VehicleSearch_ClassTobacco";
			case LF_EVehicleSearchGoodsClass.ALCOHOL: return "#LF-VehicleSearch_ClassAlcohol";
			case LF_EVehicleSearchGoodsClass.NARCOTICS: return "#LF-VehicleSearch_ClassNarcotics";
			case LF_EVehicleSearchGoodsClass.UNTAXED: return "#LF-VehicleSearch_ClassUntaxed";
		}

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! Every classed item in the inventory tree under root, with its class and units, and the units
	//! summed per class (outTotals is indexed by LF_EVehicleSearchGoodsClass).
	static int Collect(IEntity root, notnull array<IEntity> outItems, notnull array<int> outTotals)
	{
		outItems.Clear();
		outTotals.Clear();
		for (int i = 0; i <= LF_EVehicleSearchGoodsClass.UNTAXED; i++)
		{
			outTotals.Insert(0);
		}

		if (!root)
			return 0;

		array<IEntity> visited = {};
		int units = 0;
		CollectFrom(root, outItems, outTotals, visited, units);
		return units;
	}

	//------------------------------------------------------------------------------------------------
	protected static void CollectFrom(IEntity owner, notnull array<IEntity> items, notnull array<int> totals, notnull array<IEntity> visited, inout int units)
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
				LF_EVehicleSearchGoodsClass goodsClass = ClassOf(item);
				if (goodsClass == LF_EVehicleSearchGoodsClass.LEGAL)
				{
					CollectFrom(item, items, totals, visited, units);
					continue;
				}

				int quantity = 1;
				LF_QuantityComponent quantityComponent = LF_QuantityComponent.Cast(item.FindComponent(LF_QuantityComponent));
				if (quantityComponent)
					quantity = LF_Utils.MaxInt(quantityComponent.GetQuantity(), 1);

				items.Insert(item);
				totals[goodsClass] = totals[goodsClass] + quantity;
				units += quantity;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! "2 tobacco, 1 alcohol": the units per class, in class order. Empty when there is nothing.
	static string Describe(notnull array<int> totals)
	{
		string listing;
		for (int i = 1; i < totals.Count(); i++)
		{
			if (totals[i] <= 0)
				continue;

			if (!listing.IsEmpty())
				listing += ", ";

			listing += LF_Terms.Text("#LF-VehicleSearch_ClassLine", totals[i].ToString(), LF_Terms.Text(ClassKey(i)));
		}

		return listing;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the car holds a find inside PENDING_MS that the officer has not seized yet.
	static bool HasPending(IEntity vehicle, int nowMs)
	{
		if (!vehicle)
			return false;

		int foundMs;
		if (!s_mPending.Find(LF_ChainSearch.VehicleKey(vehicle), foundMs))
			return false;

		return nowMs - foundMs < PENDING_MS;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: searches the car's storage for classed goods. A find marks the car for seizure and fills
	//! outListing ("2 tobacco, 1 alcohol"). \return Units found; 0 when clean or well hidden.
	static int Search(IEntity vehicle, float roll, int nowMs, out string outListing)
	{
		outListing = string.Empty;
		if (!vehicle)
			return 0;

		array<IEntity> items = {};
		array<int> totals = {};
		int units = Collect(vehicle, items, totals);
		if (units <= 0 || roll >= LF_ChainSearch.FindChance(units))
		{
			LF_Debug.Info(FEATURE, string.Format("stop search found no contraband (carrying %1 classed unit(s), roll %2)", units, roll));
			return 0;
		}

		outListing = Describe(totals);
		s_mPending.Set(LF_ChainSearch.VehicleKey(vehicle), nowMs);
		LF_Debug.Info(FEATURE, string.Format("stop search found %1 classed unit(s): %2", units, outListing));
		return units;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: seizes the classed goods of a car with a pending find, once, and files the offence against
	//! the people sitting in it. Legal goods stay. \return Units seized; 0 when there was no find.
	static int Seize(IEntity vehicle, int nowMs)
	{
		if (!HasPending(vehicle, nowMs))
		{
			LF_Debug.Info(FEATURE, "seize refused: no pending find on this car");
			return 0;
		}

		s_mPending.Remove(LF_ChainSearch.VehicleKey(vehicle));

		// Collect again: goods taken out since the find are not seized, and nothing legal is listed.
		array<IEntity> items = {};
		array<int> totals = {};
		int units = Collect(vehicle, items, totals);
		foreach (IEntity item : items)
		{
			if (item)
				SCR_EntityHelper.DeleteEntityAndChildren(item);
		}

		if (units <= 0)
		{
			LF_Debug.Info(FEATURE, "seize found the goods gone since the search");
			return 0;
		}

		array<IEntity> suspects = {};
		LF_ChainSearch.SuspectsOf(vehicle, suspects);
		foreach (IEntity suspect : suspects)
		{
			LF_WantedReports.Report(LF_Utils.GetPlayerUID(LF_PlayerInteraction.PlayerIdOf(suspect)), suspect, LF_WantedReports.OFFENCE_CHAIN_GOODS);
		}

		LF_Debug.Info(FEATURE, string.Format("seized %1 classed unit(s) (%2); %3 suspect(s) charged", units, Describe(totals), suspects.Count()));
		return units;
	}
}

//------------------------------------------------------------------------------------------------
class LF_VehicleSearchGoods_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_VehicleSearchGoods";
	}

	override void Apply()
	{
		LF_VehicleSearchGoods.ResetStatics();
	}
}
