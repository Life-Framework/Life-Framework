//------------------------------------------------------------------------------------------------
//! The truck lent for the licence check (WK-W21, issue #4696): the examiner's "Take the truck check"
//! takes the fee (LF_DrivingCheck.Start), spawns the lent Ural at the place kind TruckCheckStart (or by
//! the examiner when no start place is placed yet), stands the player at its driver's door and hands
//! the truck to the gates (LF_TruckCheckLoan.Lend). A server tick then ends the check when the player
//! stays out of the truck for 60 s, the truck is wrecked or gone, or the player is 300 m away; the lent
//! truck is deleted when the run ends either way, passed or failed.
//!
//! The lent truck is never owned, never saved (no garage row, no persistence): it is session state
//! like the run. A restart abandons both.
//------------------------------------------------------------------------------------------------
enum LF_ETruckCheckLend
{
	//! The fee is taken, the truck is lent and the run is open.
	OK,
	RETAKE_WAIT,
	NOT_ENOUGH_CASH,
	ALREADY_HELD,
	ALREADY_RUNNING,
	//! The truck prefab did not spawn (the fee is given back).
	NO_TRUCK,
	FAILED
}

//------------------------------------------------------------------------------------------------
//! Why a run in progress ends on the tick.
enum LF_ETruckCheckEnd
{
	NONE,
	//! Out of the truck for LEFT_SECONDS.
	LEFT,
	//! The player is AWAY_METRES or more from the truck.
	AWAY,
	WRECKED
}

//------------------------------------------------------------------------------------------------
//! One lent truck: who holds it, and since when nobody has sat in it.
class LF_TruckCheckLendRow : Managed
{
	EntityID m_Truck = EntityID.INVALID;
	EntityID m_Player = EntityID.INVALID;
	float m_fEmptySince;
}

//------------------------------------------------------------------------------------------------
//! The repeating call's target (a call queue needs an instance).
class LF_TruckCheckLendClock
{
	//------------------------------------------------------------------------------------------------
	void Tick()
	{
		LF_TruckCheckLend.Tick();
	}
}

//------------------------------------------------------------------------------------------------
class LF_TruckCheckLend
{
	static const string PLACE_KIND = "TruckCheckStart";
	static const string PLACE_ID = "truck_check_start";
	static const ResourceName TRUCK_PREFAB = "{2B402516452EC797}Prefabs/Vehicles/Wheeled/Ural4320/LF_Ural4320_transport_CIV.et";
	static const float LEFT_SECONDS = 60;		// lf-fixed: how long a player may stay out of the lent truck
	static const float AWAY_METRES = 300;		// lf-fixed: how far a player may walk from the lent truck
	static const int TICK_MS = 1000;			// lf-fixed: the watcher's rate; a minute's limit needs no finer
	static const float FALLBACK_AHEAD = 6;		// lf-fixed: metres in front of the examiner when no start place is placed
	static const float FALLBACK_SIDE = 3;		// lf-fixed: metres to the examiner's right then

	protected static ref map<string, ref LF_TruckCheckLendRow> s_mLent;
	protected static ref LF_TruckCheckLendClock s_Clock;
	protected static bool s_bTicking;

