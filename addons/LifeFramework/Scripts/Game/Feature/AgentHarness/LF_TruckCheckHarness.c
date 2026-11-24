//------------------------------------------------------------------------------------------------
//! Harness stand-in for driving the lent truck through the truck check gates (#4697 proof: the harness
//! cannot drive). The gates test the truck's path between 250 ms server ticks, so setting the truck's
//! transform along the course in short steps is enough:
//!   truckcheck <gate>[,<gate>...]   (server, behind the harness gate) moves the sending player's lent
//!                                   check truck (LF_TruckCheckLend) through the named gates, 0 to 3, in
//!                                   the order given, 3 m every 200 ms, from wherever it stands; replies
//!                                   `ok truckcheck points=<n> metres=<m>` at once, the drive runs on
//!                                   the server after the reply and the gates answer in the log.
//!   truckcheck licence              buys the sending player the driving licence through the licence desk's own
//!                                   server entry (LF_LicenceDesk.BuyDriving, fee included), for a newcomer with no
//!                                   ID card who is not offered the desk row; the truck yard sells to a driving licence
//!                                   holder with the truck licence. Replies `ok truckcheck licence`.
//! `truckcheck 0,1,2,3` is a clean run; `truckcheck 0,1` then `truckcheck 0` drives back into gate 0,
//! a gate out of order. HARNESS ONLY: Run refuses in any build without -scrDefine LF_HARNESS, on top of
//! the gate the RPC already passed (LF_DevGate.CheckHarnessRequest).
//------------------------------------------------------------------------------------------------
class LF_TruckCheckDrive : Managed
{
	EntityID m_Truck;
	ref array<vector> m_aPoints = {};
	int m_iNext;

	//------------------------------------------------------------------------------------------------
	//! One step: 3 m toward the next point, or the point itself when it is closer.
	void Step()
	{
		IEntity truck;
		if (GetGame() && GetGame().GetWorld())
			truck = GetGame().GetWorld().FindEntityByID(m_Truck);

		if (!truck || m_iNext >= m_aPoints.Count())
		{
			Finish();
			return;
		}

		vector from = truck.GetOrigin();
		vector toward = m_aPoints[m_iNext] - from;
		toward[1] = 0;
		float left = toward.Length();
		vector next = m_aPoints[m_iNext];
		if (left > LF_TruckCheckHarness.STEP_METRES)
			next = from + toward * (LF_TruckCheckHarness.STEP_METRES / left);
		else
			m_iNext = m_iNext + 1;

		LF_TruckCheckHarness.Place(truck, next);
		if (m_iNext >= m_aPoints.Count())
			Finish();
	}

	//------------------------------------------------------------------------------------------------
	protected void Finish()
	{
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(Step);

		LF_Debug.Info(LF_TruckCheckHarness.FEATURE, "truckcheck: the drive is over");
		LF_TruckCheckHarness.Forget(this);
	}
}

//------------------------------------------------------------------------------------------------
class LF_TruckCheckHarness
{
	static const string FEATURE = "AgentHarness";
	static const string VERB = "truckcheck";
	static const float STEP_METRES = 3;	// lf-fixed: harness stand-in; a path test per 250 ms tick covers the span of one to two steps
	static const int STEP_MS = 200;		// lf-fixed: harness stand-in, a little faster than the gates' tick
	static const float RIDE = 0.5;		// lf-fixed: metres above the ground the truck is set down (it settles onto its wheels)

	protected static ref array<ref LF_TruckCheckDrive> s_aDrives = {};

