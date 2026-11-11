//------------------------------------------------------------------------------------------------
//! One owned vehicle. The stable id is the lock identifier and the key id.
//! A restart does not keep the entity. Apply restores this record, and the spawn
//! point places the prefab the way a character is placed at a spawn point.
//------------------------------------------------------------------------------------------------
class LF_VehicleRecord : Managed
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
	static LF_VehicleRecord Create(string vehicleId, string ownerUid, string keyId, string prefab, string spawnPrefab, string shopId, string plate, string nameKey, bool locked, bool stored)
	{
		LF_VehicleRecord record();
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
	static LF_VehicleRecord Copy(LF_VehicleRecord source)
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
class LF_VehicleSession : Managed
{
	EntityID m_Vehicle;
	EntityID m_Key;

	//------------------------------------------------------------------------------------------------
	void LF_VehicleSession()
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
class LF_VehicleOwnership
{
	protected static ref map<string, ref LF_VehicleRecord> s_mRecords;
	protected static ref map<string, ref LF_VehicleSession> s_mSessions;
	protected static ref array<string> s_aOrder;

	//------------------------------------------------------------------------------------------------
	protected static map<string, ref LF_VehicleRecord> Records()
	{
		if (!s_mRecords)
			s_mRecords = new map<string, ref LF_VehicleRecord>();

		return s_mRecords;
	}

	//------------------------------------------------------------------------------------------------
	protected static map<string, ref LF_VehicleSession> Sessions()
	{
		if (!s_mSessions)
			s_mSessions = new map<string, ref LF_VehicleSession>();

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
			LF_Debug.Error("Vehicle", string.Format("purchase refused: price %1 ceiling %2 is not configured", price, cashCeiling));
			return false;
		}

		if (price <= cashCeiling)
		{
			if (cash < price)
			{
				LF_Debug.Log("Vehicle", string.Format("purchase rejected: cash %1 below %2", cash, price));
				return false;
			}

			cashDebit = price;
			return true;
		}

		if (bank < price)
		{
			LF_Debug.Log("Vehicle", string.Format("purchase rejected: bank %1 below %2", bank, price));
			return false;
		}

		bankDebit = price;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Why QuotePurchase refused, as the key the buyer reads. Above the cash ceiling the price
	//! comes from the bank only, so a buyer holding enough cash is told to use the ATM.
	static string RefusalKey(int price, int cashCeiling)
	{
		if (price > cashCeiling)
			return "#LF-Vehicle_BankShort";

		return "#EL-Vehicle_CannotAfford";
	}

	//------------------------------------------------------------------------------------------------
	static bool Register(string vehicleId, string ownerUid, string keyId, string prefab, string spawnPrefab, string shopId, string plate, string nameKey, bool locked)
	{
		if (vehicleId.IsEmpty() || ownerUid.IsEmpty() || keyId.IsEmpty() || prefab.IsEmpty())
		{
			LF_Debug.Error("Vehicle", "register refused: vehicle, owner, key, or prefab is empty");
			return false;
		}

		LF_VehicleRecord record = LF_VehicleRecord.Create(vehicleId, ownerUid, keyId, prefab, spawnPrefab, shopId, plate, nameKey, locked, false);
		Records().Set(vehicleId, record);
		if (!Order().Contains(vehicleId))
			Order().Insert(vehicleId);

		if (!plate.IsEmpty())
			LF_LicensePlateRegistry.Remember(plate, vehicleId);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	static LF_VehicleRecord Export(string vehicleId)
	{
		return LF_VehicleRecord.Copy(Records().Get(vehicleId));
	}

	//------------------------------------------------------------------------------------------------
	static array<ref LF_VehicleRecord> ExportAll()
	{
		array<ref LF_VehicleRecord> records = new array<ref LF_VehicleRecord>();
		foreach (string vehicleId : Order())
		{
			LF_VehicleRecord record = Records().Get(vehicleId);
			if (!record)
				continue;

			records.Insert(LF_VehicleRecord.Copy(record));
		}

		return records;
	}

	//------------------------------------------------------------------------------------------------
	//! Writes a saved record onto the live one. An id this session does not know is rejected.
	static bool Apply(LF_VehicleRecord record)
	{
		if (!record || record.m_sVehicleId.IsEmpty())
			return false;

		LF_VehicleRecord live = Records().Get(record.m_sVehicleId);
		if (!live)
		{
			LF_Debug.Log("Vehicle", string.Format("apply rejected: unknown vehicle %1", record.m_sVehicleId));
			return false;
		}

		CopyOnto(live, record);
		Rebind(live);
		LF_Debug.Log("Vehicle", string.Format("apply-save %1 owner=%2 key=%3", live.m_sVehicleId, live.m_sOwnerUid, live.m_sKeyId));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static void ApplyAll(array<ref LF_VehicleRecord> records)
	{
		ref map<string, ref LF_VehicleSession> previous = new map<string, ref LF_VehicleSession>();
		foreach (string sessionId, LF_VehicleSession session : Sessions())
		{
			if (session)
				previous.Set(sessionId, session);
		}

		Records().Clear();
		Order().Clear();
		Sessions().Clear();

		if (records)
		{
			foreach (LF_VehicleRecord record : records)
			{
				if (!record || record.m_sVehicleId.IsEmpty())
					continue;

				LF_VehicleRecord copy = LF_VehicleRecord.Copy(record);
				Records().Set(copy.m_sVehicleId, copy);
				Order().Insert(copy.m_sVehicleId);

				// The plate registry is session memory. Re-reserve every saved plate so a
				// purchase after a restart cannot be issued a plate that is already on the road.
				if (!copy.m_sPlate.IsEmpty())
					LF_LicensePlateRegistry.Remember(copy.m_sPlate, copy.m_sVehicleId);

				LF_VehicleSession session = previous.Get(copy.m_sVehicleId);
				if (session)
				{
					Sessions().Set(copy.m_sVehicleId, session);
					Rebind(copy);
				}
			}
		}

		foreach (string oldId, LF_VehicleSession oldSession : previous)
		{
			if (Records().Contains(oldId) || !oldSession)
				continue;

			IEntity vehicle = FindLive(oldSession.m_Vehicle);
			if (vehicle)
				SCR_EntityHelper.DeleteEntityAndChildren(vehicle);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Garage it to keep it. After a load, an owned vehicle that was not stored has no entity:
	//! it is at the impound lot and is not spawned. A vehicle that still has a live entity (a
	//! re-apply to a running session) stays where it is. Idempotent.
	//! \return The number of vehicles at the impound lot afterwards.
	static int ImpoundStreetVehicles()
	{
		int impounded = 0;
		foreach (string vehicleId : Order())
		{
			LF_VehicleRecord record = Records().Get(vehicleId);
			if (!record || record.m_bStored)
				continue;

			if (GetVehicle(vehicleId))
			{
				Rebind(record);
				continue;
			}

			impounded++;
			LF_Debug.Info("Vehicle", string.Format("impounded %1 owner=%2 plate=%3: left on the street at restart", vehicleId, record.m_sOwnerUid, record.m_sPlate));
		}

		return impounded;
	}

	//------------------------------------------------------------------------------------------------
	//! At the impound lot: owned, not stored, and not in the world. The state is derived, so it
	//! needs no save field: every restart puts each unstored vehicle here, and a collect or a
	//! retrieve puts it back in the world.
	static bool IsImpounded(string vehicleId)
	{
		LF_VehicleRecord record = Records().Get(vehicleId);
		if (!record || record.m_bStored)
			return false;

		return GetVehicle(vehicleId) == null;
	}

	//------------------------------------------------------------------------------------------------
	static string LatestImpounded(string ownerUid)
	{
		for (int i = Order().Count() - 1; i >= 0; i--)
		{
			string vehicleId = Order().Get(i);
			LF_VehicleRecord record = Records().Get(vehicleId);
			if (!record || record.m_sOwnerUid != ownerUid)
				continue;

			if (IsImpounded(vehicleId))
				return vehicleId;
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! The owner's vehicles out in the world: not stored, with a live entity.
	static array<ref LF_VehicleRecord> InWorldFor(string ownerUid)
	{
		array<ref LF_VehicleRecord> found = new array<ref LF_VehicleRecord>();
		foreach (string vehicleId : Order())
		{
			LF_VehicleRecord record = Records().Get(vehicleId);
			if (!record || record.m_bStored || record.m_sOwnerUid != ownerUid)
				continue;

			if (GetVehicle(vehicleId))
				found.Insert(LF_VehicleRecord.Copy(record));
		}

		return found;
	}

	//------------------------------------------------------------------------------------------------
	static array<ref LF_VehicleRecord> ImpoundedFor(string ownerUid)
	{
		array<ref LF_VehicleRecord> found = new array<ref LF_VehicleRecord>();
		foreach (string vehicleId : Order())
		{
			LF_VehicleRecord record = Records().Get(vehicleId);
			if (!record || record.m_sOwnerUid != ownerUid)
				continue;

			if (IsImpounded(vehicleId))
				found.Insert(LF_VehicleRecord.Copy(record));
		}

		return found;
	}

	//------------------------------------------------------------------------------------------------
	//! The owner's live, unstored vehicle within radius of a point (the garage the owner stands at).
	//! \return The vehicle id, or empty when none of the owner's vehicles is there.
	static string OwnedVehicleNear(string ownerUid, vector point, float radius)
	{
		for (int i = Order().Count() - 1; i >= 0; i--)
		{
			string vehicleId = Order().Get(i);
			LF_VehicleRecord record = Records().Get(vehicleId);
			if (!record || record.m_bStored || record.m_sOwnerUid != ownerUid)
				continue;

			IEntity vehicle = GetVehicle(vehicleId);
			if (vehicle && vector.Distance(vehicle.GetOrigin(), point) <= radius)
				return vehicleId;
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Pays the impound fee from carried cash and puts the vehicle on the impound pad. The key is
	//! handed to the payer when the owner no longer holds a live one.
	//! \param[out] reasonKey What the owner reads, success or refusal.
	static bool CollectFromImpound(string vehicleId, string ownerUid, IEntity payer, int fee, string padId, out string reasonKey)
	{
		reasonKey = "#LF-Impound_NothingHere";
		if (!Replication.IsServer())
		{
			LF_Debug.Error("Vehicle", "impound collect refused: not running on the server");
			return false;
		}

		LF_VehicleRecord record = Records().Get(vehicleId);
		if (!Owned(record, ownerUid))
		{
			LF_Debug.Info("Vehicle", string.Format("impound collect refused: %1 does not own %2", ownerUid, vehicleId));
			return false;
		}

		if (!IsImpounded(vehicleId))
		{
			LF_Debug.Info("Vehicle", string.Format("impound collect refused: %1 is not at the impound lot", vehicleId));
			return false;
		}

		if (fee < 0)
		{
			LF_Debug.Error("Vehicle", string.Format("impound collect refused: fee %1 is not configured", fee));
			reasonKey = "#LF-Impound_Closed";
			return false;
		}

		if (fee > 0 && CashOn(payer) < fee)
		{
			LF_Debug.Info("Vehicle", string.Format("impound collect refused: %1 carries %2 of the %3 fee", ownerUid, CashOn(payer), fee));
			reasonKey = "#LF-Impound_CannotAfford";
			return false;
		}

		vector origin;
		vector angles;
		if (!LF_VehicleSpawnPointComponent.FindFreePad(padId, origin, angles))
		{
			reasonKey = "#LF-Impound_PadBusy";
			return false;
		}

		if (fee > 0 && !TakePayment(payer, null, fee, 0))
		{
			reasonKey = "#LF-Impound_CannotAfford";
			return false;
		}

		if (!SpawnOwned(record, origin, angles, payer))
		{
			Refund(payer, null, fee, 0);
			reasonKey = "#LF-Impound_PadBusy";
			return false;
		}

		reasonKey = "#LF-Impound_Collected";
		LF_Debug.Info("Vehicle", string.Format("impound collected %1 owner=%2 fee=%3 plate=%4", vehicleId, ownerUid, fee, record.m_sPlate));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static void NoteLockState(string vehicleId, bool locked)
	{
		if (vehicleId.IsEmpty())
			return;

		LF_VehicleRecord record = Records().Get(vehicleId);
		if (!record)
			return;

		record.m_bLocked = locked;
		string stateName = "unlocked";
		if (locked)
			stateName = "locked";

		LF_Debug.Log("Vehicle", string.Format("lock recorded %1 %2", vehicleId, stateName));
	}

	//------------------------------------------------------------------------------------------------
	static string GetOwner(string vehicleId)
	{
		LF_VehicleRecord record = Records().Get(vehicleId);
		if (!record)
			return "";

		return record.m_sOwnerUid;
	}

	//------------------------------------------------------------------------------------------------
	static string GetNameKey(string vehicleId)
	{
		LF_VehicleRecord record = Records().Get(vehicleId);
		if (!record)
			return "";

		return record.m_sNameKey;
	}

	//------------------------------------------------------------------------------------------------
	static string GetKeyId(string vehicleId)
	{
		LF_VehicleRecord record = Records().Get(vehicleId);
		if (!record)
			return "";

		return record.m_sKeyId;
	}

	//------------------------------------------------------------------------------------------------
	static string GetPrefab(string vehicleId)
	{
		LF_VehicleRecord record = Records().Get(vehicleId);
		if (!record)
			return "";

		return record.m_sPrefab;
	}

	//------------------------------------------------------------------------------------------------
	static string GetPlate(string vehicleId)
	{
		LF_VehicleRecord record = Records().Get(vehicleId);
		if (!record)
			return "";

		return record.m_sPlate;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsLocked(string vehicleId)
	{
		LF_VehicleRecord record = Records().Get(vehicleId);
		if (!record)
			return false;

		return record.m_bLocked;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsStored(string vehicleId)
	{
		LF_VehicleRecord record = Records().Get(vehicleId);
		if (!record)
			return false;

		return record.m_bStored;
	}

	//------------------------------------------------------------------------------------------------
	static IEntity GetVehicle(string vehicleId)
	{
		LF_VehicleSession session = Sessions().Get(vehicleId);
		if (!session)
			return null;

		return FindLive(session.m_Vehicle);
	}

	//------------------------------------------------------------------------------------------------
	static IEntity GetKey(string vehicleId)
	{
		LF_VehicleSession session = Sessions().Get(vehicleId);
		if (!session)
			return null;

		return FindLive(session.m_Key);
	}

	//------------------------------------------------------------------------------------------------
	static bool CanIssueExtraKey(string vehicleId, string requesterUid)
	{
		LF_VehicleRecord record = Records().Get(vehicleId);
		if (!record || requesterUid.IsEmpty() || record.m_sOwnerUid != requesterUid)
		{
			LF_Debug.Log("Vehicle", "extra key rejected: not the owner");
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
	static array<ref LF_VehicleRecord> StoredFor(string ownerUid)
	{
		array<ref LF_VehicleRecord> found = new array<ref LF_VehicleRecord>();
		foreach (string vehicleId : Order())
		{
			LF_VehicleRecord record = Records().Get(vehicleId);
			if (!record || !record.m_bStored)
				continue;

			if (record.m_sOwnerUid != ownerUid)
				continue;

			found.Insert(LF_VehicleRecord.Copy(record));
		}

		return found;
	}

	//------------------------------------------------------------------------------------------------
	static bool TryUnlock(string vehicleId, IEntity key)
	{
		IEntity vehicle = GetVehicle(vehicleId);
		if (!vehicle || !key)
			return false;

		LF_VehicleLockComponent vehicleLock = LF_Component<LF_VehicleLockComponent>.Find(vehicle);
		if (!vehicleLock)
			return false;

		if (!vehicleLock.IsValidKey(key))
		{
			LF_Debug.Log("Vehicle", "unlock rejected: key mismatch");
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
			LF_Debug.Error("Vehicle", "purchase refused: not running on the server");
			return false;
		}

		if (buyerUid.IsEmpty() || catalogPrefab.IsEmpty() || shopId.IsEmpty())
		{
			LF_Debug.Error("Vehicle", "purchase refused: buyer, prefab, or shop is empty");
			return false;
		}

		string spawnName = spawnPrefab;
		if (spawnName.IsEmpty())
			spawnName = catalogPrefab;

		Resource resource = Resource.Load(spawnName);
		if (!resource || !resource.IsValid())
		{
			LF_Debug.Error("Vehicle", string.Format("purchase refused: prefab does not resolve: %1", spawnName));
			return false;
		}

		int cash = CashOn(buyer);
		int bank = 0;
		LF_BankAccount account = LF_ATMManager.GetOrCreate(buyerUid);
		if (account)
			bank = account.GetBalance();

		int cashDebit = 0;
		int bankDebit = 0;
		if (!QuotePurchase(price, cashCeiling, cash, bank, cashDebit, bankDebit))
		{
			reasonKey = RefusalKey(price, cashCeiling);
			return false;
		}

		if (!TakePayment(buyer, account, cashDebit, bankDebit))
		{
			reasonKey = "#EL-Vehicle_CannotAfford";
			return false;
		}

		IEntity vehicle = LF_Utils.SpawnEntityPrefab(spawnName, origin);
		if (!vehicle)
		{
			Refund(buyer, account, cashDebit, bankDebit);
			reasonKey = "#EL-Vehicle_NoSpawn";
			return false;
		}

		LF_VehicleLockComponent vehicleLock = LF_Component<LF_VehicleLockComponent>.Find(vehicle);
		if (!vehicleLock)
		{
			LF_Debug.Error("Vehicle", string.Format("purchase refused: %1 has no LF_VehicleLockComponent", spawnName));
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
			LF_Debug.Error("Vehicle", "purchase refused: the lock did not assign an identifier");
			SCR_EntityHelper.DeleteEntityAndChildren(vehicle);
			Refund(buyer, account, cashDebit, bankDebit);
			reasonKey = "#EL-Vehicle_NoSpawn";
			return false;
		}
		IEntity key = SpawnKey(id, KeySpawnOrigin(buyer, origin));
		if (!key)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(vehicle);
			Refund(buyer, account, cashDebit, bankDebit);
			reasonKey = "#EL-Vehicle_NoSpawn";
			return false;
		}

		GiveKey(buyer, key);
		vehicleLock.SetLocked(true);

		string plate = LF_LicensePlateRegistry.Issue(new LF_LicensePlateGeneratorGeneric(), id);
		LF_LicensePlateManagerComponent plates = LF_Component<LF_LicensePlateManagerComponent>.Find(vehicle);
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

		LF_VehicleSession session = new LF_VehicleSession();
		session.m_Vehicle = vehicle.GetID();
		session.m_Key = key.GetID();
		Sessions().Set(id, session);

		vehicleId = id;
		cashCharged = cashDebit;
		reasonKey = "#EL-Vehicle_Bought";
		LF_Debug.Log("Vehicle", string.Format("purchased %1 owner=%2 id=%3 plate=%4 cash=%5 bank=%6", catalogPrefab, buyerUid, id, plate, cashDebit, bankDebit));
		LF_EventBus.Publish(LF_OnVehicleBought.Create(buyerUid, catalogPrefab, cashDebit + bankDebit));
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
		LF_BankAccount account = LF_ATMManager.GetOrCreate(ownerUid);
		if (account)
			bank = account.GetBalance();

		int cashDebit = 0;
		int bankDebit = 0;
		if (!QuotePurchase(price, cashCeiling, cash, bank, cashDebit, bankDebit))
			return false;

		if (!TakePayment(buyer, account, cashDebit, bankDebit))
			return false;

		LF_VehicleRecord record = Records().Get(vehicleId);
		IEntity key = SpawnKey(record.m_sKeyId, KeySpawnOrigin(buyer, origin));
		if (!key)
		{
			Refund(buyer, account, cashDebit, bankDebit);
			return false;
		}

		GiveKey(buyer, key);
		cashCharged = cashDebit;
		LF_Debug.Log("Vehicle", string.Format("extra key %1 owner=%2", vehicleId, ownerUid));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static bool Store(string vehicleId, string ownerUid)
	{
		LF_VehicleRecord record = Records().Get(vehicleId);
		if (!Owned(record, ownerUid))
		{
			LF_Debug.Log("Vehicle", "store rejected: not the owner");
			return false;
		}

		if (record.m_bStored)
		{
			LF_Debug.Log("Vehicle", "store rejected: already stored");
			return false;
		}

		LF_VehicleSession session = Sessions().Get(vehicleId);
		if (session)
		{
			IEntity vehicle = FindLive(session.m_Vehicle);
			if (vehicle)
			{
				LF_VehicleLockComponent vehicleLock = LF_Component<LF_VehicleLockComponent>.Find(vehicle);
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

		LF_Debug.Log("Vehicle", string.Format("stored %1 owner=%2 %3", vehicleId, ownerUid, stateName));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The vehicle is gone for good (a chop shop took it apart): drop its record, so no restart
	//! impounds it and no garage lists it, and free its plate. The key item in a pocket stays a
	//! key to nothing. The caller deletes the entity.
	//! \return True when a record was removed.
	static bool Scrap(string vehicleId)
	{
		LF_VehicleRecord record = Records().Get(vehicleId);
		if (!record)
			return false;

		LF_LicensePlateRegistry.Release(record.m_sPlate);
		Records().Remove(vehicleId);
		Order().RemoveItem(vehicleId);
		Sessions().Remove(vehicleId);
		LF_Debug.Info("Vehicle", string.Format("scrapped %1 owner=%2 plate=%3", vehicleId, record.m_sOwnerUid, record.m_sPlate));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Record half of retrieve. Does not spawn. The world retrieve calls this after the spawn.
	static bool RestoreRecord(string vehicleId, string ownerUid)
	{
		LF_VehicleRecord record = Records().Get(vehicleId);
		if (!Owned(record, ownerUid))
		{
			LF_Debug.Log("Vehicle", "retrieve rejected: not the owner");
			return false;
		}

		if (!record.m_bStored)
		{
			LF_Debug.Log("Vehicle", "retrieve rejected: vehicle is not stored");
			return false;
		}

		record.m_bStored = false;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Puts a stored vehicle on the pads of the garage the owner retrieves at.
	//! \param padId The garage's pad id (LF_VehicleGarageComponent.GetPadId).
	//! \param keyHolder Receives a fresh key when the owner holds no live one; null drops it on the pad.
	static bool Retrieve(string vehicleId, string ownerUid, string padId, IEntity keyHolder = null)
	{
		LF_VehicleRecord record = Records().Get(vehicleId);
		if (!Owned(record, ownerUid))
		{
			LF_Debug.Log("Vehicle", "retrieve rejected: not the owner");
			return false;
		}

		if (!record.m_bStored)
		{
			LF_Debug.Log("Vehicle", "retrieve rejected: vehicle is not stored");
			return false;
		}

		vector origin;
		vector angles;
		if (!LF_VehicleSpawnPointComponent.FindFreePad(padId, origin, angles))
			return false;

		if (!SpawnOwned(record, origin, angles, keyHolder))
			return false;

		record.m_bStored = false;
		LF_Debug.Info("Vehicle", string.Format("retrieved %1 owner=%2 prefab=%3 pad=%4", vehicleId, ownerUid, record.m_sPrefab, padId));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool Owned(LF_VehicleRecord record, string ownerUid)
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
			LF_VehicleRecord record = Records().Get(vehicleId);
			if (!record || record.m_sOwnerUid != ownerUid)
				continue;

			if (record.m_bStored != stored)
				continue;

			return vehicleId;
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	protected static void CopyOnto(LF_VehicleRecord live, LF_VehicleRecord record)
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

		int held = LF_MoneyUtils.GetCash(buyer);
		if (held > 0)
			return held;

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool TakePayment(IEntity buyer, LF_BankAccount account, int cashDebit, int bankDebit)
	{
		if (bankDebit > 0)
		{
			if (!account || !account.Withdraw(bankDebit, "vehicle-purchase"))
			{
				LF_Debug.Log("Vehicle", "purchase rejected: bank withdraw failed");
				return false;
			}
		}

		if (cashDebit > 0)
		{
			if (!buyer)
			{
				Refund(null, account, 0, bankDebit);
				LF_Debug.Log("Vehicle", "purchase rejected: cash payment has no character");
				return false;
			}

			int removed = LF_MoneyUtils.RemoveCash(buyer, cashDebit, "vehicle-purchase");
			if (removed != cashDebit)
			{
				if (removed > 0)
					LF_MoneyUtils.AddCash(buyer, removed, "vehicle-purchase-rollback");

				Refund(null, account, 0, bankDebit);
				LF_Debug.Log("Vehicle", "purchase rejected: cash could not be taken");
				return false;
			}
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Refund(IEntity buyer, LF_BankAccount account, int cashDebit, int bankDebit)
	{
		if (bankDebit > 0 && account)
			account.Deposit(bankDebit, "vehicle-purchase-rollback");

		if (cashDebit > 0 && buyer)
			LF_MoneyUtils.AddCash(buyer, cashDebit, "vehicle-purchase-rollback");
	}

	//------------------------------------------------------------------------------------------------
	//! Playtest grant (LF_SpawnLogic.m_bGrantTestCars): the owner gets two cars from the lot's first
	//! offers as records, with their keys in hand: the first stored in the garage, the second at the
	//! impound lot (owned, not stored, no entity: IsImpounded). Nothing spawns in the world. Only for
	//! an owner with no vehicle yet, so a respawn or a second call grants nothing.
	//! \return How many cars were granted.
	static int GrantTestCars(string ownerUid, IEntity holder, notnull array<LF_VehicleOffer> offers, string shopId)
	{
		if (ownerUid.IsEmpty() || !holder)
		{
			LF_Debug.Error("Vehicle", "test cars not granted: no owner or no body for the keys");
			return 0;
		}

		foreach (string knownId : Order())
		{
			LF_VehicleRecord known = Records().Get(knownId);
			if (known && known.m_sOwnerUid == ownerUid)
				return 0;
		}

		int granted = 0;
		for (int i = 0; i < offers.Count() && granted < 2; i++)
		{
			LF_VehicleOffer offer = offers[i];
			if (!offer)
				continue;

			string id = PersistenceIdUtils.Generate();
			IEntity key = SpawnKey(id, holder.GetOrigin());
			if (!key)
				continue;

			string plate = LF_LicensePlateRegistry.Issue(new LF_LicensePlateGeneratorGeneric(), id);
			if (!Register(id, ownerUid, id, offer.GetPrefab(), offer.GetSpawnPrefab(), shopId, plate, offer.GetNameKey(), true))
			{
				SCR_EntityHelper.DeleteEntityAndChildren(key);
				continue;
			}

			GiveKey(holder, key);
			string place = "impounded";
			if (granted == 0)
			{
				Records().Get(id).m_bStored = true;
				place = "stored in the garage";
			}

			granted++;
			LF_Debug.Info("Vehicle", string.Format("test car %1 plate=%2 for %3: %4", offer.GetNameKey(), plate, ownerUid, place));
		}

		return granted;
	}

	//------------------------------------------------------------------------------------------------
	protected static void GiveKey(IEntity buyer, IEntity key)
	{
		if (!buyer || !key)
			return;

		SCR_InventoryStorageManagerComponent inventory = LF_Component<SCR_InventoryStorageManagerComponent>.Find(buyer);
		if (!inventory)
			return;

		if (!inventory.TryInsertItem(key))
			LF_Debug.Warn("Vehicle", "key did not fit the holder's inventory; it stays at the holder's feet");
	}

	//------------------------------------------------------------------------------------------------
	//! Where a key is created: at its holder, so the item exists where the holder's client streams
	//! it before the inventory insert. A key created at a far pad and inserted from there left the
	//! client unable to resolve it and hung the inventory's loading throbber.
	static vector KeySpawnOrigin(IEntity holder, vector fallback)
	{
		if (holder)
			return holder.GetOrigin();

		return fallback;
	}

	//------------------------------------------------------------------------------------------------
	protected static IEntity SpawnKey(string vehicleId, vector origin)
	{
		IEntity key = LF_Utils.SpawnEntityPrefab(LF_VehicleCatalog.KEY, origin + Vector(1, 0.2, 0));
		if (!key)
		{
			LF_Debug.Error("Vehicle", "key spawn failed");
			return null;
		}

		LF_VehicleKeyComponent keyComponent = LF_Component<LF_VehicleKeyComponent>.Find(key);
		if (!keyComponent)
		{
			LF_Debug.Error("Vehicle", "key prefab has no LF_VehicleKeyComponent");
			SCR_EntityHelper.DeleteEntityAndChildren(key);
			return null;
		}

		keyComponent.BindToVehicleIdentifier(vehicleId);
		return key;
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns the owner's vehicle at a garage or impound pad, in the owner's presence. When the
	//! owner holds no live key (a restart unbinds carried keys), a fresh one bound to the same id
	//! goes into keyHolder's inventory, or onto the pad when there is no holder.
	protected static bool SpawnOwned(LF_VehicleRecord record, vector origin, vector angles, IEntity keyHolder)
	{
		string spawnName = record.m_sSpawnPrefab;
		if (spawnName.IsEmpty())
			spawnName = record.m_sPrefab;

		IEntity vehicle = LF_Utils.SpawnEntityPrefab(spawnName, origin, angles);
		if (!vehicle)
			return false;

		LF_VehicleLockComponent vehicleLock = LF_Component<LF_VehicleLockComponent>.Find(vehicle);
		if (!vehicleLock)
		{
			LF_Debug.Error("Vehicle", string.Format("restore refused: %1 has no LF_VehicleLockComponent", spawnName));
			SCR_EntityHelper.DeleteEntityAndChildren(vehicle);
			return false;
		}

		vehicleLock.SetVehicleIdentifier(record.m_sKeyId);
		vehicleLock.SetLocked(record.m_bLocked);

		LF_VehicleSession session = Sessions().Get(record.m_sVehicleId);
		if (!session)
		{
			session = new LF_VehicleSession();
			Sessions().Set(record.m_sVehicleId, session);
		}

		IEntity key = FindLive(session.m_Key);
		if (!key)
		{
			key = SpawnKey(record.m_sKeyId, KeySpawnOrigin(keyHolder, origin));
			if (!key)
			{
				SCR_EntityHelper.DeleteEntityAndChildren(vehicle);
				return false;
			}

			session.m_Key = key.GetID();
			GiveKey(keyHolder, key);
		}
		else if (key)
		{
			LF_VehicleKeyComponent keyComponent = LF_Component<LF_VehicleKeyComponent>.Find(key);
			if (keyComponent)
				keyComponent.BindToVehicleIdentifier(record.m_sKeyId);
		}

		session.m_Vehicle = vehicle.GetID();

		if (!record.m_sPlate.IsEmpty())
		{
			LF_LicensePlateManagerComponent plates = LF_Component<LF_LicensePlateManagerComponent>.Find(vehicle);
			if (plates)
				plates.SetRegistration(record.m_sPlate);
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Rebind(LF_VehicleRecord record)
	{
		if (!record)
			return;

		LF_VehicleSession session = Sessions().Get(record.m_sVehicleId);
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
			LF_VehicleLockComponent vehicleLock = LF_Component<LF_VehicleLockComponent>.Find(vehicle);
			if (vehicleLock)
			{
				vehicleLock.SetVehicleIdentifier(record.m_sKeyId);
				vehicleLock.SetLocked(record.m_bLocked);
			}
		}

		IEntity key = FindLive(session.m_Key);
		if (!key)
			return;

		LF_VehicleKeyComponent keyComponent = LF_Component<LF_VehicleKeyComponent>.Find(key);
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
