//------------------------------------------------------------------------------------------------
//! A gate of the truck licence check course (WK-W21, issue #4695): cones and a pole the lent truck
//! drives through. Four of them stand on the course, each with an order index 0 to 3, and register in
//! LF_Places as kind TruckCheckGate under `truck_check#<order>`, so the course is found by component and
//! never by coordinates. The server watches the lent truck (LF_TruckCheckLoan) and, when it drives
//! through a gate, calls LF_DrivingCheck.ReachGate for that truck's driver. The rules there decide:
//! the next gate in order moves the run on, the last grants the licence, any other gate fails the run.
//!
//! The entry for the examiner and the truck yard cards:
//!   LF_DrivingCheck.StartFor / Start      the fee is taken and a run opens (already there)
//!   LF_TruckCheckLoan.Lend(uid, truck)    the truck the player was lent; the gates now watch it
//!   LF_TruckCheckLoan.Release(uid)        take the loan back (done by itself when the run ends)
//! Everything after that is the gates. A truck that is deleted mid-run fails the run.
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Feature/License", description: "A gate of the truck licence check course: the lent truck driving through it advances the check.")]
class LF_TruckCheckGateComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_TruckCheckGateComponent : ScriptComponent
{
	static const string PLACE_KIND = "TruckCheckGate";
	static const string COURSE_ID = "truck_check";

	[Attribute("-1", UIWidgets.Auto, desc: "Position on the course, 0 to 3. The truck passes the gates in this order; any other gate fails the check.")]
	protected int m_iOrder;

	[Attribute("6", UIWidgets.EditBox, desc: "Metres from the gate's origin that count as driving through it.")]
	protected float m_fRadius;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		Register(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! Puts this gate in LF_Places under its order. An order outside the course is refused.
	protected void Register(IEntity owner)
	{
		if (!owner)
			return;

		if (m_iOrder < 0)
		{
			LF_Debug.Warn(LF_DrivingCheck.FEATURE, string.Format("truck check gate on %1 has no order yet; the truck cannot pass it", owner.GetName()));
			return;
		}

		if (m_iOrder >= LF_DrivingCheck.GATE_COUNT)
		{
			LF_Debug.Error(LF_DrivingCheck.FEATURE, string.Format("truck check gate on %1 has order %2; the course has gates 0 to %3", owner.GetName(), m_iOrder, LF_DrivingCheck.GATE_COUNT - 1));
			return;
		}

		LF_Places.Register(PLACE_KIND, LF_Places.OrderedId(COURSE_ID, m_iOrder), owner);
	}

	//------------------------------------------------------------------------------------------------
	//! Sets this gate's order and radius after it was spawned (tests, a spawner), and registers it.
	//! A placed gate gets both from its prefab attributes instead.
	void Configure(int order, float radius)
	{
		IEntity owner = GetOwner();
		if (owner && m_iOrder >= 0 && m_iOrder < LF_DrivingCheck.GATE_COUNT)
		{
			IEntity held = LF_Places.Find(PLACE_KIND, LF_Places.OrderedId(COURSE_ID, m_iOrder));
			if (held == owner)
				LF_Places.Unregister(PLACE_KIND, LF_Places.OrderedId(COURSE_ID, m_iOrder));
		}

		m_iOrder = order;
		m_fRadius = radius;
		Register(owner);
	}

	//------------------------------------------------------------------------------------------------
	int GetOrder()
	{
		return m_iOrder;
	}

	//------------------------------------------------------------------------------------------------
	float GetRadius()
	{
		return m_fRadius;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: `vehicle` drove through this gate. When it is a truck lent for a check run, the run
	//! reaches this gate's order. A loan ends with the run: after a pass, a fail or a gate out of order.
	//! \return The check's answer: GATE_OK, PASSED, FAILED_COURSE; NO_RUN when the vehicle is not a
	//! lent truck of an open run; FAILED off the server.
	LF_EDrivingCheck ServerReach(IEntity vehicle)
	{
		if (!Replication.IsServer() || !vehicle)
		{
			LF_Debug.Warn(LF_DrivingCheck.FEATURE, string.Format("truck check gate %1 ignored: server %2, vehicle %3", m_iOrder, Replication.IsServer(), vehicle != null));
			return LF_EDrivingCheck.FAILED;
		}

		string uid = LF_TruckCheckLoan.OwnerOf(vehicle);
		if (uid.IsEmpty())
		{
			LF_Debug.Log(LF_DrivingCheck.FEATURE, string.Format("truck check gate %1: %2 is not a lent truck", m_iOrder, vehicle));
			return LF_EDrivingCheck.NO_RUN;
		}

		LF_EDrivingCheck outcome = LF_DrivingCheck.ReachGate(uid, m_iOrder);
		LF_Debug.Info(LF_DrivingCheck.FEATURE, string.Format("truck check gate %1: %2 reached it, the check says %3", m_iOrder, uid, typename.EnumToString(LF_EDrivingCheck, outcome)));
		if (outcome != LF_EDrivingCheck.GATE_OK)
			LF_TruckCheckLoan.Release(uid);

		// Say it in the driver's words: this gate, the pass, or the wrong gate. The lent row still names the player.
		string key = ToastKey(outcome);
		if (!key.IsEmpty())
			LF_Utils.NotifyEntity(LF_TruckCheckLend.PlayerOf(uid), key, "#LF-TruckCheck_Title", 4.0, ToastArg(outcome, m_iOrder));

		return outcome;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the toast for the check's answer at a gate; "" when there is nothing to say (no open run).
	static string ToastKey(LF_EDrivingCheck outcome)
	{
		switch (outcome)
		{
			case LF_EDrivingCheck.GATE_OK: return "#LF-TruckCheck_Gate";
			case LF_EDrivingCheck.PASSED: return "#LF-TruckCheck_Passed";
			case LF_EDrivingCheck.FAILED_COURSE: return "#LF-TruckCheck_FailedCourse";
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the toast's %1: the gate's number for a gate passed (1 to 4), the retake minutes for a failure.
	static string ToastArg(LF_EDrivingCheck outcome, int order)
	{
		if (outcome == LF_EDrivingCheck.GATE_OK)
			return (order + 1).ToString();

		if (outcome == LF_EDrivingCheck.FAILED_COURSE)
			return LF_DrivingCheck.RETAKE_MINUTES.ToString();

		return "";
	}
}
