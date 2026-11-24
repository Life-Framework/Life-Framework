//------------------------------------------------------------------------------------------------
//! The truck lent for a licence check run (WK-W21, issue #4695): which vehicle belongs to which open
//! run, and the server tick that notices it driving through a gate. The examiner (W21) calls
//! Lend after LF_DrivingCheck.Start; from then on the placed gates do the rest.
//!
//! Server memory only, like the run itself: a restart abandons both.
//------------------------------------------------------------------------------------------------
//! One lent truck: the vehicle, where it was at the last tick, and which gates it stood inside then
//! (bit per gate order), so a gate fires once on entering and never again while the truck sits in it.
class LF_TruckCheckLoanRow : Managed
{
	EntityID m_Vehicle;
	vector m_vLast;
	int m_iInside;
}

//------------------------------------------------------------------------------------------------
//! The repeating call's target (a call queue needs an instance).
class LF_TruckCheckClock
{
	//------------------------------------------------------------------------------------------------
	void Tick()
	{
		LF_TruckCheckLoan.Tick();
	}
}

//------------------------------------------------------------------------------------------------
class LF_TruckCheckLoan
{
	static const int TICK_MS = 250;	// lf-fixed: a lorry at 20 m/s moves 5 m a tick; the path test covers the rest

	protected static ref map<string, ref LF_TruckCheckLoanRow> s_mLoans;
	protected static ref LF_TruckCheckClock s_Clock;
	protected static bool s_bTicking;

