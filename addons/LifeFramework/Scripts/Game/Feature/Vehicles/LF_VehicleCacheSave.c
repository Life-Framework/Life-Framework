//------------------------------------------------------------------------------------------------
//! One item in a saved hidden compartment ("vehiclecache" section, #3435). The flat list of
//! LF_StorageSnapshot: m_iParent is the index of the item this one sits in (-1 = the compartment).
//! Field order is the binary save order; append only.
class LF_VehicleCacheItemRow : Managed
{
	string m_sPrefab;
	int m_iQuantity;
	int m_iParent;
	int m_iStorage;
	int m_iSlot;
	int m_iAmmo;
	string m_sSerial;
	bool m_bStolen;

	//------------------------------------------------------------------------------------------------
	static LF_VehicleCacheItemRow From(notnull LF_StorageItemRecord item)
	{
		LF_VehicleCacheItemRow row = new LF_VehicleCacheItemRow();
		row.m_sPrefab = item.m_sPrefab;
		row.m_iQuantity = item.m_iQuantity;
		row.m_iParent = item.m_iParent;
		row.m_iStorage = item.m_iStorage;
		row.m_iSlot = item.m_iSlot;
		row.m_iAmmo = item.m_iAmmo;
		row.m_sSerial = item.m_sSerial;
		row.m_bStolen = item.m_bStolen;
		return row;
	}

	//------------------------------------------------------------------------------------------------
	LF_StorageItemRecord ToItem()
	{
		LF_StorageItemRecord item = LF_StorageItemRecord.Create(m_sPrefab, m_iQuantity, m_iParent, m_iStorage, m_iSlot);
		item.m_iAmmo = m_iAmmo;
		item.m_sSerial = m_sSerial;
		item.m_bStolen = m_bStolen;
		return item;
	}
}

//------------------------------------------------------------------------------------------------
//! One owned vehicle's cache in the save (#3435), keyed by its garage id (the LF_VehicleOwnership id,
//! which is also its lock's vehicle identifier, so stable across restarts): the inspection sticker's
//! expiry game day (-1 = none) and the hidden compartment with its goods. Append only.
class LF_VehicleCacheRow : Managed
{
	static const int ROW_VERSION = 1;

	int m_iVersion;
	string m_sVehicleId;
	int m_iStickerExpiry;
	bool m_bCompartment;
	ref array<ref LF_VehicleCacheItemRow> m_aGoods = {};

	//------------------------------------------------------------------------------------------------
	static LF_VehicleCacheRow Create(string vehicleId, int stickerExpiry, LF_StorageSnapshotRecord goods)
	{
		LF_VehicleCacheRow row = new LF_VehicleCacheRow();
		row.m_iVersion = ROW_VERSION;
		row.m_sVehicleId = vehicleId;
		row.m_iStickerExpiry = stickerExpiry;
		row.m_bCompartment = goods != null;
		if (goods && goods.m_aItems)
		{
			foreach (LF_StorageItemRecord item : goods.m_aItems)
			{
				if (item)
					row.m_aGoods.Insert(LF_VehicleCacheItemRow.From(item));
			}
		}

		return row;
	}
}

//------------------------------------------------------------------------------------------------
//! The hidden compartments of owned vehicles, by garage id (#3435). A compartment is a child entity of
//! the car (LF_VehicleCompartment), so it and its goods go when the car is garaged, towed, impounded or
//! despawns. Each time LF_VehicleOwnership captures a car's cargo (a hook in CaptureCargo) the
//! compartment is taken into this ledger; when the car spawns again (a hook in SpawnOwned) it is fitted
//! again and its goods go back in. The stickers stay in LF_Inspection; the section reads them out by
//! the owned car's id. Server-side state.
class LF_VehicleCache
{
	static const string FEATURE = "Vehicle";
	//! The snapshot slot name of a compartment's goods (never stored in LF_StorageSnapshot's own table).
	static const string SLOT_COMPARTMENT = "vehiclecompartment";

	protected static ref map<string, ref LF_StorageSnapshotRecord> s_mGoods;

