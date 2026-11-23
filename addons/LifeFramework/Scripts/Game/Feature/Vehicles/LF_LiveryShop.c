// LF_LiveryShop.c - respray and liveries at the mechanic's bay (WO-L9, #4473).
//
// A paint job is a spawn prefab, as a lot colour is (LF_VehicleColours.c): the respray puts the
// owner's car through the garage path (Store, then Retrieve at the bay's pad) with the record's
// spawn prefab changed, so the owner, key, plate, lock state, cargo and the saved record stay the
// car's own, and a restart respawns the new paint with no new save field. Fuel is carried across
// by hand: the garage path does not keep it.
//
// The options for a car are the lot colours of its model (LF_VehicleColoursConfig, at the
// respray price) and the liveries a pack lists for the model (LF_LiveryShopConfig). A livery can
// name a faction: a police or ambulance livery is only sold to that job. The client sends an
// option index; the server rebuilds the list and checks ownership, the bay, the job and the cash.

//------------------------------------------------------------------------------------------------
//! One livery a pack sells for one model.
[BaseContainerProps()]
class LF_LiveryShopRow
{
	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Vanilla catalog prefab of the model this livery fits", "et")]
	ResourceName m_Prefab;

	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Locked livery variant that spawns in this paint", "et")]
	ResourceName m_SpawnPrefab;

	[Attribute("", UIWidgets.EditBox, "Localization key of the livery's name")]
	string m_sNameKey;

	[Attribute("250", UIWidgets.Auto, "Price in cash", params: "0 1000000")]
	int m_iPrice;

	[Attribute("0", UIWidgets.ComboBox, "Faction that may buy it; CIVILIAN sells to anyone", enums: ParamEnumArray.FromEnum(LF_Faction))]
	LF_Faction m_eFaction;

	//------------------------------------------------------------------------------------------------
	static LF_LiveryShopRow Make(ResourceName prefab, ResourceName spawnPrefab, string nameKey, int price, LF_Faction faction)
	{
		LF_LiveryShopRow row = new LF_LiveryShopRow();
		row.m_Prefab = prefab;
		row.m_SpawnPrefab = spawnPrefab;
		row.m_sNameKey = nameKey;
		row.m_iPrice = price;
		row.m_eFaction = faction;
		return row;
	}
}

//------------------------------------------------------------------------------------------------
//! The livery shop config. There is no core file: every `.conf` of this class under a loaded
//! addon's `Configs/Vehicles/` is read (a pack adds its own file, no header line), rows merged in
//! path order. The respray price and bay radius are the last file's; 150 and 6 m with no file.
[BaseContainerProps(configRoot: true)]
class LF_LiveryShopConfig
{
	static const string FOLDER = "Configs/Vehicles/";

	[Attribute("150", UIWidgets.Auto, "Price of a respray in one of the model's lot colours", params: "0 1000000")]
	int m_iResprayPrice;

	[Attribute("6", UIWidgets.Auto, "How close to the bay origin the car must stand, in meters", params: "1 50")]
	float m_fBayRadius;

	[Attribute("", UIWidgets.Object, "Liveries for sale, by model")]
	ref array<ref LF_LiveryShopRow> m_aLiveries;

	protected static ref LF_LiveryShopConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	static LF_LiveryShopConfig Make(int resprayPrice)
	{
		LF_LiveryShopConfig config = new LF_LiveryShopConfig();
		config.m_iResprayPrice = resprayPrice;
		config.m_fBayRadius = 6; // the attribute default; `new` does not apply it
		config.m_aLiveries = {};
		return config;
	}

	//------------------------------------------------------------------------------------------------
	//! The merged config; an empty one (respray price 150, no liveries) when no file is found.
	static LF_LiveryShopConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		s_Config = Make(150); // the attribute default; `new` does not apply it
		LF_PackFolderCollector collector = new LF_PackFolderCollector(FOLDER);
		SearchResourcesFilter filter = new SearchResourcesFilter();
		filter.fileExtensions = { "conf" };
		filter.recursive = true;
		ResourceDatabase.SearchResources(filter, collector.OnFound);

