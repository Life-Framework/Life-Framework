//------------------------------------------------------------------------------------------------
//! The station car pool's board (#4352): where the drawn car stands. The bay is a point in the board's
//! own frame, so a pack places the board and its bay together; the car faces the bay's yaw.
[ComponentEditorProps(category: "LifeFramework/Police", description: "Police car pool board: officers draw their rank's car into the bay")]
class LF_PoliceCarPoolComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_PoliceCarPoolComponent : ScriptComponent
{
	[Attribute("0 0 6", UIWidgets.Coords, "Bay centre in the board's local frame (metres); the drawn car stands here")]
	protected vector m_vBayOffset;

	[Attribute("90", UIWidgets.Slider, "Heading of the drawn car relative to the board (degrees)", "-180 180 1")]
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
//! Police car pool (#4352): an officer on duty draws a car from the rank fleet table (`LF_RankFleet`) at
//! the pool board, and it stands in the board's bay. One car per officer: drawing again sends the last
//! one back to the pool while nobody sits in it. Server only; the drawn cars are session state (held out
//! of the world save, like the police marker cars) and service vehicles, so only an officer on duty opens one.
class LF_PoliceCarPool
{
	static const string FEATURE = "CarPool";
	static const string REFUSED_OFF_DUTY = "#LF-CarPool_OffDuty";
	static const string REFUSED_CAR_OUT = "#LF-CarPool_CarOut";
	static const string REFUSED_BAY_BLOCKED = "#LF-CarPool_BayBlocked";
	static const string REFUSED_TOO_SOON = "#LF-CarPool_TooSoon";
	static const string REFUSED_TOO_FAR = "#LF-CarPool_TooFar";
	static const string DRAWN = "#LF-CarPool_Drawn";
	//! Draws on the board's action list. The prefab carries this many slots; a slot past the table hides.
	static const int SLOTS = 4;
	//! Seconds between two draws by the same officer, so a held key cannot churn cars.
	static const int COOLDOWN_SECONDS = 10; // lf-fixed: an anti-spam guard on one officer's draws, not a server tunable
	//! A vehicle within this many metres of the bay blocks it.
	static const float BAY_CLEAR_METRES = 3.5;
	//! The officer must stand this close to the board.
	static const float REACH_METRES = 5; // lf-fixed: the board's interaction reach plus slack, a geometry bound
	protected static const float SETTLE_LIFT = 1.2;
	protected static const float SETTLE_REACH = 2.5; // lf-fixed: a geometry bound of the seat trace, not a tunable