	//------------------------------------------------------------------------------------------------
	protected static map<string, ref LF_StorageSnapshotRecord> Goods()
	{
		if (!s_mGoods)
			s_mGoods = new map<string, ref LF_StorageSnapshotRecord>();

		return s_mGoods;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mGoods = null;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the vehicle has a saved compartment (fitted, maybe empty).
	static bool Has(string vehicleId)
	{
		return Goods().Contains(vehicleId);
	}

	//------------------------------------------------------------------------------------------------
	//! \return Saved compartments.
	static int Count()
	{
		return Goods().Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Takes a live car's compartment into the ledger. A car without one leaves its row as it is.
	//! \return True when a compartment was captured.
	static bool Capture(string vehicleId, IEntity vehicle)
	{
		if (vehicleId.IsEmpty() || !vehicle)
			return false;

		IEntity panel = LF_VehicleCompartment.Find(vehicle);
		if (!panel)
			return false;

		LF_StorageSnapshotRecord goods = LF_StorageSnapshot.Capture(vehicleId, SLOT_COMPARTMENT, panel);
		if (!goods)
			return false;

		Goods().Set(vehicleId, goods);
		LF_Debug.Log(FEATURE, string.Format("compartment of %1 captured: %2 item(s)", vehicleId, goods.m_aItems.Count()));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Drops a car's saved compartment (scrapped, burned, gone from the world without a capture).
	static void Forget(string vehicleId, string why)
	{
		if (!Goods().Contains(vehicleId))
			return;

		Goods().Remove(vehicleId);
		LF_Debug.Info(FEATURE, string.Format("compartment of %1 dropped: %2", vehicleId, why));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a car spawned again (SpawnOwned). Its saved compartment goes back once its entity is up.
	static void ApplyLater(string vehicleId)
	{
		if (!Has(vehicleId) || !GetGame() || !GetGame().GetCallqueue())
			return;

		GetGame().GetCallqueue().CallLater(ApplyById, 0, false, vehicleId);
	}

	//------------------------------------------------------------------------------------------------
	protected static void ApplyById(string vehicleId)
	{
		Apply(vehicleId, LF_VehicleOwnership.GetVehicle(vehicleId));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: fits the live car's compartment again and makes its goods equal the saved ones. Safe to
	//! call twice: the second call finds the same compartment and the same goods.
	//! \return Items spawned; 0 with no live car, no saved compartment, or goods that already match.
	static int Apply(string vehicleId, IEntity vehicle)
	{
		if (!Replication.IsServer() || vehicleId.IsEmpty() || !vehicle)
			return 0;

		LF_StorageSnapshotRecord goods = Goods().Get(vehicleId);
		if (!goods)
			return 0;

		IEntity panel = LF_VehicleCompartment.Fit(vehicle);
		if (!panel)
		{
			LF_Debug.Error(FEATURE, string.Format("compartment of %1 not restored: it could not be fitted again", vehicleId));
			return 0;
		}

		LF_VehicleWorldSave.Hold(panel);
		int spawned = LF_StorageSnapshot.Apply(panel, goods);
		LF_Debug.Info(FEATURE, string.Format("compartment of %1 restored: %2 record(s), %3 spawned", vehicleId, goods.m_aItems.Count(), spawned));
		return spawned;
	}

	//------------------------------------------------------------------------------------------------
	//! One row per owned vehicle with a sticker or a compartment, in purchase order. A ledger row of a
	//! vehicle that is no longer owned is not written.
	static array<ref LF_VehicleCacheRow> ExportAll()
	{
		array<ref LF_VehicleCacheRow> rows = new array<ref LF_VehicleCacheRow>();
		foreach (LF_VehicleRecord record : LF_VehicleOwnership.ExportAll())
		{
			if (!record || record.m_sVehicleId.IsEmpty())
				continue;

			string vehicleId = record.m_sVehicleId;
			int expiry = LF_Inspection.ExpiryOf(vehicleId);
			LF_StorageSnapshotRecord goods = Goods().Get(vehicleId);
			if (expiry < 0 && !goods)
				continue;

			rows.Insert(LF_VehicleCacheRow.Create(vehicleId, expiry, goods));
		}

		return rows;
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces every owned vehicle's sticker and the compartment ledger with `rows`, then puts each live
	//! car's compartment in line with it. A row for a vehicle the records do not know (scrapped since) is
	//! dropped with a log line; a row whose car is garaged or impounded waits for it to spawn. Loading
	//! the same rows twice ends in the same state.
	//! \return How many rows were kept.
	static int ApplyAll(notnull array<ref LF_VehicleCacheRow> rows)
	{
		Goods().Clear();
		array<ref LF_VehicleRecord> records = LF_VehicleOwnership.ExportAll();
		foreach (LF_VehicleRecord cleared : records)
		{
			if (cleared)
				LF_Inspection.Restore(cleared.m_sVehicleId, -1);
		}

		int kept;
		foreach (LF_VehicleCacheRow row : rows)
		{
			if (!row || row.m_sVehicleId.IsEmpty())
				continue;

			if (LF_VehicleOwnership.GetOwner(row.m_sVehicleId).IsEmpty())
			{
				LF_Debug.Warn(FEATURE, "saved sticker and compartment of an unknown vehicle dropped: " + row.m_sVehicleId);
				continue;
			}

			LF_Inspection.Restore(row.m_sVehicleId, row.m_iStickerExpiry);
			if (row.m_bCompartment)
			{
				LF_StorageSnapshotRecord goods = LF_StorageSnapshotRecord.Create(row.m_sVehicleId, SLOT_COMPARTMENT);
				foreach (LF_VehicleCacheItemRow item : row.m_aGoods)
				{
					if (item && !item.m_sPrefab.IsEmpty())
						goods.m_aItems.Insert(item.ToItem());
				}

				Goods().Set(row.m_sVehicleId, goods);
			}

			kept++;
		}

		foreach (LF_VehicleRecord record : records)
		{
			if (!record)
				continue;

			IEntity vehicle = LF_VehicleOwnership.GetVehicle(record.m_sVehicleId);
			if (vehicle)
				Apply(record.m_sVehicleId, vehicle);
		}

		return kept;
	}
}

//------------------------------------------------------------------------------------------------
class LF_VehicleCache_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_VehicleCache";
	}

	override void Apply()
	{
		LF_VehicleCache.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
//! Save section "vehiclecache" (#3435): one LF_VehicleCacheRow per owned vehicle with an inspection
//! sticker or a hidden compartment, keyed by the vehicle's garage id. Sections apply after the
//! "vehicles" payload, so every owned car's record is back when the rows are checked; the goods return
//! when the car spawns again. A save from before the section holds no sticker and no compartment,
//! which is the state a boot starts in, so OnAbsent keeps the default no-op.
class LF_VehicleCacheSavePayload : LF_SavePayload
{
	static const string NAME = "vehiclecache";

	//------------------------------------------------------------------------------------------------
	override string Name()
	{
		return NAME;
	}

	//------------------------------------------------------------------------------------------------
	override int Version()
	{
		return 1;
	}

	//------------------------------------------------------------------------------------------------
	override void WriteSection(notnull SaveContext context)
	{
		array<ref LF_VehicleCacheRow> rows = LF_VehicleCache.ExportAll();
		context.WriteValue("rows", rows);
		LF_Debug.Log("Persistence", string.Format("saved %1 vehicle cache row(s)", rows.Count()));
	}

	//------------------------------------------------------------------------------------------------
	override bool ReadSection(notnull LoadContext context, int savedVersion)
	{
		array<ref LF_VehicleCacheRow> rows = new array<ref LF_VehicleCacheRow>();
		if (!context.ReadValue("rows", rows))
		{
			LF_Debug.Warn(LF_VehicleCache.FEATURE, "vehicle cache failed to read; stickers and compartments stay as they were");
			return false;
		}

		int kept = LF_VehicleCache.ApplyAll(rows);
		LF_Debug.Info("Persistence", string.Format("applied %1 of %2 vehicle cache row(s)", kept, rows.Count()));
		return true;
	}
}