	//------------------------------------------------------------------------------------------------
	protected static map<string, ref LF_TruckCheckLendRow> Lent()
	{
		if (!s_mLent)
			s_mLent = new map<string, ref LF_TruckCheckLendRow>();

		return s_mLent;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the player takes the truck check at the examiner. Refused with the rules' answer (held
	//! licence, run open, retake wait, short fee) and nothing lent. On OK the fee is taken, the run is
	//! open and the lent truck stands at its start with the player at the driver's door.
	//! \param examiner The examiner's entity; the fallback spawn is beside it when no start place exists.
	//! \param[out] minutesLeft In-game minutes until a retake (RETAKE_WAIT).
	static LF_ETruckCheckLend Take(IEntity user, IEntity examiner, out int minutesLeft)
	{
		minutesLeft = 0;
		if (!Replication.IsServer() || !user)
			return LF_ETruckCheckLend.FAILED;

		// A prefab that cannot load is found before the fee leaves the wallet.
		Resource resource = Resource.Load(TRUCK_PREFAB);
		if (!resource || !resource.IsValid())
		{
			LF_Debug.Error(LF_DrivingCheck.FEATURE, string.Format("truck check: the lent truck prefab does not load: %1", TRUCK_PREFAB));
			return LF_ETruckCheckLend.NO_TRUCK;
		}

		LF_EDrivingCheck started = LF_DrivingCheck.Start(user, minutesLeft);
		if (started != LF_EDrivingCheck.STARTED)
			return Refusal(started);

		string uid = LF_Utils.GetPlayerUID(user);
		vector transform[4];
		StartTransform(examiner, user, transform);
		IEntity truck = LF_Utils.SpawnEntityPrefab(TRUCK_PREFAB, transform[3], Math3D.MatrixToAngles(transform));
		if (!truck)
		{
			LF_Debug.Error(LF_DrivingCheck.FEATURE, string.Format("truck check: the lent truck did not spawn for %1; the fee is given back", uid));
			LF_DrivingCheck.Fail(uid);
			LF_MoneyUtils.AddCash(user, LF_DrivingCheck.FEE);
			return LF_ETruckCheckLend.NO_TRUCK;
		}

		// No free cars: the lent truck opens for the player on the check and nobody else.
		LF_VehicleLockComponent truckLock = LF_Component<LF_VehicleLockComponent>.Find(truck);
		if (truckLock)
			truckLock.SetJobDriver(LF_PlayerInteraction.PlayerIdOf(user));
		else
			LF_Debug.Warn(LF_DrivingCheck.FEATURE, string.Format("truck check: %1 has no LF_VehicleLockComponent: anyone can drive the lent truck", TRUCK_PREFAB));

		// Session state, not an owned vehicle: the world save must not keep it.
		LF_VehicleWorldSave.Hold(truck);

		if (!LF_TruckCheckLoan.Lend(uid, truck))
		{
			LF_Debug.Error(LF_DrivingCheck.FEATURE, string.Format("truck check: the gates refused the loan for %1; the check fails", uid));
			LF_DrivingCheck.Fail(uid);
			SCR_EntityHelper.DeleteEntityAndChildren(truck);
			return LF_ETruckCheckLend.FAILED;
		}

		LF_TruckCheckLendRow row = new LF_TruckCheckLendRow();
		row.m_Truck = truck.GetID();
		row.m_Player = user.GetID();
		row.m_fEmptySince = LF_JobGuards.Now();
		Lent().Set(uid, row);
		StartTicking();

		LF_Jail.MoveBody(user, DoorOf(truck));
		LF_Debug.Info(LF_DrivingCheck.FEATURE, string.Format("truck check: %1 was lent %2 at the start", uid, truck));
		return LF_ETruckCheckLend.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! The rules' refusal as this class's answer.
	protected static LF_ETruckCheckLend Refusal(LF_EDrivingCheck started)
	{
		switch (started)
		{
			case LF_EDrivingCheck.RETAKE_WAIT: return LF_ETruckCheckLend.RETAKE_WAIT;
			case LF_EDrivingCheck.NOT_ENOUGH_CASH: return LF_ETruckCheckLend.NOT_ENOUGH_CASH;
			case LF_EDrivingCheck.ALREADY_HELD: return LF_ETruckCheckLend.ALREADY_HELD;
			case LF_EDrivingCheck.ALREADY_RUNNING: return LF_ETruckCheckLend.ALREADY_RUNNING;
		}

		return LF_ETruckCheckLend.FAILED;
	}

	//------------------------------------------------------------------------------------------------
	//! Where the lent truck stands: the placed start (by kind), else beside the examiner.
	protected static void StartTransform(IEntity examiner, IEntity user, out vector transform[4])
	{
		IEntity start = LF_Places.Find(PLACE_KIND, PLACE_ID);
		if (start)
		{
			start.GetWorldTransform(transform);
			return;
		}

		IEntity anchor = examiner;
		if (!anchor)
			anchor = user;

		LF_Debug.Warn(LF_DrivingCheck.FEATURE, "truck check: no TruckCheckStart place is placed; the truck stands beside the examiner");
		anchor.GetWorldTransform(transform);
		vector at = anchor.GetOrigin() + anchor.GetTransformAxis(2) * FALLBACK_AHEAD + anchor.GetTransformAxis(0) * FALLBACK_SIDE;
		transform[3] = at;
	}

	//------------------------------------------------------------------------------------------------
	//! The ground beside the cab's driver door (left of a left-hand-drive Ural), where the player is put.
	protected static vector DoorOf(notnull IEntity truck)
	{
		vector door = truck.GetOrigin() - truck.GetTransformAxis(0) * 2.5 + truck.GetTransformAxis(2) * 1.5;
		door[1] = GetGame().GetWorld().GetSurfaceY(door[0], door[2]);
		return door;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure rule: does a run in progress end now?
	//! \param emptySeconds How long nobody has sat in the truck.
	//! \param awayMetres The player's distance from the truck.
	//! \param wrecked The truck is destroyed or deleted.
	static LF_ETruckCheckEnd Judge(float emptySeconds, float awayMetres, bool wrecked)
	{
		if (wrecked)
			return LF_ETruckCheckEnd.WRECKED;

		if (emptySeconds >= LEFT_SECONDS)
			return LF_ETruckCheckEnd.LEFT;

		if (awayMetres >= AWAY_METRES)
			return LF_ETruckCheckEnd.AWAY;

		return LF_ETruckCheckEnd.NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The lent truck of `uid`, or EntityID.INVALID.
	static EntityID TruckOf(string uid)
	{
		LF_TruckCheckLendRow row = Lent().Get(uid);
		if (!row)
			return EntityID.INVALID;

		return row.m_Truck;
	}

	//------------------------------------------------------------------------------------------------
	static int LentCount()
	{
		return Lent().Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Server tick: every lent truck against the rules; a run that ended elsewhere (the gates passed or
	//! failed it) takes its truck back.
	static void Tick()
	{
		if (!s_mLent)
		{
			StopTicking();
			return;
		}

		array<string> accounts = {};
		foreach (string uid, LF_TruckCheckLendRow row : s_mLent)
			accounts.Insert(uid);

		foreach (string uid : accounts)
			TickOne(uid, LF_JobGuards.Now());

		if (Lent().IsEmpty())
			StopTicking();
	}

	//------------------------------------------------------------------------------------------------
	protected static void TickOne(string uid, float now)
	{
		LF_TruckCheckLendRow row = Lent().Get(uid);
		if (!row)
			return;

		if (!LF_DrivingCheck.IsRunning(uid))
		{
			Return(uid);
			return;
		}

		IEntity truck;
		IEntity player;
		if (GetGame() && GetGame().GetWorld())
		{
			truck = GetGame().GetWorld().FindEntityByID(row.m_Truck);
			player = GetGame().GetWorld().FindEntityByID(row.m_Player);
		}

		bool wrecked = !truck || IsWrecked(truck);
		float away = 0;
		if (truck && player)
			away = vector.Distance(truck.GetOrigin(), player.GetOrigin());
		else
			away = AWAY_METRES;

		if (truck && LF_StolenVehicles.HasOccupant(truck))
			row.m_fEmptySince = now;

		LF_ETruckCheckEnd end = Judge(now - row.m_fEmptySince, away, wrecked);
		if (end == LF_ETruckCheckEnd.NONE)
			return;

		LF_Debug.Info(LF_DrivingCheck.FEATURE, string.Format("truck check: %1's check ends (%2): the truck goes back", uid, typename.EnumToString(LF_ETruckCheckEnd, end)));
		LF_DrivingCheck.Fail(uid);
		if (player)
			LF_Utils.NotifyEntity(player, EndKey(end), "#LF-TruckCheck_Title", 6.0, LF_DrivingCheck.RETAKE_MINUTES.ToString());

		Return(uid);
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsWrecked(notnull IEntity truck)
	{
		DamageManagerComponent damage = DamageManagerComponent.Cast(truck.FindComponent(DamageManagerComponent));
		return damage && damage.GetState() == EDamageState.DESTROYED;
	}

	//------------------------------------------------------------------------------------------------
	//! The run is over: the truck is deleted (a seated player is stood up with it) and the loan released.
	protected static void Return(string uid)
	{
		LF_TruckCheckLendRow row = Lent().Get(uid);
		if (!row)
			return;

		Lent().Remove(uid);
		LF_TruckCheckLoan.Release(uid);
		if (GetGame() && GetGame().GetWorld())
		{
			IEntity truck = GetGame().GetWorld().FindEntityByID(row.m_Truck);
			if (truck)
				SCR_EntityHelper.DeleteEntityAndChildren(truck);
		}

		LF_Debug.Info(LF_DrivingCheck.FEATURE, string.Format("truck check: %1's lent truck is deleted", uid));
	}

	//------------------------------------------------------------------------------------------------
	static string AnswerKey(LF_ETruckCheckLend outcome)
	{
		switch (outcome)
		{
			case LF_ETruckCheckLend.OK: return "#LF-TruckCheck_Started";
			case LF_ETruckCheckLend.RETAKE_WAIT: return "#LF-TruckCheck_RetakeWait";
			case LF_ETruckCheckLend.NOT_ENOUGH_CASH: return "#LF-TruckCheck_NoCash";
			case LF_ETruckCheckLend.ALREADY_HELD: return "#LF-TruckCheck_Held";
			case LF_ETruckCheckLend.ALREADY_RUNNING: return "#LF-TruckCheck_Running";
		}

		return "#LF-TruckCheck_Unavailable";
	}

	//------------------------------------------------------------------------------------------------
	//! The toast's %1: the minutes of the wait, or the fee.
	static string AnswerArg(LF_ETruckCheckLend outcome, int minutesLeft)
	{
		if (outcome == LF_ETruckCheckLend.RETAKE_WAIT)
			return minutesLeft.ToString();

		if (outcome == LF_ETruckCheckLend.NOT_ENOUGH_CASH)
			return LF_DrivingCheck.FEE.ToString();

		return "";
	}

	//------------------------------------------------------------------------------------------------
	static string EndKey(LF_ETruckCheckEnd end)
	{
		switch (end)
		{
			case LF_ETruckCheckEnd.LEFT: return "#LF-TruckCheck_FailedLeft";
			case LF_ETruckCheckEnd.AWAY: return "#LF-TruckCheck_FailedAway";
		}

		return "#LF-TruckCheck_FailedWreck";
	}

	//------------------------------------------------------------------------------------------------
	protected static void StartTicking()
	{
		if (s_bTicking || !GetGame() || !GetGame().GetCallqueue())
			return;

		s_bTicking = true;
		if (!s_Clock)
			s_Clock = new LF_TruckCheckLendClock();

		GetGame().GetCallqueue().CallLater(s_Clock.Tick, TICK_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	protected static void StopTicking()
	{
		if (!s_bTicking)
			return;

		s_bTicking = false;
		if (GetGame() && GetGame().GetCallqueue() && s_Clock)
			GetGame().GetCallqueue().Remove(s_Clock.Tick);
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		StopTicking();
		s_mLent = null;
		s_Clock = null;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_TruckCheckLend's statics (LF_StaticReset finds it by reflection).
class LF_TruckCheckLend_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_TruckCheckLend";
	}

	override void Apply()
	{
		LF_TruckCheckLend.ResetStatics();
	}
}
