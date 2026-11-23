//------------------------------------------------------------------------------------------------
//! A car the garage just handed out, shown to its owner (review A16: four locked cars stood on the lot and
//! nothing said which one was yours). A toast names the plate, the one waypoint sits over the car for
//! MARK_MS and its hazard lights blink for HAZARD_MS. When the mark goes, the player's guide puts its own
//! waypoint back (the next errand). Server only; nothing is saved.
class LF_VehicleHandover
{
	static const string FEATURE = "Vehicle";
	static const string WAYPOINT_ID = "my-car";
	static const int MARK_MS = 30000; // lf-fixed: long enough to walk to the car, review A16
	static const int HAZARD_MS = 8000; // lf-fixed: a few blinks, then the lights rest

	protected static ref LF_JobWaypointSink s_Sink;

	//------------------------------------------------------------------------------------------------
	//! Tests: replace where the waypoint goes. Null restores the game's.
	static void SetSink(LF_JobWaypointSink sink)
	{
		s_Sink = sink;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_JobWaypointSink Sink()
	{
		if (!s_Sink)
			s_Sink = new LF_JobWaypointRplSink();

		return s_Sink;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the garage handed `vehicleId` to `user`. \return false when the car is not in the world.
	static bool Announce(IEntity user, string vehicleId)
	{
		if (!Replication.IsServer() || !user || vehicleId.IsEmpty())
			return false;

		IEntity car = LF_VehicleOwnership.GetVehicle(vehicleId);
		string uid = LF_Utils.GetPlayerUID(user);
		if (!car || uid.IsEmpty())
		{
			LF_Debug.Warn(FEATURE, string.Format("handover of %1 not shown: no car in the world or no player", vehicleId));
			return false;
		}

		string plate = LF_VehicleOwnership.GetPlate(vehicleId);
		if (plate.IsEmpty())
			LF_Utils.NotifyEntity(user, "#LF-Vehicle_OutOnLotNoPlate", "#LF-Vehicle_OutOnLotTitle", 10);
		else
			LF_Utils.NotifyEntity(user, "#LF-Vehicle_OutOnLot", "#LF-Vehicle_OutOnLotTitle", 10, plate);

		vector origin = car.GetOrigin();
		Sink().Mark(uid, WAYPOINT_ID, "#LF-Vehicle_YourCarMark", Math.Round(origin[0]), Math.Round(origin[2]));
		Hazards(car, true);
		if (GetGame() && GetGame().GetCallqueue())
		{
			GetGame().GetCallqueue().CallLater(EndHazards, HAZARD_MS, false, vehicleId);
			GetGame().GetCallqueue().CallLater(EndMark, MARK_MS, false, uid);
		}

		LF_Debug.Info(FEATURE, string.Format("handover of %1 (plate '%2') shown to %3: toast, %4 s mark, hazards", vehicleId, plate, uid, MARK_MS / 1000));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Hazards(IEntity car, bool on)
	{
		if (!car)
			return;

		BaseLightManagerComponent lights = BaseLightManagerComponent.Cast(car.FindComponent(BaseLightManagerComponent));
		if (!lights)
		{
			LF_Debug.Info(FEATURE, "handover: the car has no light manager, no hazards");
			return;
		}

		lights.SetLightsState(ELightType.Hazard, on);
	}

	//------------------------------------------------------------------------------------------------
	protected static void EndHazards(string vehicleId)
	{
		Hazards(LF_VehicleOwnership.GetVehicle(vehicleId), false);
	}

	//------------------------------------------------------------------------------------------------
	//! The car's mark goes; the guide's own waypoint (the next errand) comes back.
	static void EndMark(string uid)
	{
		Sink().Clear(uid, WAYPOINT_ID);
		IEntity body = BodyOf(uid);
		if (!body)
			return;

		LF_QuestTrackerComponent tracker = LF_QuestTrackerComponent.Cast(body.FindComponent(LF_QuestTrackerComponent));
		if (tracker)
			tracker.RefreshGuide();
	}

	//------------------------------------------------------------------------------------------------
	protected static IEntity BodyOf(string uid)
	{
		PlayerManager players = GetGame().GetPlayerManager();
		if (!players || uid.IsEmpty())
			return null;

		array<int> ids = {};
		players.GetPlayers(ids);
		foreach (int playerId : ids)
		{
			if (LF_Utils.GetPlayerUID(playerId) == uid)
				return players.GetPlayerControlledEntity(playerId);
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_Sink = null;
		if (GetGame() && GetGame().GetCallqueue())
		{
			GetGame().GetCallqueue().Remove(EndHazards);
			GetGame().GetCallqueue().Remove(EndMark);
		}
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_VehicleHandover's statics (LF_StaticReset finds it by reflection).
class LF_VehicleHandover_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_VehicleHandover";
	}

	override void Apply()
	{
		LF_VehicleHandover.ResetStatics();
	}
}
