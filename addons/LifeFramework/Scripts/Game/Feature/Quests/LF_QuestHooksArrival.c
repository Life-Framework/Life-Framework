// LF_QuestHooksArrival.c - what a pack's arrival chain needs beyond the core steps (the Everon route:
// the clerk, the garage, the orchard, the juice press, the market). A step can put a starter car in the
// player's garage as it starts (LF_QuestDefinition.m_sStartVehiclePrefab), taking a car out of a garage
// reports `garage.retrieved`, and a sale to a trader reports `trader.sold:<GUID>`. The garage action is
// a modded override, so the vehicle files keep their own success path.

//------------------------------------------------------------------------------------------------
//! SIGNAL keys for the arrival route.
class LF_QuestArrivalSignals
{
	static const string GARAGE_RETRIEVED = "garage.retrieved";
	static const string TRADER_SOLD = "trader.sold:";

	//------------------------------------------------------------------------------------------------
	//! "trader.sold:<GUID>" when a trader paid for a prefab: the prefab's GUID in upper case
	//! (LF_Utils.PrefabGuid), so a quest names the good by GUID and survives a move.
	static string TraderSold(ResourceName prefab)
	{
		return TRADER_SOLD + LF_Utils.PrefabGuid(prefab);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: moves the user's matching SIGNAL objectives. A client, or a body with no tracker, reports nothing.
	static bool Report(IEntity user, string key)
	{
		if (!user || !Replication.IsServer())
			return false;

		LF_QuestManager manager = LF_QuestManager.GetInstance();
		if (!manager)
			return false;

		return manager.ReportSignal(user, key);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a trader paid `user` for `prefab` (LF_TraderSale.Sell).
	static void ReportTraderSale(IEntity user, ResourceName prefab)
	{
		if (prefab.IsEmpty())
			return;

		Report(user, TraderSold(prefab));
	}
}

//------------------------------------------------------------------------------------------------
//! A quest step's starter car: one cheap lot car, owned and stored in the garage, its key in the pocket.
//! Once per character: a character who owns any vehicle (stored, on the street, towed or impounded) gets
//! none, so the step starting again after a respawn, a rejoin or a restart hands over nothing. A character
//! who registered before the route existed and already owns a car takes that one out instead.
class LF_StarterVehicle
{
	static const string FEATURE = "Quests";

	//------------------------------------------------------------------------------------------------
	//! Server: puts the step's car in the holder's garage. \return True when a car was granted.
	static bool Grant(IEntity holder, string uid, notnull LF_QuestDefinition def)
	{
		if (!Replication.IsServer() || def.m_sStartVehiclePrefab.IsEmpty())
			return false;

		if (uid.IsEmpty() || !holder)
		{
			LF_Debug.Error(FEATURE, string.Format("starter car of quest=%1 not granted: no player or no body behind the step", def.m_sQuestId));
			return false;
		}

		if (LF_VehicleOwnership.OwnsAny(uid))
		{
			LF_Debug.Info(FEATURE, string.Format("starter car of quest=%1 skipped for %2: already owns a vehicle", def.m_sQuestId, uid));
			return false;
		}

		if (LF_GarageSlots.UsedBy(uid) >= LF_GarageSlots.CapOf(uid))
		{
			LF_Debug.Warn(FEATURE, string.Format("starter car of quest=%1 not granted to %2: no free garage slot", def.m_sQuestId, uid));
			return false;
		}

		string shopId;
		LF_VehicleOffer offer = LF_VehicleLotComponent.FindOfferOnAnyLot(def.m_sStartVehiclePrefab, shopId);
		if (!offer)
		{
			LF_Debug.Error(FEATURE, string.Format("starter car of quest=%1 not granted: no lot offers %2", def.m_sQuestId, def.m_sStartVehiclePrefab));
			return false;
		}

		string vehicleId;
		if (!LF_VehicleOwnership.GrantStoredCar(uid, holder, offer, shopId, vehicleId))
			return false;

		LF_Debug.Info(FEATURE, string.Format("starter car %1 put in the garage for %2 (quest=%3)", vehicleId, uid, def.m_sQuestId));
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! "garage.retrieved" when the garage hands a stored car out to its owner.
modded class LF_VehicleRetrieveAction
{
	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		string vehicleId;
		if (Replication.IsServer() && pUserEntity)
			vehicleId = LF_VehicleOwnership.LatestStored(LF_Utils.GetPlayerUID(pUserEntity));

		super.PerformAction(pOwnerEntity, pUserEntity);

		if (vehicleId.IsEmpty() || LF_VehicleOwnership.IsStored(vehicleId) || !LF_VehicleOwnership.GetVehicle(vehicleId))
			return;

		LF_QuestArrivalSignals.Report(pUserEntity, LF_QuestArrivalSignals.GARAGE_RETRIEVED);
		// After the report: the car's mark replaces the next errand's for its 30 s, then the errand's comes back.
		LF_VehicleHandover.Announce(pUserEntity, vehicleId);
	}
}
