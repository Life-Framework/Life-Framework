//------------------------------------------------------------------------------------------------
//! One owned vehicle. The stable id is the lock identifier and the key id.
//! A restart does not keep the entity. Apply restores this record, and the spawn
//! point places the prefab the way a character is placed at a spawn point.
//------------------------------------------------------------------------------------------------
class EL_VehicleRecord : Managed
{
	static const int VERSION = 1;

	string m_sVehicleId;
	string m_sOwnerUid;
	string m_sKeyId;
	string m_sPrefab;
	string m_sSpawnPrefab;
	string m_sShopId;
	string m_sPlate;
	string m_sNameKey;
	bool m_bLocked;
	bool m_bStored;

	//------------------------------------------------------------------------------------------------
	static EL_VehicleRecord Create(string vehicleId, string ownerUid, string keyId, string prefab, string spawnPrefab, string shopId, string plate, string nameKey, bool locked, bool stored)
	{
		EL_VehicleRecord record();
		record.m_sVehicleId = vehicleId;
		record.m_sOwnerUid = ownerUid;
		record.m_sKeyId = keyId;
		record.m_sPrefab = prefab;
		record.m_sSpawnPrefab = spawnPrefab;
		record.m_sShopId = shopId;
		record.m_sPlate = plate;
		record.m_sNameKey = nameKey;
		record.m_bLocked = locked;
		record.m_bStored = stored;
		return record;
	}

	//------------------------------------------------------------------------------------------------
	static EL_VehicleRecord Copy(EL_VehicleRecord source)
	{
		if (!source)
			return null;

		return Create(
			source.m_sVehicleId,
			source.m_sOwnerUid,
			source.m_sKeyId,
			source.m_sPrefab,
			source.m_sSpawnPrefab,
			source.m_sShopId,
			source.m_sPlate,
			source.m_sNameKey,
			source.m_bLocked,
			source.m_bStored
		);
	}
}

//------------------------------------------------------------------------------------------------
class EL_VehicleSession : Managed
{
	EntityID m_Vehicle;
	EntityID m_Key;

	//------------------------------------------------------------------------------------------------
	void EL_VehicleSession()
	{
		m_Vehicle = EntityID.INVALID;
		m_Key = EntityID.INVALID;
	}
}

//------------------------------------------------------------------------------------------------
//! Purchase, extra keys, garage store/retrieve, and the apply-save seam.
//! Small prices come out of MoneyStack. Prices above the lot's cash ceiling
//! debit the ATM balance. The lot does not turn cash into an account.
//------------------------------------------------------------------------------------------------
class EL_VehicleOwnership
{
	protected static ref map<string, ref EL_VehicleRecord> s_mRecords;
	protected static ref map<string, ref EL_VehicleSession> s_mSessions;
	protected static ref array<string> s_aOrder;

	//------------------------------------------------------------------------------------------------
	protected static map<string, ref EL_VehicleRecord> Records()
	{
		if (!s_mRecords)
			s_mRecords = new map<string, ref EL_VehicleRecord>();

		return s_mRecords;
	}

	//------------------------------------------------------------------------------------------------
	protected static map<string, ref EL_VehicleSession> Sessions()
	{
		if (!s_mSessions)
			s_mSessions = new map<string, ref EL_VehicleSession>();

		return s_mSessions;
	}

	//------------------------------------------------------------------------------------------------
	protected static array<string> Order()
	{
		if (!s_aOrder)
			s_aOrder = new array<string>();

		return s_aOrder;
	}

