//------------------------------------------------------------------------------------------------
//! Why a vehicle is held (#752a, "vehiclestate" payload). The restart impound is not a tow:
//! a car left on the street at a restart stays NONE and is derived as impounded (IsImpounded).
enum LF_EVehicleTowState
{
	NONE = 0,
	TOWED = 1,
	POLICE_IMPOUND = 2
}

//------------------------------------------------------------------------------------------------
//! The saved vehicle state that does not fit the positional "vehicles" record (#752a,
//! docs/architecture/record-shape-vehicle.md). One row per record whose state is not the default,
//! keyed by vehicle id. A later field is a new payload, never a new member here.
//------------------------------------------------------------------------------------------------
class LF_VehicleStateRow
{
	static const int VERSION = 1;

	int m_iVersion;
	string m_sVehicleId;
	int m_iTowState;
	int m_iTowedAt;
	int m_iInspectedAt;
	int m_iCompartment;

	//------------------------------------------------------------------------------------------------
	static LF_VehicleStateRow Create(string vehicleId, int towState, int towedAt, int inspectedAt, int compartment)
	{
		LF_VehicleStateRow row();
		row.m_iVersion = VERSION;
		row.m_sVehicleId = vehicleId;
		row.m_iTowState = towState;
		row.m_iTowedAt = towedAt;
		row.m_iInspectedAt = inspectedAt;
		row.m_iCompartment = compartment;
		return row;
	}
}

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

	// Vehicle state (#752a). Not part of the positional "vehicles" payload: it travels in the
	// "vehiclestate" payload (LF_VehicleStateRow), so the record's saved layout never moves.
	//! LF_EVehicleTowState.
	[NonSerialized()]
	int m_iTowState;
	//! Unix seconds the tow state was set, 0 = never.
	[NonSerialized()]
	int m_iTowedAt;
	//! Unix seconds of the last inspection, 0 = never.
	[NonSerialized()]
	int m_iInspectedAt;
	//! Fitted hidden compartment size, 0 = none, up to LF_VehicleOwnership.COMPARTMENT_MAX.
	[NonSerialized()]
	int m_iCompartment;

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

		LF_VehicleRecord copy = Create(
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
		copy.CopyStateFrom(source);
		return copy;
	}

	//------------------------------------------------------------------------------------------------
	void CopyStateFrom(notnull LF_VehicleRecord source)
	{
		m_iTowState = source.m_iTowState;
		m_iTowedAt = source.m_iTowedAt;
		m_iInspectedAt = source.m_iInspectedAt;
		m_iCompartment = source.m_iCompartment;
	}

	//------------------------------------------------------------------------------------------------
	//! A stock car: never towed, never inspected, no compartment. Writes no state row.
	bool HasDefaultState()
	{
		return m_iTowState == LF_EVehicleTowState.NONE && m_iTowedAt == 0 && m_iInspectedAt == 0 && m_iCompartment == 0;
	}

	//------------------------------------------------------------------------------------------------
	void ResetState()
	{
		m_iTowState = LF_EVehicleTowState.NONE;
		m_iTowedAt = 0;
		m_iInspectedAt = 0;
		m_iCompartment = 0;
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
	//! The largest hidden compartment size (#752a, maintainer default on #752: a level 0-3).
	static const int COMPARTMENT_MAX = 3; // lf-fixed: the saved range of the compartment level (#752); a pack sells sizes within it

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
	//! World end: drop every record, session and the order list; the next world loads its own.
	static void ResetStatics()
	{
		s_mRecords = null;
		s_mSessions = null;
		s_aOrder = null;
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
	//! The "vehiclestate" payload (#752a): one row per record whose state is not the default, in
	//! purchase order. A stock car writes no row.
	static array<ref LF_VehicleStateRow> ExportState()
	{
		array<ref LF_VehicleStateRow> rows = new array<ref LF_VehicleStateRow>();
		foreach (string vehicleId : Order())
		{
			LF_VehicleRecord record = Records().Get(vehicleId);
			if (!record || record.HasDefaultState())
				continue;

			rows.Insert(LF_VehicleStateRow.Create(vehicleId, record.m_iTowState, record.m_iTowedAt, record.m_iInspectedAt, record.m_iCompartment));
		}

		return rows;
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces the state of every live record: each starts at the default, then each row is written
	//! onto its record. Runs after "vehicles" in the same load. A row for a vehicle that is not loaded
	//! is skipped; a bad value is corrected and logged; one bad row never stops the others. Idempotent.
	//! \return The number of rows applied.
	static int ApplyState(array<ref LF_VehicleStateRow> rows)
	{
		foreach (string vehicleId : Order())
		{
			LF_VehicleRecord record = Records().Get(vehicleId);
			if (record)
				record.ResetState();
		}

		if (!rows)
			return 0;

		int applied = 0;
		foreach (LF_VehicleStateRow row : rows)
		{
			if (!row || row.m_sVehicleId.IsEmpty())
				continue;

			LF_VehicleRecord live = Records().Get(row.m_sVehicleId);
			if (!live)
			{
				LF_Debug.Warn("Vehicle", string.Format("state row for unknown vehicle %1 skipped", row.m_sVehicleId));
				continue;
			}

			WriteState(live, row.m_iTowState, row.m_iTowedAt, row.m_iInspectedAt, row.m_iCompartment);
			applied++;
		}

		return applied;
	}

	//------------------------------------------------------------------------------------------------
	//! Sets one vehicle's saved state (server). The writers are #774 (tow), #778 (inspection,
	//! compartment) and #859 (police impound); bad values are corrected as on load.
	//! \return False when the vehicle is not owned.
	static bool SetState(string vehicleId, int towState, int towedAt, int inspectedAt, int compartment)
	{
		LF_VehicleRecord live = Records().Get(vehicleId);
		if (!live)
		{
			LF_Debug.Warn("Vehicle", string.Format("set state rejected: unknown vehicle %1", vehicleId));
			return false;
		}

		WriteState(live, towState, towedAt, inspectedAt, compartment);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The vehicle's state as a row (a copy), or null when it is not owned.
	static LF_VehicleStateRow StateOf(string vehicleId)
	{
		LF_VehicleRecord live = Records().Get(vehicleId);
		if (!live)
			return null;

		return LF_VehicleStateRow.Create(vehicleId, live.m_iTowState, live.m_iTowedAt, live.m_iInspectedAt, live.m_iCompartment);
	}

	//------------------------------------------------------------------------------------------------
	protected static void WriteState(notnull LF_VehicleRecord live, int towState, int towedAt, int inspectedAt, int compartment)
	{
		string vehicleId = live.m_sVehicleId;
		if (towState < LF_EVehicleTowState.NONE || towState > LF_EVehicleTowState.POLICE_IMPOUND)
		{
			LF_Debug.Error("Vehicle", string.Format("vehicle %1: unknown tow state %2 reads as none", vehicleId, towState));
			towState = LF_EVehicleTowState.NONE;
		}

		if (towedAt < 0)
		{
			LF_Debug.Error("Vehicle", string.Format("vehicle %1: negative towed-at %2 reads as never", vehicleId, towedAt));
			towedAt = 0;
		}

		if (inspectedAt < 0)
		{
			LF_Debug.Error("Vehicle", string.Format("vehicle %1: negative inspected-at %2 reads as never", vehicleId, inspectedAt));
			inspectedAt = 0;
		}

		int fitted = Math.ClampInt(compartment, 0, COMPARTMENT_MAX);
		if (fitted != compartment)
			LF_Debug.Error("Vehicle", string.Format("vehicle %1: compartment level %2 clamped to %3", vehicleId, compartment, fitted));

		live.m_iTowState = towState;
		live.m_iTowedAt = towedAt;
		live.m_iInspectedAt = inspectedAt;
		live.m_iCompartment = fitted;
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
	//! What the impound desk charges for this vehicle: the vehicle prices config (#283), else the
	//! desk's flat fee (LF_GarageConfig).
	static int ImpoundFeeOf(string vehicleId)
	{
		LF_VehicleRecord record = Records().Get(vehicleId);
		if (record)
		{
			int fee = LF_VehiclePricesConfig.Lookup(LF_EVehiclePriceAction.IMPOUND, record.m_sSpawnPrefab);
			if (fee < 0)
				fee = LF_VehiclePricesConfig.Lookup(LF_EVehiclePriceAction.IMPOUND, record.m_sPrefab);
			if (fee >= 0)
				return fee;
		}

		return LF_GarageConfig.ImpoundFee();
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
		reasonKey = "#LF-Systems_Off";
		if (LF_Systems.Refuses("Vehicles", "impound collect"))
			return false;

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
	//! True when this entity is the live car of a saved vehicle record, by session or by the key id
	//! its lock carries. The world cleanup never sweeps one.
	static bool IsOwnedEntity(IEntity entity)
	{
		if (!entity)
			return false;

		string lockId;
		LF_VehicleLockComponent vehicleLock = LF_Component<LF_VehicleLockComponent>.Find(entity);
		if (vehicleLock)
			lockId = vehicleLock.GetVehicleIdentifier();

		foreach (string vehicleId, LF_VehicleRecord record : Records())
		{
			if (GetVehicle(vehicleId) == entity)
				return true;

			if (!lockId.IsEmpty() && (record.m_sVehicleId == lockId || record.m_sKeyId == lockId))
				return true;
		}

		return false;
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
		reasonKey = "#LF-Systems_Off";
		if (LF_Systems.Refuses("Vehicles", "vehicle purchase"))
			return false;

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
		if (LF_Systems.Refuses("Vehicles", "extra key"))
			return false;

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
	//! Change the locks (#2411): the owner pays, the vehicle takes a new key id, and every key cut
	//! before stops working, wherever it is. The owner gets one new key. The new id lives in the
	//! record's key id, which the "vehicles" payload already saves, so a restart keeps it.
	//! \return True when the locks changed and the new key was handed over.
	static bool ChangeLocks(string vehicleId, string ownerUid, int price, int cashCeiling, IEntity owner, vector origin, out int cashCharged, out string reasonKey)
	{
		cashCharged = 0;
		reasonKey = "#LF-Systems_Off";
		if (LF_Systems.Refuses("Vehicles", "change locks"))
			return false;

		if (!Replication.IsServer())
		{
			LF_Debug.Error("Vehicle", "change locks refused: not running on the server");
			return false;
		}

		reasonKey = "#EL-Vehicle_NotOwner";
		LF_VehicleRecord record = Records().Get(vehicleId);
		if (!Owned(record, ownerUid))
		{
			LF_Debug.Info("Vehicle", string.Format("change locks refused: %1 does not own %2", ownerUid, vehicleId));
			return false;
		}

		reasonKey = RefusalKey(price, cashCeiling);
		int cash = CashOn(owner);
		int bank = 0;
		LF_BankAccount account = LF_ATMManager.GetOrCreate(ownerUid);
		if (account)
			bank = account.GetBalance();

		int cashDebit = 0;
		int bankDebit = 0;
		if (!QuotePurchase(price, cashCeiling, cash, bank, cashDebit, bankDebit))
		{
			LF_Debug.Info("Vehicle", string.Format("change locks refused: %1 cannot pay %2", ownerUid, price));
			return false;
		}

		if (!TakePayment(owner, account, cashDebit, bankDebit))
			return false;

		reasonKey = "#EL-Vehicle_NoSpawn";
		string keyId = PersistenceIdUtils.Generate();
		IEntity key = SpawnKey(keyId, KeySpawnOrigin(owner, origin));
		if (!key)
		{
			Refund(owner, account, cashDebit, bankDebit);
			return false;
		}

		string oldKeyId = record.m_sKeyId;
		record.m_sKeyId = keyId;

		LF_VehicleSession session = Sessions().Get(vehicleId);
		if (!session)
		{
			session = new LF_VehicleSession();
			Sessions().Set(vehicleId, session);
		}

		session.m_Key = key.GetID();
		IEntity vehicle = FindLive(session.m_Vehicle);
		if (vehicle)
		{
			LF_VehicleLockComponent vehicleLock = LF_Component<LF_VehicleLockComponent>.Find(vehicle);
			if (vehicleLock)
				vehicleLock.SetKeyId(keyId);
		}

		GiveKey(owner, key);
		cashCharged = cashDebit;
		reasonKey = "#LF-Lot_LocksChanged";
		LF_Debug.Info("Vehicle", string.Format("locks changed on %1 owner=%2: key %3 -> %4", vehicleId, ownerUid, oldKeyId, keyId));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when holder carries a key bound to keyId (left hand or any storage).
	static bool HoldsKey(IEntity holder, string keyId)
	{
		if (!holder || keyId.IsEmpty())
			return false;

		CharacterControllerComponent controller = LF_Component<CharacterControllerComponent>.Find(holder);
		if (controller && KeyMatches(controller.GetAttachedGadgetAtLeftHandSlot(), keyId))
			return true;

		SCR_InventoryStorageManagerComponent inventory = LF_Component<SCR_InventoryStorageManagerComponent>.Find(holder);
		if (!inventory)
			return false;

		array<IEntity> items = {};
		inventory.FindItems(items, new LF_VehicleKeyScanPredicate());
		foreach (IEntity item : items)
		{
			if (KeyMatches(item, keyId))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool KeyMatches(IEntity item, string keyId)
	{
		if (!item)
			return false;

		LF_VehicleKeyComponent keyComponent = LF_Component<LF_VehicleKeyComponent>.Find(item);
		return keyComponent && LF_VehicleLockComponent.IdentifiersMatch(keyComponent.GetVehicleIdentifier(), keyId);
	}

	//------------------------------------------------------------------------------------------------
	static bool Store(string vehicleId, string ownerUid)
	{
		if (LF_Systems.Refuses("Vehicles", "vehicle store"))
			return false;

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
				// The record keeps no inventory and the delete below takes the children with it:
				// a car holding items or people stays on the street (#2411).
				string refusalKey = LF_Garage.StoreRefusalKey(vehicle);
				if (!refusalKey.IsEmpty())
				{
					LF_Debug.Warn("Vehicle", string.Format("store rejected: %1 is not empty (%2)", vehicleId, refusalKey));
					return false;
				}

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
		if (LF_Systems.Refuses("Vehicles", "vehicle retrieve"))
			return false;

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
		live.CopyStateFrom(record);
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
		if (LF_Systems.Refuses("Vehicles", "test car grant"))
			return 0;

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
			LF_VehicleSession session = new LF_VehicleSession();
			session.m_Key = key.GetID();
			Sessions().Set(id, session);
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
	//! The prefab a respawn uses. The saved spawn prefab is the car's colour (epic #1826); when a
	//! pack dropped that colour it no longer loads, so the car comes back in the model's base colour
	//! (the colours config's first, else the locked child the prices config lists) with a warning.
	//! With nothing better to offer it returns the saved name, so the failure stays the spawn's own.
	static string RespawnNameFor(string spawnName, string catalogPrefab, bool savedLoads, LF_VehicleColoursConfig colours, LF_VehiclePricesConfig prices)
	{
		if (savedLoads)
			return spawnName;

		string pricesSpawn;
		if (prices)
		{
			LF_VehiclePriceEntry entry = prices.Find(catalogPrefab);
			if (entry)
				pricesSpawn = entry.m_SpawnPrefab;
		}

		string fallback = pricesSpawn;
		if (colours)
			fallback = colours.RespawnFallback(catalogPrefab, pricesSpawn);

		if (fallback.IsEmpty() || fallback == spawnName)
			return spawnName;

		LF_Debug.Warn("Vehicle", string.Format("saved spawn prefab does not load (%1); respawning %2 in its base colour %3", spawnName, catalogPrefab, fallback));
		return fallback;
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

		Resource saved = Resource.Load(spawnName);
		bool savedLoads = saved && saved.IsValid();
		spawnName = RespawnNameFor(spawnName, record.m_sPrefab, savedLoads, LF_VehicleColoursConfig.Get(), LF_VehiclePricesConfig.Get());
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

		vehicleLock.SetVehicleIdentifier(record.m_sVehicleId);
		vehicleLock.SetKeyId(record.m_sKeyId);
		vehicleLock.SetLocked(record.m_bLocked);

		LF_VehicleSession session = Sessions().Get(record.m_sVehicleId);
		if (!session)
		{
			session = new LF_VehicleSession();
			Sessions().Set(record.m_sVehicleId, session);
		}

		// The owner leaves with a working key (#2411). When the last key handed out is alive but not
		// on the owner (a thief, a friend, a stash), the garage re-keys the car: the owner gets a
		// key to a new id and the one elsewhere stops working, so a garage trip never mints a free
		// duplicate of a key that still opens the car.
		IEntity key = FindLive(session.m_Key);
		bool keyElsewhere = key && keyHolder && !HoldsKey(keyHolder, record.m_sKeyId);
		if (!key || keyElsewhere)
		{
			string keyId = record.m_sKeyId;
			if (keyElsewhere)
				keyId = PersistenceIdUtils.Generate();

			key = SpawnKey(keyId, KeySpawnOrigin(keyHolder, origin));
			if (!key)
			{
				SCR_EntityHelper.DeleteEntityAndChildren(vehicle);
				return false;
			}

			if (keyElsewhere)
			{
				LF_Debug.Info("Vehicle", string.Format("re-keyed %1 at the pad: the owner came without the last key (%2 -> %3)", record.m_sVehicleId, record.m_sKeyId, keyId));
				record.m_sKeyId = keyId;
				vehicleLock.SetKeyId(keyId);
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
				vehicleLock.SetVehicleIdentifier(record.m_sVehicleId);
				vehicleLock.SetKeyId(record.m_sKeyId);
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

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_VehicleOwnership's statics (registered at script load; Core/LF_StaticReset.c).
class LF_VehicleOwnership_StaticResetHook : LF_StaticResetHook
{
	protected static bool s_bRegistered = LF_StaticReset.Register(new LF_VehicleOwnership_StaticResetHook());

	override string Owner()
	{
		return "LF_VehicleOwnership";
	}

	override void Apply()
	{
		LF_VehicleOwnership.ResetStatics();
	}
}
