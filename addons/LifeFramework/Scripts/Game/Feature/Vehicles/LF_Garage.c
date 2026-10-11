// LF_Garage.c — civilian garage catalog, store, and retrieve.
//
// The list and the actions follow the EveronLife garage and vehicle shop:
// a title, one row per vehicle (name, cash price, preview), previous, next,
// buy, store, retrieve, and exit. The screen is assembled with LF_UIPart and
// opened with LF_UIScreen.Open. It does not use MenuManager.
//
// The preview widget is VehicleImage in LF_GarageVehicleRow.layout. The slot
// names the vehicle prefab. The texture path stays empty until a capture, and
// the row still shows the name. A buy quotes the lot cash ceiling: MoneyStack
// at or below it, the ATM balance above it, never both. Paint is stored on the
// record and applied with EL_Utils. Park refuses a seated player, then stops
// the engine and ejects before the world record is removed. The lock stays
// EL_VehicleLockComponent. The spawn prefab is the existing locked child.

class LF_GarageRecord : Managed
{
	string m_sVehicleId;
	string m_sPrefab;
	string m_sOwnerUid;
	string m_sKeyId;
	int m_iRed;
	int m_iGreen;
	int m_iBlue;

	//------------------------------------------------------------------------------------------------
	static LF_GarageRecord Create(string vehicleId, string prefab, string ownerUid, string keyId)
	{
		LF_GarageRecord record();
		record.m_sVehicleId = vehicleId;
		record.m_sPrefab = prefab;
		record.m_sOwnerUid = ownerUid;
		record.m_sKeyId = keyId;
		record.m_iRed = 255;
		record.m_iGreen = 255;
		record.m_iBlue = 255;
		return record;
	}

	//------------------------------------------------------------------------------------------------
	void SetPaint(int red, int green, int blue)
	{
		m_iRed = LF_Garage.Channel(red);
		m_iGreen = LF_Garage.Channel(green);
		m_iBlue = LF_Garage.Channel(blue);
	}
}

//------------------------------------------------------------------------------------------------
class LF_GarageCatalogRow : Managed
{
	string m_sId;
	string m_sNameKey;
	int m_iPrice;
	string m_sPrefab;
	string m_sSpawnPrefab;
	string m_sTexture;
	string m_sPreviewLayout;

	//------------------------------------------------------------------------------------------------
	string PriceText()
	{
		return string.Format("%1", m_iPrice);
	}
}

//------------------------------------------------------------------------------------------------
class LF_Garage
{
	static const string PREVIEW_LAYOUT = "{280751AD1F984C8E}UI/Layouts/Garage/LF_GarageVehicleRow.layout";
	static const int CASH_CEILING = 500;
	static const string CAMERA_PREFAB = "Prefabs/Editor/Camera/ManualCameraBase.et";

	protected ref map<string, ref LF_GarageRecord> m_mRecords;
	protected ref map<string, ref array<string>> m_mStored;
	protected ref array<string> m_aWorld;

	//------------------------------------------------------------------------------------------------
	void LF_Garage()
	{
		Ensure();
	}

	//------------------------------------------------------------------------------------------------
	protected void Ensure()
	{
		if (!m_mRecords)
			m_mRecords = new map<string, ref LF_GarageRecord>();

		if (!m_mStored)
			m_mStored = new map<string, ref array<string>>();

		if (!m_aWorld)
			m_aWorld = {};
	}

	//------------------------------------------------------------------------------------------------
	//! Civilian catalog in scope order. A row with no prefab is logged and skipped.
	static array<ref LF_GarageCatalogRow> Catalog()
	{
		ref array<ref LF_GarageCatalogRow> rows = {};
		AppendRow(rows, "s105", "#EL-Vehicle_S105", 1000, EL_VehicleCatalog.S105, EL_VehicleCatalog.S105_LOCKED);
		AppendRow(rows, "uaz469", "#EL-Vehicle_UAZ469", 2500, EL_VehicleCatalog.UAZ469, EL_VehicleCatalog.UAZ469_LOCKED);
		AppendRow(rows, "s1203", "#EL-Vehicle_S1203", 4000, EL_VehicleCatalog.S1203, EL_VehicleCatalog.S1203_LOCKED);
		AppendRow(rows, "uaz452", "#EL-Vehicle_UAZ452", 3500, EL_VehicleCatalog.UAZ452, EL_VehicleCatalog.UAZ452_LOCKED);
		AppendRow(rows, "m923", "#EL-Vehicle_M923", 8000, EL_VehicleCatalog.M923, EL_VehicleCatalog.M923_LOCKED);
		return rows;
	}