		foreach (string entry : collector.GetSorted())
		{
			int bar = entry.IndexOf("|");
			ResourceName path = entry.Substring(bar + 1, entry.Length() - bar - 1); // lf-bounded: a resource path
			Resource resource = BaseContainerTools.LoadContainer(path);
			if (!resource || !resource.IsValid() || !resource.GetResource())
				continue;

			BaseContainer container = resource.GetResource().ToBaseContainer();
			if (!container || !container.GetClassName().ToType() || !container.GetClassName().ToType().IsInherited(LF_LiveryShopConfig))
				continue;

			LF_LiveryShopConfig layer = LF_LiveryShopConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container));
			if (!layer)
			{
				LF_Debug.Error("LiveryShop", string.Format("livery shop config does not load: %1", path));
				continue;
			}

			s_Config.m_iResprayPrice = layer.m_iResprayPrice;
			s_Config.m_fBayRadius = layer.m_fBayRadius;
			if (layer.m_aLiveries)
			{
				foreach (LF_LiveryShopRow row : layer.m_aLiveries)
				{
					if (row && !row.m_Prefab.IsEmpty() && !row.m_SpawnPrefab.IsEmpty())
						s_Config.m_aLiveries.Insert(row);
				}
			}

			LF_Debug.Info("LiveryShop", string.Format("livery shop: %1 adds %2 liveries", path, s_Config.m_aLiveries.Count()));
		}

		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	//! Test seam: makes `config` what Get() returns. Reload() undoes it.
	static void Use(LF_LiveryShopConfig config)
	{
		s_Config = config;
		s_bLoadAttempted = true;
	}

	//------------------------------------------------------------------------------------------------
	static void Reload()
	{
		s_Config = null;
		s_bLoadAttempted = false;
	}
}

//------------------------------------------------------------------------------------------------
//! One paint job on offer for one car.
class LF_LiveryOption : Managed
{
	ResourceName m_SpawnPrefab;
	string m_sNameKey;
	int m_iPrice;
	LF_Faction m_eFaction;
	bool m_bLivery;
}