	//------------------------------------------------------------------------------------------------
	protected static map<string, ref LF_TruckCheckLoanRow> Loans()
	{
		if (!s_mLoans)
			s_mLoans = new map<string, ref LF_TruckCheckLoanRow>();

		return s_mLoans;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: `vehicle` is the truck lent to `uid` for the open check run; the gates now watch it.
	//! Gates the truck already stands in do not count (it starts the course in front of gate 0).
	//! Refused with no open run, off the server, or when another player holds the truck.
	//! \return true when the loan is held.
	static bool Lend(string uid, IEntity vehicle)
	{
		if (!Replication.IsServer() || uid.IsEmpty() || !vehicle)
		{
			LF_Debug.Warn(LF_DrivingCheck.FEATURE, string.Format("truck loan refused: server %1, account '%2', vehicle %3", Replication.IsServer(), uid, vehicle != null));
			return false;
		}

		if (!LF_DrivingCheck.IsRunning(uid))
		{
			LF_Debug.Info(LF_DrivingCheck.FEATURE, string.Format("truck loan refused: %1 has no check run open", uid));
			return false;
		}

		string holder = OwnerOf(vehicle);
		if (!holder.IsEmpty() && holder != uid)
		{
			LF_Debug.Info(LF_DrivingCheck.FEATURE, string.Format("truck loan refused: the truck is already lent to %1", holder));
			return false;
		}

		LF_TruckCheckLoanRow row = new LF_TruckCheckLoanRow();
		row.m_Vehicle = vehicle.GetID();
		row.m_vLast = vehicle.GetOrigin();
		row.m_iInside = CrossedMask(row.m_vLast, row.m_vLast);
		Loans().Set(uid, row);
		StartTicking();
		LF_Debug.Info(LF_DrivingCheck.FEATURE, string.Format("truck loan: %1 was lent %2", uid, vehicle));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the loan ends (the run is over, or the truck went back). No-op without a loan.
	static void Release(string uid)
	{
		if (!Loans().Contains(uid))
			return;

		Loans().Remove(uid);
		LF_Debug.Info(LF_DrivingCheck.FEATURE, string.Format("truck loan: %1 gave the truck back", uid));
		if (Loans().IsEmpty())
			StopTicking();
	}

	//------------------------------------------------------------------------------------------------
	//! \return The account the vehicle is lent to, or "" when it is not a lent truck.
	static string OwnerOf(IEntity vehicle)
	{
		if (!vehicle || !s_mLoans)
			return "";

		EntityID id = vehicle.GetID();
		foreach (string uid, LF_TruckCheckLoanRow row : s_mLoans)
		{
			if (row.m_Vehicle == id)
				return uid;
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	static bool HasLoan(string uid)
	{
		return Loans().Contains(uid);
	}

	//------------------------------------------------------------------------------------------------
	//! Bit per gate order (0 to 3): the straight path from `from` to `to` came within that gate's radius.
	static int CrossedMask(vector from, vector to)
	{
		array<IEntity> gates = {};
		LF_Places.FindAll(LF_TruckCheckGateComponent.PLACE_KIND, gates);

		int mask;
		foreach (IEntity gate : gates)
		{
			LF_TruckCheckGateComponent component = LF_TruckCheckGateComponent.Cast(gate.FindComponent(LF_TruckCheckGateComponent));
			if (!component)
				continue;

			int order = component.GetOrder();
			if (order < 0 || order >= LF_DrivingCheck.GATE_COUNT)
				continue;

			if (LF_RaceCheckpointComponent.PathWithin(gate.GetOrigin(), component.GetRadius(), from, to))
				mask = mask | (1 << order);
		}

		return mask;
	}

	//------------------------------------------------------------------------------------------------
	//! Server tick: every lent truck against the gates.
	static void Tick()
	{
		if (!s_mLoans)
		{
			StopTicking();
			return;
		}

		array<string> accounts = {};
		foreach (string uid, LF_TruckCheckLoanRow row : s_mLoans)
			accounts.Insert(uid);

		foreach (string uid : accounts)
			TickLoan(uid);

		if (Loans().IsEmpty())
			StopTicking();
	}

	//------------------------------------------------------------------------------------------------
	protected static void TickLoan(string uid)
	{
		LF_TruckCheckLoanRow row = Loans().Get(uid);
		if (!row)
			return;

		if (!LF_DrivingCheck.IsRunning(uid))
		{
			Release(uid);
			return;
		}

		IEntity vehicle;
		if (GetGame() && GetGame().GetWorld())
			vehicle = GetGame().GetWorld().FindEntityByID(row.m_Vehicle);

		if (!vehicle)
		{
			LF_Debug.Info(LF_DrivingCheck.FEATURE, string.Format("truck loan: %1's truck is gone; the check fails", uid));
			LF_DrivingCheck.Fail(uid);
			Release(uid);
			return;
		}

		TickLoanAt(uid, vehicle, vehicle.GetOrigin());
	}

	//------------------------------------------------------------------------------------------------
	//! The truck of `uid` moved from where it was at the last tick to `to`. Each gate it newly drove
	//! through, in order, goes to the gate's server entry; the first answer that is not GATE_OK ends the run.
	//! One tick of TickLoan, with the position given, for tests.
	static void TickLoanAt(string uid, IEntity vehicle, vector to)
	{
		LF_TruckCheckLoanRow row = Loans().Get(uid);
		if (!row || !vehicle)
			return;

		int crossed = CrossedMask(row.m_vLast, to);
		int entered = crossed & ~row.m_iInside;
		row.m_iInside = crossed;
		row.m_vLast = to;
		if (entered == 0)
			return;

		for (int order = 0; order < LF_DrivingCheck.GATE_COUNT; order++)
		{
			if ((entered & (1 << order)) == 0)
				continue;

			IEntity gate = LF_Places.Find(LF_TruckCheckGateComponent.PLACE_KIND, LF_Places.OrderedId(LF_TruckCheckGateComponent.COURSE_ID, order));
			if (!gate)
				continue;

			LF_TruckCheckGateComponent component = LF_TruckCheckGateComponent.Cast(gate.FindComponent(LF_TruckCheckGateComponent));
			if (!component)
				continue;

			if (component.ServerReach(vehicle) != LF_EDrivingCheck.GATE_OK)
				return;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void StartTicking()
	{
		if (s_bTicking || !GetGame() || !GetGame().GetCallqueue())
			return;

		s_bTicking = true;
		if (!s_Clock)
			s_Clock = new LF_TruckCheckClock();

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
	static bool IsTicking()
	{
		return s_bTicking;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		StopTicking();
		s_mLoans = null;
		s_Clock = null;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_TruckCheckLoan's statics (LF_StaticReset finds it by reflection).
class LF_TruckCheckLoan_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_TruckCheckLoan";
	}

	override void Apply()
	{
		LF_TruckCheckLoan.ResetStatics();
	}
}
