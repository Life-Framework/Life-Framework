//------------------------------------------------------------------------------------------------
//! The clinic car pool board's bay (#4432): where the drawn ambulance stands. The bay is a point in the
//! board's own frame, so a pack places the board and its bay together; the car faces the bay's yaw.
[ComponentEditorProps(category: "LifeFramework/Medic", description: "Medic car pool board: medics draw their rank's ambulance into the bay")]
class LF_MedicCarPoolComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_MedicCarPoolComponent : ScriptComponent
{
	[Attribute("0 0 6", UIWidgets.Coords, "Bay centre in the board's local frame (metres); the drawn ambulance stands here")]
	protected vector m_vBayOffset;

	[Attribute("90", UIWidgets.Slider, "Heading of the drawn ambulance relative to the board (degrees)", "-180 180 1")]
	protected float m_fBayYaw;

	//------------------------------------------------------------------------------------------------
	//! The bay as a world transform: origin at the bay, the board's yaw plus m_fBayYaw.
	void BayTransform(out vector transform[4])
	{
		IEntity owner = GetOwner();
		owner.GetWorldTransform(transform);
		vector at = owner.CoordToParent(m_vBayOffset);
		vector angles = Math3D.MatrixToAngles(transform);
		angles[0] = angles[0] + m_fBayYaw;
		Math3D.AnglesToMatrix(angles, transform);
		transform[3] = at;
	}
}

//------------------------------------------------------------------------------------------------
//! Medic car pool (#4432): a medic on duty draws an ambulance from the medic fleet table
//! (`LF_MedicFleet`) at the clinic's board, and it stands in the board's bay. One ambulance per medic: drawing
//! again sends the last one back while nobody sits in it. Server only; the drawn cars are session state (held
//! out of the world save) and service vehicles, so only a medic on duty opens one. Same shape and refusal
//! words as the police pool (`LF_PoliceCarPool`).
class LF_MedicCarPool
{
	static const string FEATURE = "MedicCarPool";
	//! The refusal and draw words are the police pool's (their text is neutral): one string row each.
	static const string REFUSED_OFF_DUTY = "#LF-CarPool_OffDuty";
	static const string REFUSED_CAR_OUT = "#LF-CarPool_CarOut";
	static const string REFUSED_BAY_BLOCKED = "#LF-CarPool_BayBlocked";
	static const string REFUSED_TOO_SOON = "#LF-CarPool_TooSoon";
	static const string REFUSED_TOO_FAR = "#LF-CarPool_TooFar";
	static const string DRAWN = "#LF-CarPool_Drawn";
	static const string TITLE = "#LF-MedicFleet_Title";
	//! Draws on the board's action list. The prefab carries this many slots; a slot past the table hides.
	static const int SLOTS = 3;
	//! Seconds between two draws by the same medic, so a held key cannot churn cars.
	static const int COOLDOWN_SECONDS = 10; // lf-fixed: an anti-spam guard on one medic's draws, not a server tunable
	//! A vehicle within this many metres of the bay blocks it.
	static const float BAY_CLEAR_METRES = 4.0;
	//! The medic must stand this close to the board.
	static const float REACH_METRES = 5; // lf-fixed: the board's interaction reach plus slack, a geometry bound
	protected static const float SETTLE_LIFT = 1.2;
	protected static const float SETTLE_REACH = 2.5; // lf-fixed: a geometry bound of the seat trace, not a tunable

