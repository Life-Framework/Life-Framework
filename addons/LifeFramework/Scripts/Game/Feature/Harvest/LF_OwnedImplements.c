// LF_OwnedImplements.c: the farm implements a character owns, and the rack that hitches them (#3295, harvester
// spike H8). The farm supply counter sells a reaper as a catalogue offer; the purchase puts "reaper" on the
// buyer's owned list (kept per character record key, saved in the "harvest" section: LF_HarvestSavePayload).
// The implement rack takes the owned reaper out onto a hitch-list vehicle beside the player (LF_ImplementHitch)
// and puts it back, which unhitches it and dumps whatever is in its hopper.
//
// What is out is session state only: the world holds the hitched entity, the save holds the list. A reaper
// that is out at a restart is therefore back on the list and not in the world. The hopper is never saved.

enum LF_EImplementResult
{
	OK,
	NOT_OWNED,
	ALREADY_OUT,
	NOT_OUT,
	NO_VEHICLE,
	HITCH_FAILED,
	VEHICLE_FAR,
	ALREADY_OWNED,
	BAD_OFFER,
	NO_CASH
}

class LF_OwnedImplements
{
	static const string FEATURE = "Harvest";
	static const string ID_REAPER = "reaper";
	//! Metres around the rack in which a hitch-list vehicle is found for take-out and must stand for put-back.
	static const float RACK_REACH_M = 8; // lf-fixed: the card's reach (#3295)
	static const string CASH_REASON = "implement-buy";

	//! Character record key -> implement ids owned.
	protected static ref map<string, ref array<string>> s_mOwned;
	//! "key|implement id" -> the implement entity in the world right now (session only).
	protected static ref map<string, EntityID> s_mOut;

	//------------------------------------------------------------------------------------------------
	protected static map<string, ref array<string>> Owned()
	{
		if (!s_mOwned)
			s_mOwned = new map<string, ref array<string>>();

		return s_mOwned;
	}

	//------------------------------------------------------------------------------------------------
	protected static map<string, EntityID> Out()
	{
		if (!s_mOut)
			s_mOut = new map<string, EntityID>();

		return s_mOut;
	}

	//------------------------------------------------------------------------------------------------
	protected static string OutKey(string key, string id)
	{
		return key + "|" + id;
	}

