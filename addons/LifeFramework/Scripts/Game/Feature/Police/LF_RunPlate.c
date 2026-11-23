//------------------------------------------------------------------------------------------------
//! "Run plate" (#4425, PO-hotlist): an on-duty officer behind the wheel of a police car reads the plate
//! of the car ahead and the answer comes back on the officer's pager: STOLEN, CLEAR or UNKNOWN, with the
//! plate. The action is offered by the car, the server does the rest: it re-checks the duty against the
//! real account and the driver's seat, finds the car ahead itself (no plate string ever comes from a
//! client) and asks the hotlist (LF_StolenVehicles.CheckPlate, #414/#4212).
//!
//! A pager line is the board entry "time|R;<plate>;<STATUS>|0|place"; the pager card renders it as
//! "Plate <plate>: <STATUS>" (the modded LF_Pager.BuildParts below turns it into a board line on the
//! client, so the plain text stays translatable on each client).
//------------------------------------------------------------------------------------------------
enum LF_ERunPlate
{
	//! The officer is not on duty (the real account says so).
	OFF_DUTY,
	//! The officer is not in the driver's seat of the car.
	NOT_DRIVING,
	//! No car in front within range.
	NO_CAR,
	STOLEN,
	CLEAR,
	UNKNOWN
}

//------------------------------------------------------------------------------------------------
//! The pure rules.
class LF_RunPlateRules
{
	//! How far ahead the plate is read, in metres.
	static const float RANGE = 30;	// lf-fixed: the card's 30 m reach (PO-hotlist)
	//! A car this close is the police car itself or its neighbour at the kerb, not "in front".
	static const float MIN_AHEAD = 1.5;	// lf-fixed: a car length is more than this; below it is our own car
	//! The cone: a car this far to the side per metre ahead is still "in front" (about 19 degrees).
	static const float SIDE_PER_METRE = 0.35;	// lf-fixed: one lane at the far end of the reach
	//! The lane the cone starts from, in metres either side of the car's centre line.
	static const float LANE_HALF = 2.5;	// lf-fixed: half a road lane plus a car's half width

	//------------------------------------------------------------------------------------------------
	//! Is target in front of a car at origin facing forward (horizontal), within RANGE?
	static bool IsAhead(vector origin, vector forward, vector target)
	{
		vector rel = target - origin;
		rel[1] = 0;
		vector dir = forward;
		dir[1] = 0;
		if (dir.LengthSq() < 0.0001)
			return false;

		dir.Normalize();
		float along = vector.Dot(rel, dir);
		if (along < MIN_AHEAD || along > RANGE)
			return false;

		vector side = rel - dir * along;
		return side.Length() <= LANE_HALF + along * SIDE_PER_METRE;
	}

	//------------------------------------------------------------------------------------------------
	//! The outcome from the three checks the server makes in order.
	static LF_ERunPlate Decide(bool onDuty, bool driving, bool carAhead, LF_EPlateStatus status)
	{
		if (!onDuty)
			return LF_ERunPlate.OFF_DUTY;

		if (!driving)
			return LF_ERunPlate.NOT_DRIVING;

		if (!carAhead)
			return LF_ERunPlate.NO_CAR;

		if (status == LF_EPlateStatus.STOLEN)
			return LF_ERunPlate.STOLEN;

		if (status == LF_EPlateStatus.CLEAR)
			return LF_ERunPlate.CLEAR;

		return LF_ERunPlate.UNKNOWN;
	}