	//! Account id to the car that medic drew (session only).
	protected static ref map<string, EntityID> s_mCars = new map<string, EntityID>();
	//! Account id to the world time (ms) of that medic's last draw.
	protected static ref map<string, float> s_mLastDraw = new map<string, float>();
	protected static int s_iBayHits;
	protected static EntityID s_IgnoreId;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mCars = new map<string, EntityID>();
		s_mLastDraw = new map<string, float>();
		s_iBayHits = 0;
		s_IgnoreId = EntityID.INVALID;
	}

	//------------------------------------------------------------------------------------------------
	//! The ambulances the board offers, one per slot, in ladder order (the lowest rank's first, no repeats).
	static void Cars(notnull array<ResourceName> cars)
	{
		cars.Clear();
		int count = LF_MedicRanks.RankCount();
		for (int rank = 0; rank < count; rank++)
		{
			ResourceName car = LF_MedicFleet.CarFor(rank);
			if (car.IsEmpty())
				continue;

			bool seen;
			foreach (ResourceName have : cars)
			{
				if (LF_Utils.SamePrefab(have, car))
					seen = true;
			}

			if (!seen)
				cars.Insert(car);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The car on board slot `slot`, or empty when the table has fewer cars.
	static ResourceName SlotCar(int slot)
	{
		array<ResourceName> cars = {};
		Cars(cars);
		if (slot < 0 || slot >= cars.Count())
			return ResourceName.Empty;

		return cars[slot];
	}

	//------------------------------------------------------------------------------------------------
	//! \return The car `uid` drew and still has out, or null.
	static IEntity CarOf(string uid)
	{
		EntityID id;
		if (!s_mCars.Find(uid, id) || !GetGame() || !GetGame().GetWorld())
			return null;

		return GetGame().GetWorld().FindEntityByID(id);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: medic `uid` (body `medic`) draws the ambulance on slot `slot` of `board`.
	//! \param[out] refusalKey Empty on a draw; else the localization key that says why not.
	//! \param[out] refusalArg The key's %1 (the rank a car needs), or empty.
	//! \return The car standing in the bay, or null when refused (logged).
	static IEntity Draw(string uid, IEntity medic, IEntity board, int slot, out string refusalKey, out string refusalArg)
	{
		refusalKey = string.Empty;
		refusalArg = string.Empty;
		if (!Replication.IsServer() || !board || !medic)
			return null;

		LF_MedicCarPoolComponent pool = LF_Component<LF_MedicCarPoolComponent>.Find(board);
		if (!pool)
		{
			LF_Debug.Error(FEATURE, "draw refused: the board has no LF_MedicCarPoolComponent");
			refusalKey = LF_RankFleet.REFUSED_NOT_ISSUED;
			return null;
		}

		LF_PlayerAccount account;
		LF_PlayerAccountManager manager = LF_PlayerAccountManager.GetInstance();
		if (manager && !uid.IsEmpty())
			account = manager.GetAccount(uid);

		if (!LF_MedicDuty.IsOnDuty(account))
			return Refuse(REFUSED_OFF_DUTY, string.Format("draw refused: %1 is not a medic on duty", uid), refusalKey);

		float away = vector.Distance(medic.GetOrigin(), board.GetOrigin());
		if (away > REACH_METRES)
			return Refuse(REFUSED_TOO_FAR, string.Format("draw refused: %1 is %2 m from the board", uid, away), refusalKey);

		float now = GetGame().GetWorld().GetWorldTime();
		float last;
		if (s_mLastDraw.Find(uid, last) && now - last < COOLDOWN_SECONDS * 1000)
			return Refuse(REFUSED_TOO_SOON, string.Format("draw refused: %1 drew a car %2 ms ago", uid, now - last), refusalKey);

		ResourceName car = SlotCar(slot);
		if (car.IsEmpty())
			return Refuse(LF_RankFleet.REFUSED_NOT_ISSUED, string.Format("draw refused: slot %1 has no car", slot), refusalKey);

		int rank = account.GetMedicRank();
		string neededTitleKey;
		if (!LF_MedicFleet.MayDraw(rank, car, refusalKey, neededTitleKey))
		{
			if (!neededTitleKey.IsEmpty())
				refusalArg = "#" + neededTitleKey;

			return null;
		}

		// One ambulance per medic: the last one goes back to the pool, unless someone sits in it.
		IEntity previous = CarOf(uid);
		if (previous && LF_Garage.HasOccupants(previous))
			return Refuse(REFUSED_CAR_OUT, string.Format("draw refused: %1's last car is still out with people in it", uid), refusalKey);

		vector transform[4];
		pool.BayTransform(transform);
		EntityID previousId = EntityID.INVALID;
		if (previous)
			previousId = previous.GetID();

		if (BayBlocked(transform[3], previousId))
			return Refuse(REFUSED_BAY_BLOCKED, string.Format("draw refused: a vehicle stands in the bay at %1", transform[3]), refusalKey);

		if (previous)
		{
			LF_Debug.Info(FEATURE, string.Format("%1's last car went back to the pool", uid));
			SCR_EntityHelper.DeleteEntityAndChildren(previous);
		}

		IEntity spawned = LF_Utils.SpawnEntityPrefab(car, transform[3] + "0 0.3 0", Math3D.MatrixToAngles(transform));
		if (!spawned)
		{
			LF_Debug.Error(FEATURE, string.Format("draw failed: %1 did not spawn", car));
			refusalKey = LF_RankFleet.REFUSED_NOT_ISSUED;
			return null;
		}

		LF_VehicleLockComponent vehicleLock = LF_Component<LF_VehicleLockComponent>.Find(spawned);
		if (vehicleLock)
		{
			// A service car opens for a medic on duty only, locked or not; it stands open for the one who drew it.
			vehicleLock.SetServiceVehicle(true);
			vehicleLock.SetLocked(false);
		}
		else
			LF_Debug.Warn(FEATURE, string.Format("%1 has no LF_VehicleLockComponent: the drawn car is not a service car", car));

		LF_VehicleWorldSave.Hold(spawned);
		LF_PlaceSnap.Settle(spawned, SETTLE_LIFT, SETTLE_REACH);
		s_mCars.Set(uid, spawned.GetID());
		s_mLastDraw.Set(uid, now);
		LF_Debug.Info(FEATURE, string.Format("%1 (rank %2) drew %3 into the bay at %4", uid, rank, car, spawned.GetOrigin()));
		return spawned;
	}

	//------------------------------------------------------------------------------------------------
	//! Server, from the board's action: draws and tells the medic in a toast.
	static void DrawAndNotify(IEntity medic, IEntity board, int slot)
	{
		string uid = LF_PoliceRanks.ResolveUid(medic);
		if (uid.IsEmpty())
		{
			LF_Debug.Error(FEATURE, "draw refused: player has no account id");
			return;
		}

		string refusalKey;
		string refusalArg;
		IEntity car = Draw(uid, medic, board, slot, refusalKey, refusalArg);
		if (!GetGame().GetPlayerManager())
			return;

		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(medic);
		if (playerId <= 0)
			return;

		if (car)
			LF_NotificationManagerComponent.NotifyPlayer(playerId, TITLE, DRAWN, 3.0, LF_ENotificationType.INFO);
		else
			LF_NotificationManagerComponent.NotifyPlayer(playerId, TITLE, refusalKey, 3.0, LF_ENotificationType.WARNING, refusalArg);
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: forget `uid`'s cooldown so a second draw can follow at once.
	static void ClearCooldownForTest(string uid)
	{
		s_mLastDraw.Remove(uid);
	}

	//------------------------------------------------------------------------------------------------
	protected static IEntity Refuse(string key, string why, out string refusalKey)
	{
		refusalKey = key;
		LF_Debug.Warn(FEATURE, why);
		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool BayBlocked(vector at, EntityID ignore)
	{
		s_iBayHits = 0;
		s_IgnoreId = ignore;
		GetGame().GetWorld().QueryEntitiesBySphere(at, BAY_CLEAR_METRES, CollectVehicle, null, EQueryEntitiesFlags.DYNAMIC);
		return s_iBayHits > 0;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool CollectVehicle(IEntity entity)
	{
		if (Vehicle.Cast(entity) && entity.GetID() != s_IgnoreId)
		{
			s_iBayHits++;
			return false;
		}

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! One ambulance on the pool board's action list. The prefab carries LF_MedicCarPool.SLOTS of these, slot 0
//! to 2; each names its car's rank ("Take the Orderly ambulance") and hides when the fleet table has no car for it.
class LF_MedicCarPoolDrawAction : ScriptedUserAction
{
	[Attribute("0", UIWidgets.EditBox, "Which car of the fleet table, in ladder order (0 = the lowest rank's car)")]
	protected int m_iSlot;

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!LF_Systems.IsEnabled("Medic") || !GetOwner())
			return false;

		if (!LF_MedicDuty.IsOnDutyState(LF_AccountReplication.GetState(user)))
			return false;

		// The medic rank rides in the replicated account state (#4566): a slot above the rank is not offered.
		// The server's draw checks the real account again and refuses in words.
		int rank = LF_AccountStateBits.GetMedicRank(LF_AccountReplication.GetState(user));
		string refusalKey;
		string neededTitleKey;
		return LF_MedicFleet.MayDraw(rank, LF_MedicCarPool.SlotCar(m_iSlot), refusalKey, neededTitleKey);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (LF_Systems.Refuses("Medic", "LF_MedicCarPoolDrawAction"))
			return;

		if (!Replication.IsServer())
			return;

		LF_MedicCarPool.DrawAndNotify(pUserEntity, pOwnerEntity, m_iSlot);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		int needed = LF_MedicFleet.RankNeeded(LF_MedicCarPool.SlotCar(m_iSlot));
		if (needed < 0)
			return false;

		outName = WidgetManager.Translate("#LF-MedicFleet_Take", WidgetManager.Translate("#" + LF_MedicRanks.TitleKey(needed)));
		return true;
	}
}

//------------------------------------------------------------------------------------------------
class LF_MedicCarPoolStaticReset : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_MedicCarPool";
	}

	override void Apply()
	{
		LF_MedicCarPool.ResetStatics();
	}
}