	//! Account id to the car that officer drew (session only).
	protected static ref map<string, EntityID> s_mCars;
	protected static map<string, EntityID> LzCars()
	{
		if (!s_mCars)
			s_mCars = new map<string, EntityID>();
		return s_mCars;
	}
	//! Account id to the world time (ms) of that officer's last draw.
	protected static ref map<string, float> s_mLastDraw;
	protected static map<string, float> LzLastDraw()
	{
		if (!s_mLastDraw)
			s_mLastDraw = new map<string, float>();
		return s_mLastDraw;
	}
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
	//! The cars the board offers, one per slot, in ladder order (the lowest rank's car first, no repeats).
	static void Cars(notnull array<ResourceName> cars)
	{
		cars.Clear();
		int count = LF_PoliceRanks.RankCount();
		for (int rank = 0; rank < count; rank++)
		{
			ResourceName car = LF_RankFleet.CarFor(rank);
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
		LzCars();
		if (!s_mCars.Find(uid, id) || !GetGame() || !GetGame().GetWorld())
			return null;

		return GetGame().GetWorld().FindEntityByID(id);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: officer `uid` (body `officer`) draws the car on slot `slot` of `board`.
	//! \param[out] refusalKey Empty on a draw; else the localization key that says why not.
	//! \param[out] refusalArg The key's %1 (the rank a car needs), or empty.
	//! \return The car standing in the bay, or null when refused (logged).
	static IEntity Draw(string uid, IEntity officer, IEntity board, int slot, out string refusalKey, out string refusalArg)
	{
		refusalKey = string.Empty;
		refusalArg = string.Empty;
		if (!Replication.IsServer() || !board || !officer)
			return null;

		LF_PoliceCarPoolComponent pool = LF_Component<LF_PoliceCarPoolComponent>.Find(board);
		if (!pool)
		{
			LF_Debug.Error(FEATURE, "draw refused: the board has no LF_PoliceCarPoolComponent");
			refusalKey = LF_RankFleet.REFUSED_NOT_ISSUED;
			return null;
		}

		LF_PlayerAccount account;
		LF_PlayerAccountManager manager = LF_PlayerAccountManager.GetInstance();
		if (manager && !uid.IsEmpty())
			account = manager.GetAccount(uid);

		if (!account || account.GetFaction() != LF_Faction.POLICE || !account.IsOnDuty())
			return Refuse(REFUSED_OFF_DUTY, string.Format("draw refused: %1 is not police on duty", uid), refusalKey);

		if (vector.Distance(officer.GetOrigin(), board.GetOrigin()) > REACH_METRES)
			return Refuse(REFUSED_TOO_FAR, string.Format("draw refused: %1 is %2 m from the board", uid, vector.Distance(officer.GetOrigin(), board.GetOrigin())), refusalKey);

		float now = GetGame().GetWorld().GetWorldTime();
		float last;
		LzLastDraw();
		if (s_mLastDraw.Find(uid, last) && now - last < COOLDOWN_SECONDS * 1000)
			return Refuse(REFUSED_TOO_SOON, string.Format("draw refused: %1 drew a car %2 ms ago", uid, now - last), refusalKey);

		ResourceName car = SlotCar(slot);
		if (car.IsEmpty())
			return Refuse(LF_RankFleet.REFUSED_NOT_ISSUED, string.Format("draw refused: slot %1 has no car", slot), refusalKey);

		int rank = account.GetPoliceRank();
		string neededTitleKey;
		if (!LF_RankFleet.MayDraw(rank, car, refusalKey, neededTitleKey))
		{
			if (!neededTitleKey.IsEmpty())
				refusalArg = "#" + neededTitleKey;

			return null;
		}

		// One car per officer: the last one goes back to the pool, unless someone sits in it.
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
			// A service car opens for an officer on duty only, locked or not; it stands open for the one who drew it.
			vehicleLock.SetServiceVehicle(true);
			vehicleLock.SetLocked(false);
		}
		else
			LF_Debug.Warn(FEATURE, string.Format("%1 has no LF_VehicleLockComponent: the drawn car is not a service car", car));

		LF_VehicleWorldSave.Hold(spawned);
		LF_PlaceSnap.Settle(spawned, SETTLE_LIFT, SETTLE_REACH);
		LzCars();
		s_mCars.Set(uid, spawned.GetID());
		LzLastDraw();
		s_mLastDraw.Set(uid, now);
		LF_Debug.Info(FEATURE, string.Format("%1 (rank %2) drew %3 into the bay at %4", uid, rank, car, spawned.GetOrigin()));
		return spawned;
	}

	//------------------------------------------------------------------------------------------------
	//! Server, from the board's action: draws and tells the officer in a toast.
	static void DrawAndNotify(IEntity officer, IEntity board, int slot)
	{
		string uid = LF_PoliceRanks.ResolveUid(officer);
		if (uid.IsEmpty())
		{
			LF_Debug.Error(FEATURE, "draw refused: player has no account id");
			return;
		}

		string refusalKey;
		string refusalArg;
		IEntity car = Draw(uid, officer, board, slot, refusalKey, refusalArg);
		if (!GetGame().GetPlayerManager())
			return;

		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(officer);
		if (playerId <= 0)
			return;

		if (car)
			LF_NotificationManagerComponent.NotifyPlayer(playerId, "#EL-Police_Title", DRAWN, 3.0, LF_ENotificationType.INFO);
		else
			LF_NotificationManagerComponent.NotifyPlayer(playerId, "#EL-Police_Title", refusalKey, 3.0, LF_ENotificationType.WARNING, refusalArg);
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: forget `uid`'s cooldown so a second draw can follow at once.
	static void ClearCooldownForTest(string uid)
	{
		LzLastDraw();
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
//! One car on the pool board's action list. The prefab carries LF_PoliceCarPool.SLOTS of these, slot 0 to 3;
//! each names its car's rank ("Take the Cadet car") and hides when the fleet table has no car for it.
class LF_PoliceCarPoolDrawAction : ScriptedUserAction
{
	[Attribute("0", UIWidgets.EditBox, "Which car of the fleet table, in ladder order (0 = the lowest rank's car)")]
	protected int m_iSlot;

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!LF_Systems.IsEnabled("Police") || !GetOwner())
			return false;

		if (!LF_AccountStateBits.IsOnDutyPolice(LF_AccountReplication.GetState(user)))
			return false;

		return !LF_PoliceCarPool.SlotCar(m_iSlot).IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		int rank = LF_AccountStateBits.GetPoliceRank(LF_AccountReplication.GetState(user));
		string refusalKey;
		string neededTitleKey;
		if (LF_RankFleet.MayDraw(rank, LF_PoliceCarPool.SlotCar(m_iSlot), refusalKey, neededTitleKey))
			return true;

		if (!neededTitleKey.IsEmpty())
			SetCannotPerformReason(WidgetManager.Translate(refusalKey, WidgetManager.Translate("#" + neededTitleKey)));
		else
			SetCannotPerformReason(refusalKey);

		return false;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (LF_Systems.Refuses("Police", "LF_PoliceCarPoolDrawAction"))
			return;

		if (!Replication.IsServer())
			return;

		LF_PoliceCarPool.DrawAndNotify(pUserEntity, pOwnerEntity, m_iSlot);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		int needed = LF_RankFleet.RankNeeded(LF_PoliceCarPool.SlotCar(m_iSlot));
		if (needed < 0)
			return false;

		outName = WidgetManager.Translate("#LF-CarPool_Take", WidgetManager.Translate("#" + LF_PoliceRanks.TitleKey(needed)));
		return true;
	}
}

//------------------------------------------------------------------------------------------------
class LF_PoliceCarPoolStaticReset : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_PoliceCarPool";
	}

	override void Apply()
	{
		LF_PoliceCarPool.ResetStatics();
	}
}