	//------------------------------------------------------------------------------------------------
	//! The implement id a catalogue prefab sells ("reaper"), or "" when the prefab is not an implement.
	static string IdForPrefab(ResourceName prefab)
	{
		if (prefab.IsEmpty())
			return "";

		LF_HitchesConfig config = LF_HitchesConfig.Load();
		if (!config || config.m_ReaperPrefab.IsEmpty())
			return "";

		if (LF_Utils.SamePrefab(prefab, config.m_ReaperPrefab))
			return ID_REAPER;

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! The prefab the hitch spawns for an implement id, or "".
	static ResourceName PrefabFor(string id)
	{
		if (id != ID_REAPER)
			return "";

		LF_HitchesConfig config = LF_HitchesConfig.Load();
		if (!config)
			return "";

		return config.m_ReaperPrefab;
	}

	//------------------------------------------------------------------------------------------------
	static bool Owns(string key, string id)
	{
		array<string> ids;
		if (key.IsEmpty() || !Owned().Find(key, ids))
			return false;

		return ids.Contains(id);
	}

	//------------------------------------------------------------------------------------------------
	//! Puts an implement on a character's list. False when the key or id is empty or it is already there.
	static bool Grant(string key, string id)
	{
		if (key.IsEmpty() || id.IsEmpty() || Owns(key, id))
			return false;

		array<string> ids;
		if (!Owned().Find(key, ids))
		{
			ids = new array<string>();
			Owned().Set(key, ids);
		}

		ids.Insert(id);
		LF_Debug.Info(FEATURE, string.Format("%1 now owns a %2", key, id));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the farm supply counter's sale of an implement (LF_ShopComponent.Buy hands an implement offer
	//! here instead of the inventory). Charges cash once, puts the implement on the buyer's owned list, and
	//! refuses a second reaper (an implement is bought once) or a quantity other than one before any cash moves.
	//! \param[out] charged Cash taken (0 when refused).
	static LF_EImplementResult Purchase(ResourceName prefab, IEntity buyer, string key, int unitPrice, int quantity, out int charged)
	{
		charged = 0;
		string id = IdForPrefab(prefab);
		if (id.IsEmpty() || !buyer || key.IsEmpty() || quantity != 1 || unitPrice <= 0)
		{
			LF_Debug.Warn(FEATURE, string.Format("implement buy refused: bad offer (prefab %1, key '%2', qty %3, price %4)", prefab, key, quantity, unitPrice));
			return LF_EImplementResult.BAD_OFFER;
		}

		if (Owns(key, id))
		{
			LF_Debug.Info(FEATURE, string.Format("implement buy refused: %1 already owns a %2", key, id));
			return LF_EImplementResult.ALREADY_OWNED;
		}

		int removed = LF_MoneyUtils.RemoveAmount(buyer, unitPrice, CASH_REASON);
		if (removed != unitPrice)
		{
			if (removed > 0)
				LF_MoneyUtils.AddAmount(buyer, removed, CASH_REASON + "-rollback");

			LF_Debug.Info(FEATURE, string.Format("implement buy refused: %1 could not pay %2", key, unitPrice));
			return LF_EImplementResult.NO_CASH;
		}

		Grant(key, id);
		charged = unitPrice;
		LF_Debug.Info(FEATURE, string.Format("implement bought: %1 paid %2 for a %3", key, unitPrice, id));
		return LF_EImplementResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: LF_ShopComponent.Buy's hand-off for an implement offer (after its price and gate checks).
	//! Maps Purchase's result onto the till's refusal for the order slip.
	static LF_EShopRefusal ShopBuy(ResourceName prefab, IEntity buyer, int unitPrice, int quantity, out int charged)
	{
		LF_EImplementResult result = Purchase(prefab, buyer, LF_Utils.GetPlayerUID(buyer), unitPrice, quantity, charged);
		switch (result)
		{
			case LF_EImplementResult.OK: return LF_EShopRefusal.NONE;
			case LF_EImplementResult.ALREADY_OWNED: return LF_EShopRefusal.ALREADY_OWNED;
			case LF_EImplementResult.NO_CASH: return LF_EShopRefusal.NOT_ENOUGH_CASH;
		}

		return LF_EShopRefusal.BAD_QUANTITY;
	}

	//------------------------------------------------------------------------------------------------
	//! The implement entity a character has out right now, or null (the entity is gone: a deleted or
	//! destroyed vehicle takes its reaper with it, which counts as put back).
	static IEntity OutEntity(string key, string id)
	{
		EntityID entityId;
		if (!Out().Find(OutKey(key, id), entityId))
			return null;

		IEntity entity = GetGame().GetWorld().FindEntityByID(entityId);
		if (!entity)
			Out().Remove(OutKey(key, id));

		return entity;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: takes the character's reaper out and hitches it to the nearest hitch-list vehicle within
	//! RACK_REACH_M of `at` that tows nothing and is not another character's saved vehicle.
	static LF_EImplementResult TakeOut(string key, string id, vector at, out IEntity implement)
	{
		implement = null;
		if (!Owns(key, id))
		{
			LF_Debug.Info(FEATURE, string.Format("take-out refused: %1 owns no %2", key, id));
			return LF_EImplementResult.NOT_OWNED;
		}

		if (OutEntity(key, id))
		{
			LF_Debug.Info(FEATURE, string.Format("take-out refused: %1's %2 is already out", key, id));
			return LF_EImplementResult.ALREADY_OUT;
		}

		IEntity vehicle = NearestHitchVehicle(key, at);
		if (!vehicle)
		{
			LF_Debug.Info(FEATURE, string.Format("take-out refused: no free hitch-list vehicle of %1 within %2 m", key, RACK_REACH_M));
			return LF_EImplementResult.NO_VEHICLE;
		}

		string reason;
		implement = LF_ImplementHitch.Hitch(vehicle, reason, PrefabFor(id));
		if (!implement)
		{
			LF_Debug.Warn(FEATURE, string.Format("take-out failed for %1: %2", key, reason));
			return LF_EImplementResult.HITCH_FAILED;
		}

		Out().Set(OutKey(key, id), implement.GetID());
		LF_Debug.Info(FEATURE, string.Format("%1 took the %2 out onto %3", key, id, LF_ImplementHitch.Describe(vehicle)));
		return LF_EImplementResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: puts the character's reaper back. Its towing vehicle must stand within RACK_REACH_M of `at`.
	//! The hopper is emptied into the ground, never kept or paid: `dumpedKg` says how much was in it.
	static LF_EImplementResult PutBack(string key, string id, vector at, out int dumpedKg)
	{
		dumpedKg = 0;
		IEntity implement = OutEntity(key, id);
		if (!implement)
		{
			LF_Debug.Info(FEATURE, string.Format("put-back refused: %1 has no %2 out", key, id));
			return LF_EImplementResult.NOT_OUT;
		}

		IEntity vehicle = implement.GetParent();
		if (!vehicle || vector.Distance(vehicle.GetOrigin(), at) > RACK_REACH_M)
		{
			LF_Debug.Info(FEATURE, string.Format("put-back refused: %1's towing vehicle is not within %2 m of the rack", key, RACK_REACH_M));
			return LF_EImplementResult.VEHICLE_FAR;
		}

		LF_ImplementComponent component = LF_ImplementComponent.Of(implement);
		if (component)
			dumpedKg = component.GetHopperKg();

		if (!LF_ImplementHitch.Unhitch(vehicle))
			return LF_EImplementResult.HITCH_FAILED;

		Out().Remove(OutKey(key, id));
		LF_Debug.Info(FEATURE, string.Format("%1 put the %2 back, hopper dumped (%3 kg)", key, id, dumpedKg));
		return LF_EImplementResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! The nearest vehicle within reach of `at` that is on the hitch list, tows nothing and is not saved as
	//! another character's. Null when there is none.
	protected static IEntity NearestHitchVehicle(string key, vector at)
	{
		IEntity best;
		float bestDistance = RACK_REACH_M + 1;
		array<IEntity> nearby = LF_Utils.GetNearbyVehicles(at, RACK_REACH_M);
		foreach (IEntity vehicle : nearby)
		{
			if (!LF_ImplementHitch.RowFor(vehicle) || LF_ImplementHitch.FindHitched(vehicle))
				continue;

			if (IsOthersVehicle(key, vehicle))
				continue;

			float distance = vector.Distance(vehicle.GetOrigin(), at);
			if (distance < bestDistance)
			{
				bestDistance = distance;
				best = vehicle;
			}
		}

		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! A vehicle saved as another character's (by its lock id) is not hitched to by someone else.
	protected static bool IsOthersVehicle(string key, IEntity vehicle)
	{
		LF_VehicleLockComponent vehicleLock = LF_Component<LF_VehicleLockComponent>.Find(vehicle);
		if (!vehicleLock)
			return false;

		string owner = LF_VehicleOwnership.GetOwner(vehicleLock.GetVehicleIdentifier());
		return !owner.IsEmpty() && owner != key;
	}

	//------------------------------------------------------------------------------------------------
	//! One row per implement owned, in key order for a stable file.
	static array<ref LF_OwnedImplementRow> ExportAll()
	{
		array<ref LF_OwnedImplementRow> rows = {};
		foreach (string key, array<string> ids : Owned())
		{
			foreach (string id : ids)
			{
				rows.Insert(LF_OwnedImplementRow.Create(key, id));
			}
		}

		return rows;
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces the owned list with the saved rows; a row with no key, or an id that is not an implement
	//! the config knows, is skipped with a log line. What is out in the world is not touched.
	//! \return the implements owned afterwards.
	static int ApplyAll(array<ref LF_OwnedImplementRow> rows)
	{
		Owned().Clear();
		if (!rows)
			return 0;

		int kept;
		foreach (LF_OwnedImplementRow row : rows)
		{
			if (!row || row.m_sKey.IsEmpty() || row.m_sImplement != ID_REAPER)
			{
				LF_Debug.Warn(FEATURE, "saved implement row skipped (no key, or an implement this build does not know)");
				continue;
			}

			if (Grant(row.m_sKey, row.m_sImplement))
				kept++;
		}

		return kept;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mOwned = null;
		s_mOut = null;
	}
}

//------------------------------------------------------------------------------------------------
//! One saved ownership: a character record key and an implement id. Field order is the binary save
//! order; append only.
class LF_OwnedImplementRow : Managed
{
	static const int ROW_VERSION = 1;

	int m_iVersion;
	string m_sKey;
	string m_sImplement;

	//------------------------------------------------------------------------------------------------
	static LF_OwnedImplementRow Create(string key, string implement)
	{
		LF_OwnedImplementRow row = new LF_OwnedImplementRow();
		row.m_iVersion = ROW_VERSION;
		row.m_sKey = key;
		row.m_sImplement = implement;
		return row;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_OwnedImplements' statics (LF_StaticReset finds it by reflection).
class LF_OwnedImplements_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_OwnedImplements";
	}

	override void Apply()
	{
		LF_OwnedImplements.ResetStatics();
	}
}
