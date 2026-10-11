// LF_Garage.c — civilian garage catalog, store, and retrieve.
//
// The list and the actions follow the EveronLife garage and vehicle shop:
// a title, one row per vehicle (name, cash price, preview), previous, next,
// buy, store, retrieve, and exit. The screen is assembled with LF_UIPart and
// opened with LF_UIScreen.Open. It does not use MenuManager.
//
// The preview widget is VehicleImage in LF_GarageVehicleRow.layout. The slot
// names the vehicle prefab. The texture path stays empty until a capture, and
// the row still shows the name. Cash prices are the lot figures. MoneyStack
// and the ATM stay on the existing lot purchase. The lock stays
// EL_VehicleLockComponent.

class LF_GarageRecord : Managed
{
	string m_sVehicleId;
	string m_sPrefab;
	string m_sOwnerUid;
	string m_sKeyId;

	//------------------------------------------------------------------------------------------------
	static LF_GarageRecord Create(string vehicleId, string prefab, string ownerUid, string keyId)
	{
		LF_GarageRecord record();
		record.m_sVehicleId = vehicleId;
		record.m_sPrefab = prefab;
		record.m_sOwnerUid = ownerUid;
		record.m_sKeyId = keyId;
		return record;
	}
}

//------------------------------------------------------------------------------------------------
class LF_GarageCatalogRow : Managed
{
	string m_sId;
	string m_sNameKey;
	int m_iPrice;
	string m_sPrefab;
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
		AppendRow(rows, "s105", "#EL-Vehicle_S105", 1000, EL_VehicleCatalog.S105);
		AppendRow(rows, "uaz469", "#EL-Vehicle_UAZ469", 2500, EL_VehicleCatalog.UAZ469);
		AppendRow(rows, "s1203", "#EL-Vehicle_S1203", 4000, EL_VehicleCatalog.S1203);
		AppendRow(rows, "uaz452", "#EL-Vehicle_UAZ452", 3500, EL_VehicleCatalog.UAZ452);
		AppendRow(rows, "m923", "#EL-Vehicle_M923", 8000, EL_VehicleCatalog.M923);
		return rows;
	}

	//------------------------------------------------------------------------------------------------
	static void AppendRow(array<ref LF_GarageCatalogRow> rows, string id, string nameKey, int price, string prefab)
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
}