	//------------------------------------------------------------------------------------------------
	static void AppendRow(array<ref LF_GarageCatalogRow> rows, string id, string nameKey, int price, string prefab, string spawnPrefab)
	{
		if (!rows)
			return;

		if (prefab.IsEmpty())
		{
			EL_Debug.Error("Garage", "catalog row skipped: missing prefab");
			return;
		}

		LF_GarageCatalogRow row();
		row.m_sId = id;
		row.m_sNameKey = nameKey;
		row.m_iPrice = price;
		row.m_sPrefab = prefab;
		row.m_sSpawnPrefab = spawnPrefab;
		if (row.m_sSpawnPrefab.IsEmpty())
			row.m_sSpawnPrefab = prefab;

		row.m_sTexture = string.Empty;
		row.m_sPreviewLayout = PREVIEW_LAYOUT;
		rows.Insert(row);
	}

	//------------------------------------------------------------------------------------------------
	bool Place(LF_GarageRecord record)
	{
		Ensure();

		if (!record || record.m_sPrefab.IsEmpty() || record.m_sVehicleId.IsEmpty())
		{
			EL_Debug.Error("Garage", "row skipped: missing prefab");
			return false;
		}

		if (m_mRecords.Get(record.m_sVehicleId))
		{
			EL_Debug.Log("Garage", "place refused: already recorded");
			return false;
		}

		m_mRecords.Insert(record.m_sVehicleId, record);
		m_aWorld.Insert(record.m_sVehicleId);
		EL_Debug.Log("Garage", "in the world " + record.m_sVehicleId);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	bool IsInWorld(string vehicleId)
	{
		Ensure();
		return HasId(m_aWorld, vehicleId);
	}

	//------------------------------------------------------------------------------------------------
	bool IsStored(string vehicleId, string ownerUid)
	{
		Ensure();
		return HasId(m_mStored.Get(ownerUid), vehicleId);
	}

	//------------------------------------------------------------------------------------------------
	//! Takes the vehicle out of the world record and keeps it for the owner.
	bool Store(string vehicleId, string ownerUid)
	{
		Ensure();

		LF_GarageRecord record = m_mRecords.Get(vehicleId);
		if (!Owned(record, ownerUid))
		{
			EL_Debug.Log("Garage", "store refused: not the owner");
			return false;
		}

		if (!HasId(m_aWorld, vehicleId))
		{
			EL_Debug.Log("Garage", "store refused: not in the world");
			return false;
		}

		DropId(m_aWorld, vehicleId);

		ref array<string> stored = m_mStored.Get(ownerUid);
		if (!stored)
		{
			stored = {};
			m_mStored.Insert(ownerUid, stored);
		}

		stored.Insert(vehicleId);
		EL_Debug.Log("Garage", "stored " + vehicleId + " owner=" + ownerUid);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Gives the same prefab and key back to the owner and returns it to the world.
	bool Retrieve(string vehicleId, string ownerUid, out string prefab, out string keyId)
	{
		Ensure();
		prefab = string.Empty;
		keyId = string.Empty;

		LF_GarageRecord record = m_mRecords.Get(vehicleId);
		if (!Owned(record, ownerUid))
		{
			EL_Debug.Log("Garage", "retrieve refused: not the owner");
			return false;
		}

		ref array<string> stored = m_mStored.Get(ownerUid);
		if (!HasId(stored, vehicleId))
		{
			EL_Debug.Log("Garage", "retrieve refused: not stored");
			return false;
		}

		DropId(stored, vehicleId);
		m_aWorld.Insert(vehicleId);
		prefab = record.m_sPrefab;
		keyId = record.m_sKeyId;
		EL_Debug.Log("Garage", "retrieved " + vehicleId + " owner=" + ownerUid);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected bool Owned(LF_GarageRecord record, string ownerUid)
	{
		if (!record || ownerUid.IsEmpty())
			return false;

		return record.m_sOwnerUid == ownerUid;
	}

	//------------------------------------------------------------------------------------------------
	protected bool HasId(array<string> ids, string id)
	{
		if (!ids || id.IsEmpty())
			return false;

		for (int i = 0; i < ids.Count(); i++)
		{
			if (ids.Get(i) == id)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected void DropId(array<string> ids, string id)
	{
		if (!ids)
			return;

		ids.RemoveItem(id);
	}

	//------------------------------------------------------------------------------------------------
	static int Channel(int value)
	{
		if (value < 0)
			return 0;

		if (value > 255)
			return 255;

		return value;
	}

	//------------------------------------------------------------------------------------------------
	//! Lot rule: cash at or below the ceiling, the ATM balance above it.
	static bool Quote(int price, int cash, int bank, out int cashDebit, out int bankDebit)
	{
		return EL_VehicleOwnership.QuotePurchase(price, CASH_CEILING, cash, bank, cashDebit, bankDebit);
	}

	//------------------------------------------------------------------------------------------------
	bool AcceptPurchase(int index, string vehicleId, string ownerUid, string keyId, int cash, int bank, int red, int green, int blue, out int cashDebit, out int bankDebit)
	{
		cashDebit = 0;
		bankDebit = 0;

		array<ref LF_GarageCatalogRow> rows = Catalog();
		if (index < 0 || index >= rows.Count())
		{
			EL_Debug.Error("Garage", "purchase refused: no such row");
			return false;
		}

		LF_GarageCatalogRow row = rows.Get(index);
		if (!row || !Quote(row.m_iPrice, cash, bank, cashDebit, bankDebit))
			return false;

		LF_GarageRecord record = LF_GarageRecord.Create(vehicleId, row.m_sPrefab, ownerUid, keyId);
		record.SetPaint(red, green, blue);
		return Place(record);
	}

	//------------------------------------------------------------------------------------------------
	bool GetPaint(string vehicleId, out int red, out int green, out int blue)
	{
		Ensure();
		red = 0;
		green = 0;
		blue = 0;

		LF_GarageRecord record = m_mRecords.Get(vehicleId);
		if (!record)
			return false;

		red = record.m_iRed;
		green = record.m_iGreen;
		blue = record.m_iBlue;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static bool ParkAllowed(bool isOwner, bool inVehicle, bool garageNearby)
	{
		if (!isOwner)
			return false;

		if (inVehicle)
			return false;

		if (!garageNearby)
			return false;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	static bool CanPark(bool isOwner, bool inVehicle, bool garageNearby)
	{
		if (!isOwner)
		{
			EL_Debug.Log("Garage", "park refused: not the owner");
			return false;
		}

		if (inVehicle)
		{
			EL_Debug.Log("Garage", "park refused: still in the vehicle");
			return false;
		}

		if (!garageNearby)
		{
			EL_Debug.Log("Garage", "park refused: no garage nearby");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	bool Park(string vehicleId, string ownerUid, bool inVehicle, bool garageNearby)
	{
		if (!CanPark(true, inVehicle, garageNearby))
			return false;

		return Store(vehicleId, ownerUid);
	}

	//------------------------------------------------------------------------------------------------
	static bool PadFree(int nearbyVehicles)
	{
		if (nearbyVehicles > 0)
		{
			EL_Debug.Log("Garage", "spawn refused: pad occupied");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	static bool PreviewReady(string buildingName, string prefab)
	{
		if (buildingName.IsEmpty() || prefab.IsEmpty())
		{
			EL_Debug.Error("Garage", "preview skipped: building or prefab is missing");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	static bool CameraReady(string cameraPrefab)
	{
		if (cameraPrefab.IsEmpty())
		{
			EL_Debug.Error("Garage", "camera skipped: no camera prefab");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns the catalog prefab at a named preview building. The texture stays empty.
	static bool ShowPreview(string buildingName, string prefab)
	{
		if (!PreviewReady(buildingName, prefab))
			return false;

		if (!GetGame() || !GetGame().GetWorld())
		{
			EL_Debug.Error("Garage", "preview skipped: no world");
			return false;
		}

		IEntity building = GetGame().GetWorld().FindEntityByName(buildingName);
		if (!building)
		{
			EL_Debug.Error("Garage", "preview skipped: building not found");
			return false;
		}

		IEntity preview = EL_Utils.SpawnEntityPrefab(prefab, building.GetOrigin(), "0 0 0");
		if (!preview)
		{
			EL_Debug.Error("Garage", "preview skipped: vehicle did not spawn");
			return false;
		}

		EL_Debug.Log("Garage", "preview spawned " + prefab);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static void ApplyPaint(IEntity vehicle, int red, int green, int blue)
	{
		if (!vehicle)
		{
			EL_Debug.Error("Garage", "paint skipped: no vehicle");
			return;
		}

		Color color = Color.FromSRGBA(Channel(red), Channel(green), Channel(blue), 255);
		EL_Utils.ChangeColorRecursive(vehicle, color);
		EL_Utils.SetSlotsColor(vehicle, color.PackToInt());
	}

	//------------------------------------------------------------------------------------------------
	static void ClearCargo(IEntity vehicle)
	{
		if (!vehicle)
		{
			EL_Debug.Error("Garage", "cargo clear skipped: no vehicle");
			return;
		}

		SCR_VehicleInventoryStorageManagerComponent storage = SCR_VehicleInventoryStorageManagerComponent.Cast(vehicle.FindComponent(SCR_VehicleInventoryStorageManagerComponent));
		if (!storage)
		{
			EL_Debug.Error("Garage", "cargo clear skipped: no storage");
			return;
		}

		array<IEntity> items = {};
		storage.GetItems(items);
		foreach (IEntity item : items)
		{
			if (!item)
				continue;

			storage.TryDeleteItem(item);
		}
	}

	//------------------------------------------------------------------------------------------------
	static void StopAndEject(IEntity vehicle)
	{
		if (!vehicle)
		{
			EL_Debug.Error("Garage", "park skipped: no vehicle");
			return;
		}

		VehicleControllerComponent controller = VehicleControllerComponent.Cast(vehicle.FindComponent(VehicleControllerComponent));
		if (controller)
			controller.StopEngine();

		SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
		if (!compartments)
		{
			EL_Debug.Error("Garage", "park skipped: no compartments");
			return;
		}

		array<IEntity> occupants = {};
		compartments.GetOccupants(occupants);
		foreach (IEntity occupant : occupants)
		{
			ChimeraCharacter character = ChimeraCharacter.Cast(occupant);
			if (!character)
				continue;

			CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
			if (!access)
				continue;

			access.EjectOutOfVehicle();
		}
	}

	//------------------------------------------------------------------------------------------------
	static void ReadStats(string prefab, out float engine, out float handling, out float braking, out float storage)
	{
		engine = 0;
		handling = 0;
		braking = 0;
		storage = 0;

		if (prefab.IsEmpty())
		{
			EL_Debug.Error("Garage", "stats skipped: missing prefab");
			return;
		}

		Resource resource = Resource.Load(prefab);
		if (!resource || !resource.IsValid())
		{
			EL_Debug.Error("Garage", "stats skipped: prefab did not load");
			return;
		}

		BaseContainer inventory = SCR_BaseContainerTools.FindComponentSource(resource, "SCR_UniversalInventoryStorageComponent");
		if (inventory)
			inventory.Get("m_fMaxWeight", storage);

		BaseContainer wheeled = SCR_BaseContainerTools.FindComponentSource(resource, "VehicleWheeledSimulation");
		if (!wheeled)
		{
			EL_Debug.Error("Garage", "stats skipped: no wheeled simulation");
			return;
		}

		BaseContainer simulation = wheeled.GetObject("Simulation");
		if (!simulation)
		{
			EL_Debug.Error("Garage", "stats skipped: no simulation");
			return;
		}

		BaseContainer engineSource = simulation.GetObject("Engine");
		if (engineSource)
		{
			engineSource.Get("MaxPower", engine);
			engineSource.Get("Friction", handling);
		}

		BaseContainerList axles = simulation.GetObjectArray("Axles");
		if (!axles)
			return;

		BaseContainer axle = axles.Get(0);
		if (!axle)
			return;

		BaseContainer tyre = axle.GetObject("Tyre");
		if (!tyre)
			return;

		tyre.Get("LongitudinalFriction", braking);
	}

	//------------------------------------------------------------------------------------------------
	static string StatLine(float value, string key)
	{
		if (value == 0)
			return key;

		return string.Format("%1", value);
	}
}

//------------------------------------------------------------------------------------------------
class LF_GarageScreen
{
	protected int m_iIndex;

	//------------------------------------------------------------------------------------------------
	static array<ref LF_UIPart> GetParts()
	{
		ref array<ref LF_UIPart> parts = {};
		array<ref LF_GarageCatalogRow> rows = LF_Garage.Catalog();

		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.TITLE, "VehicleTitle", "#LF-Garage_Title"));

		foreach (LF_GarageCatalogRow row : rows)
		{
			if (!row)
				continue;

			// The preview texture is empty. The name is still on the row.
			parts.Insert(LF_UIPart.CreateStatus(row.m_sId, row.m_sNameKey, row.PriceText(), "#EL-Vehicle_Buy"));
		}

		string priceLine = "#LF-Garage_Price";
		if (rows.Count() > 0)
			priceLine = rows.Get(0).PriceText();

		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "VehiclePriceText", priceLine));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "Engine", "#LF-Garage_Engine"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "Handling", "#LF-Garage_Handling"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "Braking", "#LF-Garage_Braking"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "StorageSizeText", "#LF-Garage_Storage"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.FIELD, "Red", "#LF-Garage_Red"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.FIELD, "Green", "#LF-Garage_Green"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.FIELD, "Blue", "#LF-Garage_Blue"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.BUTTON, "Left", "#LF-Garage_Previous"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.BUTTON, "Right", "#LF-Garage_Next"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.BUTTON, "Buy", "#EL-Vehicle_Buy"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.BUTTON, "Store", "#EL-Vehicle_Store"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.BUTTON, "Retrieve", "#EL-Vehicle_Retrieve"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.CLOSE, "Exit", "#LF-Garage_Exit"));
		return parts;
	}

	//------------------------------------------------------------------------------------------------
	static LF_UIScreen Open()
	{
		return LF_UIScreen.Open(GetParts());
	}

	//------------------------------------------------------------------------------------------------
	//! Shop previous/next. The index stays put at either end of the catalog.
	void Select(int offset)
	{
		array<ref LF_GarageCatalogRow> rows = LF_Garage.Catalog();
		int count = rows.Count();
		if (count == 0)
			return;

		int next = m_iIndex + offset;
		if (next < 0 || next >= count)
			return;

		m_iIndex = next;
	}

	//------------------------------------------------------------------------------------------------
	int GetIndex()
	{
		return m_iIndex;
	}

	//------------------------------------------------------------------------------------------------
	LF_GarageCatalogRow Current()
	{
		array<ref LF_GarageCatalogRow> rows = LF_Garage.Catalog();
		if (m_iIndex < 0 || m_iIndex >= rows.Count())
			return null;

		return rows.Get(m_iIndex);
	}

	//------------------------------------------------------------------------------------------------
	string PriceLine()
	{
		LF_GarageCatalogRow row = Current();
		if (!row)
			return "#LF-Garage_Price";

		return row.PriceText();
	}

	//------------------------------------------------------------------------------------------------
	static string BuyLabel(int price, int cash, int bank)
	{
		int cashDebit;
		int bankDebit;
		if (LF_Garage.Quote(price, cash, bank, cashDebit, bankDebit))
			return "#EL-Vehicle_Buy";

		return "#EL-Vehicle_CannotAfford";
	}
}