	//------------------------------------------------------------------------------------------------
	//! Pure: whether a harness tool string is this verb.
	static bool IsVerb(string tool)
	{
		return tool.StartsWith(VERB + " ");
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the gate orders of `words` ("0,1,2"), or an empty array when one is not a gate 0 to 3.
	static array<int> ParseGates(string words)
	{
		array<int> gates = {};
		array<string> parts = {};
		words.Split(",", parts, true);
		foreach (string part : parts)
		{
			int gate = part.ToInt();
			if (part != gate.ToString() || gate < 0 || gate >= LF_DrivingCheck.GATE_COUNT)
				return new array<int>();

			gates.Insert(gate);
		}

		return gates;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: start moving the sending player's lent truck through the named gates.
	//! \return "ok ..." or "error <reason>"
	static string Run(IEntity character, string tool)
	{
		if (!LF_DevGate.HarnessBuild())
		{
			LF_Debug.Warn(FEATURE, "truckcheck refused: not a LF_HARNESS build");
			return "error harness_only";
		}

		array<string> words = {};
		tool.Split(" ", words, true);
		if (words.Count() < 2)
			return "error usage_truckcheck_gates";

		if (words[1] == "licence")
		{
			LF_ELicenceDesk bought = LF_LicenceDesk.BuyDriving(character);
			if (bought != LF_ELicenceDesk.ISSUED && bought != LF_ELicenceDesk.ALREADY_HELD)
				return "error licence " + typename.EnumToString(LF_ELicenceDesk, bought);

			return "ok truckcheck licence";
		}

		array<int> gates = ParseGates(words[1]);
		if (gates.IsEmpty())
			return "error bad_gates " + words[1];

		string uid = LF_Utils.GetPlayerUid(character);
		EntityID truckId = LF_TruckCheckLend.TruckOf(uid);
		IEntity truck;
		if (truckId != EntityID.INVALID)
			truck = GetGame().GetWorld().FindEntityByID(truckId);

		if (!truck)
			return "error no_lent_truck";

		LF_TruckCheckDrive drive = new LF_TruckCheckDrive();
		drive.m_Truck = truckId;
		float metres;
		vector at = truck.GetOrigin();
		foreach (int order : gates)
		{
			IEntity gate = LF_Places.Find(LF_TruckCheckGateComponent.PLACE_KIND, LF_Places.OrderedId(LF_TruckCheckGateComponent.COURSE_ID, order));
			if (!gate)
				return "error no_gate " + order.ToString();

			drive.m_aPoints.Insert(gate.GetOrigin());
			metres = metres + vector.Distance(at, gate.GetOrigin());
			at = gate.GetOrigin();
		}

		s_aDrives.Insert(drive);
		GetGame().GetCallqueue().CallLater(drive.Step, STEP_MS, true);
		LF_Debug.Info(FEATURE, string.Format("truckcheck: %1's lent truck drives %2 point(s), %3 m", uid, gates.Count(), metres));
		return string.Format("ok truckcheck points=%1 metres=%2", gates.Count(), Math.Round(metres));
	}

	//------------------------------------------------------------------------------------------------
	//! Sets the truck on the ground at `spot`, still, keeping the way it faces.
	static void Place(notnull IEntity truck, vector spot)
	{
		vector previous = truck.GetOrigin();
		vector transform[4];
		truck.GetWorldTransform(transform);
		spot[1] = GetGame().GetWorld().GetSurfaceY(spot[0], spot[2]) + RIDE;
		transform[3] = spot;
		truck.SetWorldTransform(transform);
		truck.Update();
		// A replicated entity moved on the server stays where it was for the clients unless its node is told.
		RplComponent rpl = RplComponent.Cast(truck.FindComponent(RplComponent));
		if (rpl)
			rpl.ForceNodeMovement(previous);

		Physics physics = truck.GetPhysics();
		if (physics)
		{
			physics.SetVelocity(vector.Zero);
			physics.SetAngularVelocity(vector.Zero);
			physics.SetActive(ActiveState.ACTIVE);
		}
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		foreach (LF_TruckCheckDrive drive : s_aDrives)
		{
			if (GetGame() && GetGame().GetCallqueue())
				GetGame().GetCallqueue().Remove(drive.Step);
		}

		s_aDrives.Clear();
	}

	//------------------------------------------------------------------------------------------------
	static void Forget(LF_TruckCheckDrive drive)
	{
		int at = s_aDrives.Find(drive);
		if (at >= 0)
			s_aDrives.Remove(at);
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of the drives in progress (LF_StaticReset finds it by reflection).
class LF_TruckCheckHarness_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_TruckCheckHarness";
	}

	override void Apply()
	{
		LF_TruckCheckHarness.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
//! Client side of `truckcheck`: hands the words to the sending character's server request.
modded class LF_AgentHarnessVerbs
{
	override bool Handle(LF_AgentCommand cmd)
	{
		if (cmd.Verb != LF_TruckCheckHarness.VERB)
			return super.Handle(cmd);

		SCR_ChimeraCharacter character = LF_Utils.GetLocalCharacter();
		if (!character)
		{
			LF_AgentHarness.Answer(cmd.Id, "error", "no_character");
			return true;
		}

		character.LF_AskHarnessScanner(cmd.Id, LF_TruckCheckHarness.VERB + " " + cmd.Arg1);
		return true;
	}
}