//------------------------------------------------------------------------------------------------
//! The server half of the bay: the options and the paid respray.
class LF_LiveryShop
{
	//------------------------------------------------------------------------------------------------
	//! The paint jobs for an owned car, in the order the bay lists them: the model's lot colours
	//! (its current paint left out), then the liveries for the model. Empty for an unknown car.
	static void OptionsFor(string vehicleId, notnull array<ref LF_LiveryOption> options)
	{
		options.Clear();
		LF_VehicleRecord record = LF_VehicleOwnership.Export(vehicleId);
		if (!record)
			return;

		string current = record.m_sSpawnPrefab;
		if (current.IsEmpty())
			current = record.m_sPrefab;

		LF_LiveryShopConfig config = LF_LiveryShopConfig.Get();
		LF_VehicleColoursConfig colours = LF_VehicleColoursConfig.Get();
		LF_VehicleColourSet model;
		if (colours)
			model = colours.SetFor(record.m_sPrefab);

		if (model)
		{
			foreach (LF_VehicleColour colour : model.m_aColours)
			{
				if (!colour || colour.m_SpawnPrefab.IsEmpty() || LF_VehiclePriceModel.SameResource(colour.m_SpawnPrefab, current))
					continue;

				LF_LiveryOption option = new LF_LiveryOption();
				option.m_SpawnPrefab = colour.m_SpawnPrefab;
				option.m_sNameKey = colour.m_sNameKey;
				option.m_iPrice = config.m_iResprayPrice;
				option.m_eFaction = LF_Faction.CIVILIAN;
				options.Insert(option);
			}
		}

		foreach (LF_LiveryShopRow row : config.m_aLiveries)
		{
			if (!LF_VehiclePriceModel.SameResource(row.m_Prefab, record.m_sPrefab) || LF_VehiclePriceModel.SameResource(row.m_SpawnPrefab, current))
				continue;

			LF_LiveryOption livery = new LF_LiveryOption();
			livery.m_SpawnPrefab = row.m_SpawnPrefab;
			livery.m_sNameKey = row.m_sNameKey;
			livery.m_iPrice = row.m_iPrice;
			livery.m_eFaction = row.m_eFaction;
			livery.m_bLivery = true;
			options.Insert(livery);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: repaints the payer's car standing in the bay with option `index` of OptionsFor.
	//! Checks, in order: the car is the payer's, it stands in the bay, the option exists, the job
	//! allows it, the payer carries the price in cash. The cash is taken once, then the car goes
	//! through the garage and comes back on `padId` in the new paint; a failed respawn puts the old
	//! paint back and refunds. reasonKey is the line the payer reads either way.
	static bool Respray(IEntity payer, string payerUid, LF_Faction faction, string vehicleId, vector bayOrigin, string padId, int index, out int charged, out string reasonKey)
	{
		charged = 0;
		reasonKey = "#LF-Systems_Off";
		if (LF_Systems.Refuses("Vehicles", "respray"))
			return false;

		if (!Replication.IsServer())
		{
			LF_Debug.Error("LiveryShop", "respray refused: not running on the server");
			return false;
		}

		reasonKey = "#EL-Vehicle_NotOwner";
		LF_VehicleRecord record = LF_VehicleOwnership.Export(vehicleId);
		if (!record || payerUid.IsEmpty() || record.m_sOwnerUid != payerUid)
		{
			LF_Debug.Info("LiveryShop", string.Format("respray refused: %1 does not own %2", payerUid, vehicleId));
			return false;
		}

		reasonKey = "#LF-LiveryShop_NotInBay";
		IEntity car = LF_VehicleOwnership.GetVehicle(vehicleId);
		if (!car || record.m_bStored || vector.Distance(car.GetOrigin(), bayOrigin) > LF_LiveryShopConfig.Get().m_fBayRadius)
		{
			LF_Debug.Info("LiveryShop", string.Format("respray refused: %1 is not in the bay", vehicleId));
			return false;
		}

		reasonKey = "#LF-LiveryShop_NoOption";
		array<ref LF_LiveryOption> options = {};
		OptionsFor(vehicleId, options);
		if (index < 0 || index >= options.Count())
		{
			LF_Debug.Info("LiveryShop", string.Format("respray refused: %1 has no option %2 (of %3)", vehicleId, index, options.Count()));
			return false;
		}

		LF_LiveryOption option = options[index];
		reasonKey = "#LF-LiveryShop_WrongJob";
		if (option.m_eFaction != LF_Faction.CIVILIAN && option.m_eFaction != faction)
		{
			LF_Debug.Info("LiveryShop", string.Format("respray refused: %1 is not %2 for %3", payerUid, typename.EnumToString(LF_Faction, option.m_eFaction), option.m_SpawnPrefab));
			return false;
		}

		reasonKey = "#LF-LiveryShop_NoCash";
		if (!payer || LF_MoneyUtils.GetCash(payer) < option.m_iPrice)
		{
			LF_Debug.Info("LiveryShop", string.Format("respray refused: %1 cannot pay %2", payerUid, option.m_iPrice));
			return false;
		}

		Resource paint = Resource.Load(option.m_SpawnPrefab);
		reasonKey = "#EL-Vehicle_NoSpawn";
		if (!paint || !paint.IsValid())
		{
			LF_Debug.Error("LiveryShop", string.Format("respray refused: the paint prefab does not load: %1", option.m_SpawnPrefab));
			return false;
		}

		int removed = 0;
		if (option.m_iPrice > 0)
			removed = LF_MoneyUtils.RemoveCash(payer, option.m_iPrice, "respray");

		if (removed != option.m_iPrice)
		{
			if (removed > 0)
				LF_MoneyUtils.AddCash(payer, removed, "respray-rollback");

			LF_Debug.Warn("LiveryShop", string.Format("respray refused: cash could not be taken from %1", payerUid));
			reasonKey = "#LF-LiveryShop_NoCash";
			return false;
		}

		array<float> fuel = {};
		ReadFuel(car, fuel);
		string oldPaint = record.m_sSpawnPrefab;
		if (!Swap(vehicleId, payerUid, option.m_SpawnPrefab, padId, payer))
		{
			// The old paint comes back on the pad, or the car waits in the garage; the money goes back.
			Swap(vehicleId, payerUid, oldPaint, padId, payer);
			if (option.m_iPrice > 0)
				LF_MoneyUtils.AddCash(payer, option.m_iPrice, "respray-rollback");

			LF_Debug.Warn("LiveryShop", string.Format("respray of %1 failed; refunded %2", vehicleId, option.m_iPrice));
			return false;
		}

		WriteFuel(LF_VehicleOwnership.GetVehicle(vehicleId), fuel);
		charged = option.m_iPrice;
		reasonKey = "#LF-LiveryShop_Done";
		LF_Debug.Info("LiveryShop", string.Format("resprayed %1 owner=%2 %3 -> %4 for %5", vehicleId, payerUid, oldPaint, option.m_SpawnPrefab, charged));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Through the garage: store the car (its cargo captured), set the spawn prefab, retrieve on the pad.
	protected static bool Swap(string vehicleId, string ownerUid, ResourceName spawnPrefab, string padId, IEntity keyHolder)
	{
		LF_VehicleRecord record = LF_VehicleOwnership.Export(vehicleId);
		if (!record)
			return false;

		if (!record.m_bStored && !LF_VehicleOwnership.Store(vehicleId, ownerUid))
			return false;

		record = LF_VehicleOwnership.Export(vehicleId);
		record.m_sSpawnPrefab = spawnPrefab;
		if (!LF_VehicleOwnership.Apply(record))
			return false;

		return LF_VehicleOwnership.Retrieve(vehicleId, ownerUid, padId, keyHolder);
	}

	//------------------------------------------------------------------------------------------------
	//! Each fuel node's fill as a share of its tank, in node order.
	protected static void ReadFuel(IEntity car, notnull array<float> shares)
	{
		shares.Clear();
		SCR_FuelManagerComponent manager;
		if (car)
			manager = SCR_FuelManagerComponent.Cast(car.FindComponent(SCR_FuelManagerComponent));
		if (!manager)
			return;

		array<BaseFuelNode> nodes = {};
		manager.GetFuelNodesList(nodes);
		foreach (BaseFuelNode node : nodes)
		{
			float share = 0;
			if (node.GetMaxFuel() > 0)
				share = node.GetFuel() / node.GetMaxFuel();

			shares.Insert(share);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void WriteFuel(IEntity car, notnull array<float> shares)
	{
		SCR_FuelManagerComponent manager;
		if (car)
			manager = SCR_FuelManagerComponent.Cast(car.FindComponent(SCR_FuelManagerComponent));
		if (!manager || shares.IsEmpty())
			return;

		array<BaseFuelNode> nodes = {};
		manager.GetFuelNodesList(nodes);
		foreach (int i, BaseFuelNode node : nodes)
		{
			if (i < shares.Count())
				node.SetFuel(node.GetMaxFuel() * shares[i]);
		}
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_LiveryShopConfig's statics (LF_StaticReset finds it by reflection).
class LF_LiveryShopConfig_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_LiveryShopConfig";
	}

	override void Apply()
	{
		LF_LiveryShopConfig.Reload();
	}
}