	//------------------------------------------------------------------------------------------------
	//! The refusal in words, empty for an answer.
	static string RefusalKey(LF_ERunPlate outcome)
	{
		switch (outcome)
		{
			case LF_ERunPlate.OFF_DUTY: return "#LF-RunPlate_OffDuty";
			case LF_ERunPlate.NOT_DRIVING: return "#LF-RunPlate_NotDriving";
			case LF_ERunPlate.NO_CAR: return "#LF-RunPlate_NoCar";
		}

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! The status word of an answer, empty for a refusal.
	static string WordKey(LF_ERunPlate outcome)
	{
		switch (outcome)
		{
			case LF_ERunPlate.STOLEN: return "#LF-RunPlate_Stolen";
			case LF_ERunPlate.CLEAR: return "#LF-RunPlate_Clear";
			case LF_ERunPlate.UNKNOWN: return "#LF-RunPlate_Unknown";
		}

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsAnswer(LF_ERunPlate outcome)
	{
		return outcome == LF_ERunPlate.STOLEN || outcome == LF_ERunPlate.CLEAR || outcome == LF_ERunPlate.UNKNOWN;
	}
}

//------------------------------------------------------------------------------------------------
class LF_RunPlate
{
	static const string FEATURE = "RunPlate";
	static const string SUMMARY_PREFIX = "R;";
	static const string SUMMARY_SEPARATOR = ";";
	static const string FIELD_SEPARATOR = "|";

	//------------------------------------------------------------------------------------------------
	//! Pure: the summary field of a pager answer.
	static string Summary(string plate, LF_ERunPlate outcome)
	{
		return SUMMARY_PREFIX + plate + SUMMARY_SEPARATOR + typename.EnumToString(LF_ERunPlate, outcome);
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: is this stored pager message a plate answer?
	static bool IsEntry(string entry)
	{
		array<string> fields = {};
		entry.Split(FIELD_SEPARATOR, fields, false);
		return fields.Count() >= 4 && fields[1].StartsWith(SUMMARY_PREFIX);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: a plate answer as a plain board entry ("time|text|0|place") that the pager card shows
	//! as its own line. Any other message, and a malformed answer, comes back unchanged.
	static string ToBoardEntry(string entry)
	{
		if (!IsEntry(entry))
			return entry;

		array<string> fields = {};
		entry.Split(FIELD_SEPARATOR, fields, false);
		array<string> summary = {};
		fields[1].Split(SUMMARY_SEPARATOR, summary, false);
		if (summary.Count() < 3)
			return entry;

		string plate = summary[1];
		if (plate.IsEmpty())
			plate = WidgetManager.Translate("#LF-Pager_NoPlate");

		string word = WordKeyOfName(summary[2]);
		if (word.IsEmpty())
			return entry;

		string line = WidgetManager.Translate("#LF-RunPlate_Line", plate, WidgetManager.Translate(word));
		string place = fields[3];
		for (int i = 4; i < fields.Count(); i++)
		{
			place += FIELD_SEPARATOR + fields[i];
		}

		return fields[0] + FIELD_SEPARATOR + line + FIELD_SEPARATOR + "0" + FIELD_SEPARATOR + place;
	}

	//------------------------------------------------------------------------------------------------
	protected static string WordKeyOfName(string name)
	{
		if (name == typename.EnumToString(LF_ERunPlate, LF_ERunPlate.STOLEN))
			return LF_RunPlateRules.WordKey(LF_ERunPlate.STOLEN);

		if (name == typename.EnumToString(LF_ERunPlate, LF_ERunPlate.CLEAR))
			return LF_RunPlateRules.WordKey(LF_ERunPlate.CLEAR);

		if (name == typename.EnumToString(LF_ERunPlate, LF_ERunPlate.UNKNOWN))
			return LF_RunPlateRules.WordKey(LF_ERunPlate.UNKNOWN);

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the nearest vehicle in front of policeCar within range, else null. The server finds it
	//! from the car's own pose; the client names nothing.
	static IEntity FindAhead(IEntity policeCar)
	{
		if (!policeCar)
			return null;

		vector origin = policeCar.GetOrigin();
		vector forward = policeCar.GetTransformAxis(2);
		array<IEntity> near = LF_Utils.GetNearbyVehicles(origin, LF_RunPlateRules.RANGE);
		IEntity best;
		float bestDistance = LF_RunPlateRules.RANGE * 2;
		foreach (IEntity candidate : near)
		{
			if (!candidate || candidate == policeCar || candidate.GetParent() == policeCar || policeCar.GetParent() == candidate)
				continue;

			if (!LF_RunPlateRules.IsAhead(origin, forward, candidate.GetOrigin()))
				continue;

			float distance = vector.Distance(origin, candidate.GetOrigin());
			if (distance < bestDistance)
			{
				bestDistance = distance;
				best = candidate;
			}
		}

		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: run the plate of `ahead` for an officer who sits at the wheel of a police car. The seam
	//! tests drive. The answer goes to each pager and beeps; with no pager on the officer it goes to a
	//! toast so the answer is never lost. A refusal is a toast in words.
	//! \param driving Whether the officer is at the wheel (the action reads the compartment).
	//! \param ahead The car in front, null when there is none.
	//! \return The outcome.
	static LF_ERunPlate Run(IEntity officer, bool driving, IEntity ahead, notnull array<LF_PagerComponent> pagers, string time)
	{
		if (!Replication.IsServer() || !officer)
			return LF_ERunPlate.OFF_DUTY;

		bool onDuty = LF_PoliceUtils.IsOnDutyOfficer(LF_PoliceUtils.GetAccountForEntity(officer));
		string plate;
		LF_EPlateStatus status = LF_EPlateStatus.UNKNOWN;
		if (onDuty && driving && ahead)
		{
			plate = LF_StolenVehicles.PlateOf(ahead);
			LF_StolenReport report;
			status = LF_StolenVehicles.CheckPlate(plate, report);
		}

		LF_ERunPlate outcome = LF_RunPlateRules.Decide(onDuty, driving, ahead != null, status);
		string officerUid = LF_Utils.GetPlayerUID(officer);
		if (!LF_RunPlateRules.IsAnswer(outcome))
		{
			LF_Debug.Warn(FEATURE, string.Format("run plate refused for %1: %2", officerUid, typename.EnumToString(LF_ERunPlate, outcome)));
			LF_Utils.NotifyEntity(officer, LF_RunPlateRules.RefusalKey(outcome), "#LF-Stolen_CheckTitle", 4.0);
			return outcome;
		}

		LF_Debug.Info(FEATURE, string.Format("run plate by %1: '%2' -> %3", officerUid, plate, typename.EnumToString(LF_ERunPlate, outcome)));
		string entry = LF_Pager.Entry(time, Summary(plate, outcome), LF_WantedReports.DescribePlace(ahead.GetOrigin()));
		int written;
		foreach (LF_PagerComponent pager : pagers)
		{
			if (!pager)
				continue;

			pager.Receive(entry, LF_ServiceCallsConfig.PagerMessages());
			written++;
		}

		if (written == 0)
		{
			// No pager on the officer: the plate check toast still says it (the old CheckAsOfficer reply).
			LF_Debug.Warn(FEATURE, string.Format("run plate for %1: no pager carried, the answer is a toast", officerUid));
			LF_Utils.NotifyEntity(officer, LF_StolenVehicleRules.StatusKey(StatusOf(outcome)), "#LF-Stolen_CheckTitle", 5.0, plate);
		}

		return outcome;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_EPlateStatus StatusOf(LF_ERunPlate outcome)
	{
		if (outcome == LF_ERunPlate.STOLEN)
			return LF_EPlateStatus.STOLEN;

		if (outcome == LF_ERunPlate.CLEAR)
			return LF_EPlateStatus.CLEAR;

		return LF_EPlateStatus.UNKNOWN;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the action's whole path for one press.
	static LF_ERunPlate RunFromCar(IEntity policeCar, IEntity officer)
	{
		if (!policeCar || !officer)
			return LF_ERunPlate.OFF_DUTY;

		bool driving = LF_Hotwire.IsAtWheel(policeCar, officer);
		IEntity ahead;
		if (driving)
			ahead = FindAhead(policeCar);

		array<LF_PagerComponent> pagers = {};
		LF_Pager.FindCarried(officer, pagers);
		return Run(officer, driving, ahead, pagers, LF_Dispatch.ClockText());
	}
}

//------------------------------------------------------------------------------------------------
//! The "Run plate" row on a police car. The prompt reads the replicated duty bit and the seat; the
//! server re-checks the officer against the real account, the seat and the car ahead (LF_RunPlate).
class LF_RunPlateAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!LF_Systems.IsEnabled("Police"))
			return false;

		if (!LF_PlayerInteraction.IsOnDutyPolice(user))
			return false;

		return LF_Hotwire.IsAtWheel(GetOwner(), user);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#LF-RunPlate_Action";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (LF_Systems.Refuses("Police", "LF_RunPlateAction"))
			return;

		if (!Replication.IsServer() || !pOwnerEntity || !pUserEntity)
			return;

		LF_RunPlate.RunFromCar(pOwnerEntity, pUserEntity);
	}
}

//------------------------------------------------------------------------------------------------
//! The pager card shows a plate answer as its own line.
modded class LF_Pager
{
	//------------------------------------------------------------------------------------------------
	override static array<ref LF_UIPart> BuildParts(notnull array<string> messages)
	{
		array<string> shown = {};
		foreach (string message : messages)
		{
			shown.Insert(LF_RunPlate.ToBoardEntry(message));
		}

		return super.BuildParts(shown);
	}
}
