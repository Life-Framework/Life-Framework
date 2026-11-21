// Harness verb `bedcheck`: every first-spawn and hospital bed in the loaded world, measured the way
// a waking body uses it. For each bed: the wake point (where the body is spawned and sits up) and
// the stand point against the floor found by an independent all-layer trace, and whether the wake
// point is clear of the mattress with room for a standing body. `ok beds=<n> worst=<metres>` when
// every point is within 3 cm of the floor, off the mattress and roomy; `error` names the first bad
// bed. One `[LFDebug:BedWake]` line per bed.
// Read-only.

//------------------------------------------------------------------------------------------------
class LF_BedWakeHarness
{
	static const float TOLERANCE = 0.03;

	//------------------------------------------------------------------------------------------------
	//! True when the first solid under a point (all layers, the bed's own mesh counts) is the bed.
	protected static bool OverBed(vector point, IEntity bed)
	{
		IEntity hit;
		LF_PlacementAudit.SurfaceTrace(point + "0 1.5 0", point - "0 1.5 0", null, hit);
		return hit == bed;
	}

	//------------------------------------------------------------------------------------------------
	//! The floor the bed stands on: the first solid under the bed's centre that is not the bed.
	protected static float FloorUnderBed(IEntity bed)
	{
		IEntity hit;
		vector from = bed.GetOrigin() + "0 0.3 0";
		float d = LF_PlacementAudit.SurfaceTrace(from, from - "0 2.3 0", bed, hit);
		return from[1] - d;
	}

	//------------------------------------------------------------------------------------------------
	//! One bed: logs it and returns the worst floor gap; problem is set when it fails. Gaps are
	//! measured against the floor under the bed, so a point on furniture beside it fails too.
	static float Check(string kind, IEntity bed, vector wake[4], vector stand, out string problem)
	{
		problem = "";
		float floor = FloorUnderBed(bed);
		bool wakeOnBed = OverBed(wake[3], bed);
		bool standOnBed = OverBed(stand, bed);
		float wakeGap = wake[3][1] - floor;
		float standGap = stand[1] - floor;
		bool fits = LF_BedWakePose.BodyFits(wake[3], bed);
		LF_Debug.Info("BedWake", string.Format("%1 bed %2 at %3: wake %4 gap %5 m%6 room %7, stand %8 gap %9 m", kind, bed.GetName(), bed.GetOrigin(), wake[3], wakeGap, OnBedText(wakeOnBed), fits, stand, standGap) + OnBedText(standOnBed));

		if (!fits)
			problem = string.Format("%1 bed %2: no room for a body at the wake point %3", kind, bed.GetName(), wake[3]);
		else if (wakeOnBed || standOnBed)
			problem = string.Format("%1 bed %2: a wake or stand point is over the mattress", kind, bed.GetName());
		else if (Math.AbsFloat(wakeGap) > TOLERANCE || Math.AbsFloat(standGap) > TOLERANCE)
			problem = string.Format("%1 bed %2: wake gap %3 m, stand gap %4 m", kind, bed.GetName(), wakeGap, standGap);

		return Math.Max(Math.AbsFloat(wakeGap), Math.AbsFloat(standGap));
	}

	//------------------------------------------------------------------------------------------------
	protected static string OnBedText(bool onBed)
	{
		if (onBed)
			return " (over the mattress)";

		return "";
	}
}

//------------------------------------------------------------------------------------------------
modded class LF_AgentHarnessVerbs
{
	override bool Handle(LF_AgentCommand cmd)
	{
		if (cmd.Verb != "bedcheck")
			return super.Handle(cmd);

		int count;
		float worst;
		string problem;
		array<LF_FirstSpawnBedComponent> firstBeds = {};
		LF_FirstSpawnBeds.GetBeds(firstBeds);
		foreach (LF_FirstSpawnBedComponent first : firstBeds)
		{
			vector wake[4];
			first.GetWakeTransform(wake);
			string bad;
			worst = Math.Max(worst, LF_BedWakeHarness.Check("first-spawn", first.GetOwner(), wake, first.GetStandPosition(), bad));
			if (problem.IsEmpty())
				problem = bad;
			count++;
		}

		array<LF_HospitalBedComponent> hospitalBeds = {};
		LF_HospitalBedComponent.GetBeds(hospitalBeds);
		foreach (LF_HospitalBedComponent hospital : hospitalBeds)
		{
			vector sit[4];
			hospital.GetSitTransform(sit);
			string hospitalBad;
			worst = Math.Max(worst, LF_BedWakeHarness.Check("hospital", hospital.GetOwner(), sit, hospital.GetStandPosition(), hospitalBad));
			if (problem.IsEmpty())
				problem = hospitalBad;
			count++;
		}

		if (!problem.IsEmpty())
		{
			LF_AgentHarness.Answer(cmd.Id, "error", problem);
			return true;
		}

		LF_AgentHarness.Answer(cmd.Id, "ok", string.Format("beds=%1 worst=%2", count, worst));
		return true;
	}
}