	//------------------------------------------------------------------------------------------------
	//! Cash at or below the ceiling. Above it, the bank balance. Never both.
	//! \return True when the price can be paid from the one source the rule allows.
	static bool QuotePurchase(int price, int cashCeiling, int cash, int bank, out int cashDebit, out int bankDebit)
	{
		cashDebit = 0;
		bankDebit = 0;

		if (price <= 0 || cashCeiling < 0)
		{
			EL_Debug.Error("Vehicle", string.Format("purchase refused: price %1 ceiling %2 is not configured", price, cashCeiling));
			return false;
		}

		if (price <= cashCeiling)
		{
			if (cash < price)
			{
				EL_Debug.Log("Vehicle", string.Format("purchase rejected: cash %1 below %2", cash, price));
				return false;
			}

			cashDebit = price;
			return true;
		}

		if (bank < price)
		{
			EL_Debug.Log("Vehicle", string.Format("purchase rejected: bank %1 below %2", bank, price));
			return false;
		}

		bankDebit = price;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static bool Register(string vehicleId, string ownerUid, string keyId, string prefab, string spawnPrefab, string shopId, string plate, string nameKey, bool locked)
	{
		if (vehicleId.IsEmpty() || ownerUid.IsEmpty() || keyId.IsEmpty() || prefab.IsEmpty())
		{
			EL_Debug.Error("Vehicle", "register refused: vehicle, owner, key, or prefab is empty");
			return false;
		}

		EL_VehicleRecord record = EL_VehicleRecord.Create(vehicleId, ownerUid, keyId, prefab, spawnPrefab, shopId, plate, nameKey, locked, false);
		Records().Set(vehicleId, record);
		if (!Order().Contains(vehicleId))
			Order().Insert(vehicleId);

		if (!plate.IsEmpty())
			EL_LicensePlateRegistry.Remember(plate, vehicleId);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	static EL_VehicleRecord Export(string vehicleId)
	{
		return EL_VehicleRecord.Copy(Records().Get(vehicleId));
	}

	//------------------------------------------------------------------------------------------------
	static array<ref EL_VehicleRecord> ExportAll()
	{
		array<ref EL_VehicleRecord> records = new array<ref EL_VehicleRecord>();
		foreach (string vehicleId : Order())
		{
			EL_VehicleRecord record = Records().Get(vehicleId);
			if (!record)
				continue;

			records.Insert(EL_VehicleRecord.Copy(record));
		}

		return records;
	}

	//------------------------------------------------------------------------------------------------
	//! Writes a saved record onto the live one. An id this session does not know is rejected.
	static bool Apply(EL_VehicleRecord record)
	{
		if (!record || record.m_sVehicleId.IsEmpty())
			return false;

		EL_VehicleRecord live = Records().Get(record.m_sVehicleId);
		if (!live)
		{
			EL_Debug.Log("Vehicle", string.Format("apply rejected: unknown vehicle %1", record.m_sVehicleId));
			return false;
		}

		CopyOnto(live, record);
		Rebind(live);
		EL_Debug.Log("Vehicle", string.Format("apply-save %1 owner=%2 key=%3", live.m_sVehicleId, live.m_sOwnerUid, live.m_sKeyId));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static void ApplyAll(array<ref EL_VehicleRecord> records)
	{
		ref map<string, ref EL_VehicleSession> previous = new map<string, ref EL_VehicleSession>();
		foreach (string sessionId, EL_VehicleSession session : Sessions())
		{
			if (session)
				previous.Set(sessionId, session);
		}

		Records().Clear();
		Order().Clear();
		Sessions().Clear();

		if (records)
		{
			foreach (EL_VehicleRecord record : records)
			{
				if (!record || record.m_sVehicleId.IsEmpty())
					continue;

				EL_VehicleRecord copy = EL_VehicleRecord.Copy(record);
				Records().Set(copy.m_sVehicleId, copy);
				Order().Insert(copy.m_sVehicleId);

				EL_VehicleSession session = previous.Get(copy.m_sVehicleId);
				if (session)
				{
					Sessions().Set(copy.m_sVehicleId, session);
					Rebind(copy);
				}
			}
		}

		foreach (string oldId, EL_VehicleSession oldSession : previous)
		{
			if (Records().Contains(oldId) || !oldSession)
				continue;

			IEntity vehicle = FindLive(oldSession.m_Vehicle);
			if (vehicle)
				SCR_EntityHelper.DeleteEntityAndChildren(vehicle);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns owned vehicles that are not in the garage and have no live entity.
	static void RestoreWorld()
	{
		foreach (string vehicleId : Order())
		{
			EL_VehicleRecord record = Records().Get(vehicleId);
			if (!record || record.m_bStored)
				continue;

			if (GetVehicle(vehicleId))
			{
				Rebind(record);
				continue;
			}

			vector origin;
			if (!EL_VehicleSpawnPointComponent.FindFreeOrigin(record.m_sShopId, origin))
				continue;

			SpawnOwned(record, origin);
		}
	}

	//------------------------------------------------------------------------------------------------
	static void NoteLockState(string vehicleId, bool locked)
	{
		if (vehicleId.IsEmpty())
			return;

		EL_VehicleRecord record = Records().Get(vehicleId);
		if (!record)
			return;

		record.m_bLocked = locked;
		string stateName = "unlocked";
		if (locked)
			stateName = "locked";

		EL_Debug.Log("Vehicle", string.Format("lock recorded %1 %2", vehicleId, stateName));
	}

	//------------------------------------------------------------------------------------------------
	static string GetOwner(string vehicleId)
	{
		EL_VehicleRecord record = Records().Get(vehicleId);
		if (!record)
			return "";

		return record.m_sOwnerUid;
	}

	//------------------------------------------------------------------------------------------------
	static string GetKeyId(string vehicleId)
	{
		EL_VehicleRecord record = Records().Get(vehicleId);
		if (!record)
			return "";

		return record.m_sKeyId;
	}

	//------------------------------------------------------------------------------------------------
	static string GetPrefab(string vehicleId)
	{
		EL_VehicleRecord record = Records().Get(vehicleId);
		if (!record)
			return "";

		return record.m_sPrefab;
	}

	//------------------------------------------------------------------------------------------------
	static string GetPlate(string vehicleId)
	{
		EL_VehicleRecord record = Records().Get(vehicleId);
		if (!record)
			return "";

		return record.m_sPlate;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsLocked(string vehicleId)
	{
		EL_VehicleRecord record = Records().Get(vehicleId);
		if (!record)
			return false;

		return record.m_bLocked;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsStored(string vehicleId)
	{
		EL_VehicleRecord record = Records().Get(vehicleId);
		if (!record)
			return false;

		return record.m_bStored;
	}

	//------------------------------------------------------------------------------------------------
	static IEntity GetVehicle(string vehicleId)
	{
		EL_VehicleSession session = Sessions().Get(vehicleId);
		if (!session)
			return null;

		return FindLive(session.m_Vehicle);
	}

	//------------------------------------------------------------------------------------------------
	static IEntity GetKey(string vehicleId)
	{
		EL_VehicleSession session = Sessions().Get(vehicleId);
		if (!session)
			return null;

		return FindLive(session.m_Key);
	}

	//------------------------------------------------------------------------------------------------
	static bool CanIssueExtraKey(string vehicleId, string requesterUid)
	{
		EL_VehicleRecord record = Records().Get(vehicleId);
		if (!record || requesterUid.IsEmpty() || record.m_sOwnerUid != requesterUid)
		{
			EL_Debug.Log("Vehicle", "extra key rejected: not the owner");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	static string LatestInWorld(string ownerUid)
	{
		return Latest(ownerUid, false);
	}

	//------------------------------------------------------------------------------------------------
	static string LatestStored(string ownerUid)
	{
		return Latest(ownerUid, true);
	}

	//------------------------------------------------------------------------------------------------
	static array<ref EL_VehicleRecord> StoredFor(string ownerUid)
	{
		array<ref EL_VehicleRecord> found = new array<ref EL_VehicleRecord>();
		foreach (string vehicleId : Order())
		{
			EL_VehicleRecord record = Records().Get(vehicleId);
			if (!record || !record.m_bStored)
				continue;

			if (record.m_sOwnerUid != ownerUid)
				continue;

			found.Insert(EL_VehicleRecord.Copy(record));
		}

		return found;
	}

	//------------------------------------------------------------------------------------------------
	static bool TryUnlock(string vehicleId, IEntity key)
	{
		IEntity vehicle = GetVehicle(vehicleId);
		if (!vehicle || !key)
			return false;

		EL_VehicleLockComponent vehicleLock = EL_Component<EL_VehicleLockComponent>.Find(vehicle);
		if (!vehicleLock)
			return false;

		if (!vehicleLock.IsValidKey(key))
		{
			EL_Debug.Log("Vehicle", "unlock rejected: key mismatch");
			return false;
		}

		vehicleLock.SetLocked(false);
		NoteLockState(vehicleId, false);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static bool Buy(string buyerUid, string catalogPrefab, string spawnPrefab, int price, int cashCeiling, string shopId, string nameKey, vector origin, IEntity buyer, out string vehicleId, out int cashCharged, out string reasonKey)
	{
		vehicleId = "";
		cashCharged = 0;
		reasonKey = "#EL-Vehicle_NotSold";

		if (!Replication.IsServer())
		{
			EL_Debug.Error("Vehicle", "purchase refused: not running on the server");
			return false;
		}

		if (buyerUid.IsEmpty() || catalogPrefab.IsEmpty() || shopId.IsEmpty())
		{
			EL_Debug.Error("Vehicle", "purchase refused: buyer, prefab, or shop is empty");
			return false;
		}

		string spawnName = spawnPrefab;
		if (spawnName.IsEmpty())
			spawnName = catalogPrefab;

		Resource resource = Resource.Load(spawnName);
		if (!resource || !resource.IsValid())
		{
			EL_Debug.Error("Vehicle", string.Format("purchase refused: prefab does not resolve: %1", spawnName));
			return false;
		}

		int cash = CashOn(buyer);
		int bank = 0;
		EL_BankAccount account = EL_ATMManager.GetOrCreate(buyerUid);
		if (account)
			bank = account.GetBalance();

		int cashDebit = 0;
		int bankDebit = 0;
		if (!QuotePurchase(price, cashCeiling, cash, bank, cashDebit, bankDebit))
		{
			reasonKey = "#EL-Vehicle_CannotAfford";
			return false;
		}

		if (!TakePayment(buyer, account, cashDebit, bankDebit))
		{
			reasonKey = "#EL-Vehicle_CannotAfford";
			return false;
		}

		IEntity vehicle = EL_Utils.SpawnEntityPrefab(spawnName, origin);
		if (!vehicle)
		{
			Refund(buyer, account, cashDebit, bankDebit);
			reasonKey = "#EL-Vehicle_NoSpawn";
			return false;
		}

		EL_VehicleLockComponent vehicleLock = EL_Component<EL_VehicleLockComponent>.Find(vehicle);
		if (!vehicleLock)
		{
			EL_Debug.Error("Vehicle", string.Format("purchase refused: %1 has no EL_VehicleLockComponent", spawnName));
			SCR_EntityHelper.DeleteEntityAndChildren(vehicle);
			Refund(buyer, account, cashDebit, bankDebit);
			reasonKey = "#EL-Vehicle_NotSold";
			return false;
		}

		string id = vehicleLock.GetVehicleIdentifier();
		if (id.IsEmpty())
		{
			id = PersistenceIdUtils.Generate();
			vehicleLock.SetVehicleIdentifier(id);
		}

		if (id.IsEmpty())
		{
			EL_Debug.Error("Vehicle", "purchase refused: the lock did not assign an identifier");
			SCR_EntityHelper.DeleteEntityAndChildren(vehicle);
			Refund(buyer, account, cashDebit, bankDebit);
			reasonKey = "#EL-Vehicle_NoSpawn";
			return false;
		}
		IEntity key = SpawnKey(id, origin);
		if (!key)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(vehicle);
			Refund(buyer, account, cashDebit, bankDebit);
			reasonKey = "#EL-Vehicle_NoSpawn";
			return false;
		}

		GiveKey(buyer, key);
		vehicleLock.SetLocked(true);

		string plate = EL_LicensePlateRegistry.Issue(new EL_LicensePlateGeneratorGeneric(), id);
		EL_LicensePlateManagerComponent plates = EL_Component<EL_LicensePlateManagerComponent>.Find(vehicle);
		if (plates && !plate.IsEmpty())
			plates.SetRegistration(plate);

		if (!Register(id, buyerUid, id, catalogPrefab, spawnName, shopId, plate, nameKey, true))
		{
			SCR_EntityHelper.DeleteEntityAndChildren(vehicle);
			SCR_EntityHelper.DeleteEntityAndChildren(key);
			Refund(buyer, account, cashDebit, bankDebit);
			reasonKey = "#EL-Vehicle_NotSold";
			return false;
		}

		EL_VehicleSession session = new EL_VehicleSession();
		session.m_Vehicle = vehicle.GetID();
		session.m_Key = key.GetID();
		Sessions().Set(id, session);

		vehicleId = id;
		cashCharged = cashDebit;
		reasonKey = "#EL-Vehicle_Bought";
		EL_Debug.Log("Vehicle", string.Format("purchased %1 owner=%2 id=%3 plate=%4 cash=%5 bank=%6", catalogPrefab, buyerUid, id, plate, cashDebit, bankDebit));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static bool GiveExtraKey(string vehicleId, string ownerUid, int price, int cashCeiling, IEntity buyer, vector origin, out int cashCharged)
	{
		cashCharged = 0;
		if (!CanIssueExtraKey(vehicleId, ownerUid))
			return false;

		int cash = CashOn(buyer);
		int bank = 0;
		EL_BankAccount account = EL_ATMManager.GetOrCreate(ownerUid);
		if (account)
			bank = account.GetBalance();

		int cashDebit = 0;
		int bankDebit = 0;
		if (!QuotePurchase(price, cashCeiling, cash, bank, cashDebit, bankDebit))
			return false;

		if (!TakePayment(buyer, account, cashDebit, bankDebit))
			return false;

		EL_VehicleRecord record = Records().Get(vehicleId);
		IEntity key = SpawnKey(record.m_sKeyId, origin);
		if (!key)
		{
			Refund(buyer, account, cashDebit, bankDebit);
			return false;
		}

		GiveKey(buyer, key);
		cashCharged = cashDebit;
		EL_Debug.Log("Vehicle", string.Format("extra key %1 owner=%2", vehicleId, ownerUid));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static bool Store(string vehicleId, string ownerUid)
	{
		EL_VehicleRecord record = Records().Get(vehicleId);
		if (!Owned(record, ownerUid))
		{
			EL_Debug.Log("Vehicle", "store rejected: not the owner");
			return false;
		}

		if (record.m_bStored)
		{
			EL_Debug.Log("Vehicle", "store rejected: already stored");
			return false;
		}

		EL_VehicleSession session = Sessions().Get(vehicleId);
		if (session)
		{
			IEntity vehicle = FindLive(session.m_Vehicle);
			if (vehicle)
			{
				EL_VehicleLockComponent vehicleLock = EL_Component<EL_VehicleLockComponent>.Find(vehicle);
				if (vehicleLock)
					record.m_bLocked = vehicleLock.IsVehicleLocked();

				SCR_EntityHelper.DeleteEntityAndChildren(vehicle);
			}

			session.m_Vehicle = EntityID.INVALID;
		}

		record.m_bStored = true;
		string stateName = "unlocked";
		if (record.m_bLocked)
			stateName = "locked";

		EL_Debug.Log("Vehicle", string.Format("stored %1 owner=%2 %3", vehicleId, ownerUid, stateName));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Record half of retrieve. Does not spawn. The world retrieve calls this after the spawn.
	static bool RestoreRecord(string vehicleId, string ownerUid)
	{
		EL_VehicleRecord record = Records().Get(vehicleId);
		if (!Owned(record, ownerUid))
		{
			EL_Debug.Log("Vehicle", "retrieve rejected: not the owner");
			return false;
		}

		if (!record.m_bStored)
		{
			EL_Debug.Log("Vehicle", "retrieve rejected: vehicle is not stored");
			return false;
		}

		record.m_bStored = false;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static bool Retrieve(string vehicleId, string ownerUid)
	{
		EL_VehicleRecord record = Records().Get(vehicleId);
		if (!Owned(record, ownerUid))
		{
			EL_Debug.Log("Vehicle", "retrieve rejected: not the owner");
			return false;
		}

		if (!record.m_bStored)
		{
			EL_Debug.Log("Vehicle", "retrieve rejected: vehicle is not stored");
			return false;
		}

		vector origin;
		if (!EL_VehicleSpawnPointComponent.FindFreeOrigin(record.m_sShopId, origin))
			return false;

		if (!SpawnOwned(record, origin))
			return false;

		record.m_bStored = false;
		EL_Debug.Log("Vehicle", string.Format("retrieved %1 owner=%2 prefab=%3", vehicleId, ownerUid, record.m_sPrefab));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool Owned(EL_VehicleRecord record, string ownerUid)
	{
		if (!record || ownerUid.IsEmpty())
			return false;

		return record.m_sOwnerUid == ownerUid;
	}

	//------------------------------------------------------------------------------------------------
	protected static string Latest(string ownerUid, bool stored)
	{
		for (int i = Order().Count() - 1; i >= 0; i--)
		{
			string vehicleId = Order().Get(i);
			EL_VehicleRecord record = Records().Get(vehicleId);
			if (!record || record.m_sOwnerUid != ownerUid)
				continue;

			if (record.m_bStored != stored)
				continue;

			return vehicleId;
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	protected static void CopyOnto(EL_VehicleRecord live, EL_VehicleRecord record)
	{
		live.m_sOwnerUid = record.m_sOwnerUid;
		live.m_sKeyId = record.m_sKeyId;
		live.m_sPrefab = record.m_sPrefab;
		live.m_sSpawnPrefab = record.m_sSpawnPrefab;
		live.m_sShopId = record.m_sShopId;
		live.m_sPlate = record.m_sPlate;
		live.m_sNameKey = record.m_sNameKey;
		live.m_bLocked = record.m_bLocked;
		live.m_bStored = record.m_bStored;
	}

	//------------------------------------------------------------------------------------------------
	protected static int CashOn(IEntity buyer)
	{
		if (!buyer)
			return 0;

		int held = EL_MoneyUtils.GetCash(buyer);
		if (held > 0)
			return held;

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool TakePayment(IEntity buyer, EL_BankAccount account, int cashDebit, int bankDebit)
	{
		if (bankDebit > 0)
		{
			if (!account || !account.Withdraw(bankDebit))
			{
				EL_Debug.Log("Vehicle", "purchase rejected: bank withdraw failed");
				return false;
			}
		}

		if (cashDebit > 0)
		{
			if (!buyer)
			{
				Refund(null, account, 0, bankDebit);
				EL_Debug.Log("Vehicle", "purchase rejected: cash payment has no character");
				return false;
			}

			int removed = EL_MoneyUtils.RemoveCash(buyer, cashDebit);
			if (removed != cashDebit)
			{
				if (removed > 0)
					EL_MoneyUtils.AddCash(buyer, removed);

				Refund(null, account, 0, bankDebit);
				EL_Debug.Log("Vehicle", "purchase rejected: cash could not be taken");
				return false;
			}
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Refund(IEntity buyer, EL_BankAccount account, int cashDebit, int bankDebit)
	{
		if (bankDebit > 0 && account)
			account.Deposit(bankDebit);

		if (cashDebit > 0 && buyer)
			EL_MoneyUtils.AddCash(buyer, cashDebit);
	}

	//------------------------------------------------------------------------------------------------
	protected static void GiveKey(IEntity buyer, IEntity key)
	{
		if (!buyer || !key)
			return;

		SCR_InventoryStorageManagerComponent inventory = EL_Component<SCR_InventoryStorageManagerComponent>.Find(buyer);
		if (!inventory)
			return;

		inventory.TryInsertItem(key);
	}

	//------------------------------------------------------------------------------------------------
	protected static IEntity SpawnKey(string vehicleId, vector origin)
	{
		IEntity key = EL_Utils.SpawnEntityPrefab(EL_VehicleCatalog.KEY, origin + Vector(1, 0.2, 0));
		if (!key)
		{
			EL_Debug.Error("Vehicle", "key spawn failed");
			return null;
		}

		EL_VehicleKeyComponent keyComponent = EL_Component<EL_VehicleKeyComponent>.Find(key);
		if (!keyComponent)
		{
			EL_Debug.Error("Vehicle", "key prefab has no EL_VehicleKeyComponent");
			SCR_EntityHelper.DeleteEntityAndChildren(key);
			return null;
		}

		keyComponent.BindToVehicleIdentifier(vehicleId);
		return key;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool SpawnOwned(EL_VehicleRecord record, vector origin)
	{
		string spawnName = record.m_sSpawnPrefab;
		if (spawnName.IsEmpty())
			spawnName = record.m_sPrefab;

		IEntity vehicle = EL_Utils.SpawnEntityPrefab(spawnName, origin);
		if (!vehicle)
			return false;

		EL_VehicleLockComponent vehicleLock = EL_Component<EL_VehicleLockComponent>.Find(vehicle);
		if (!vehicleLock)
		{
			EL_Debug.Error("Vehicle", string.Format("restore refused: %1 has no EL_VehicleLockComponent", spawnName));
			SCR_EntityHelper.DeleteEntityAndChildren(vehicle);
			return false;
		}

		vehicleLock.SetVehicleIdentifier(record.m_sKeyId);
		vehicleLock.SetLocked(record.m_bLocked);

		EL_VehicleSession session = Sessions().Get(record.m_sVehicleId);
		if (!session)
		{
			session = new EL_VehicleSession();
			Sessions().Set(record.m_sVehicleId, session);
		}

		IEntity key = FindLive(session.m_Key);
		if (!key)
		{
			key = SpawnKey(record.m_sKeyId, origin);
			if (!key)
			{
				SCR_EntityHelper.DeleteEntityAndChildren(vehicle);
				return false;
			}

			session.m_Key = key.GetID();
		}
		else
		{
			EL_VehicleKeyComponent keyComponent = EL_Component<EL_VehicleKeyComponent>.Find(key);
			if (keyComponent)
				keyComponent.BindToVehicleIdentifier(record.m_sKeyId);
		}

		session.m_Vehicle = vehicle.GetID();

		if (!record.m_sPlate.IsEmpty())
		{
			EL_LicensePlateManagerComponent plates = EL_Component<EL_LicensePlateManagerComponent>.Find(vehicle);
			if (plates)
				plates.SetRegistration(record.m_sPlate);
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Rebind(EL_VehicleRecord record)
	{
		if (!record)
			return;

		EL_VehicleSession session = Sessions().Get(record.m_sVehicleId);
		if (!session)
			return;

		IEntity vehicle = FindLive(session.m_Vehicle);
		if (record.m_bStored)
		{
			if (vehicle)
				SCR_EntityHelper.DeleteEntityAndChildren(vehicle);

			session.m_Vehicle = EntityID.INVALID;
		}
		else if (vehicle)
		{
			EL_VehicleLockComponent vehicleLock = EL_Component<EL_VehicleLockComponent>.Find(vehicle);
			if (vehicleLock)
			{
				vehicleLock.SetVehicleIdentifier(record.m_sKeyId);
				vehicleLock.SetLocked(record.m_bLocked);
			}
		}

		IEntity key = FindLive(session.m_Key);
		if (!key)
			return;

		EL_VehicleKeyComponent keyComponent = EL_Component<EL_VehicleKeyComponent>.Find(key);
		if (keyComponent)
			keyComponent.BindToVehicleIdentifier(record.m_sKeyId);
	}

	//------------------------------------------------------------------------------------------------
	protected static IEntity FindLive(EntityID id)
	{
		if (id == EntityID.INVALID)
			return null;

		if (!GetGame() || !GetGame().GetWorld())
			return null;

		return GetGame().GetWorld().FindEntityByID(id);
	}
}
