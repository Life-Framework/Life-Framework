//------------------------------------------------------------------------------------------------
//! The real half of the escort car (#3431). LF_JailEscort holds who is escorted by whom and which
//! seat row is taken; this class does what a player sees: the held suspect is moved into a real
//! cargo seat of the police car (`SCR_CompartmentAccessComponent.MoveInVehicle`, the compartment
//! API every client and a late joiner render from, as LF_Carry does for a downed body), and
//! stepped out beside it on take-out. Server only. The suspect's body belongs to their client, so
//! both moves are requests the owner answers; a lost seat request is asked again, and a seat the
//! engine refuses rolls the hold back to on foot.
//------------------------------------------------------------------------------------------------
class LF_JailEscortCar
{
	static const string FEATURE = "Jail";
	//! The suspect must stand this close to the car to be put in (metres).
	static const float PUT_IN_REACH = 8.0; // lf-fixed: a car's reach, not a server choice
	//! Where the suspect steps out: this far to the car's side (metres).
	static const float STEP_OUT_SIDE = 2.5; // lf-fixed: where the suspect lands beside the car
	protected static const int SEAT_VERIFY_MS = 1500; // lf-fixed: engine seat-request settle time, as LF_Carry
	protected static const int SEAT_TRIES = 4; // lf-fixed: retries of a lost seat request, as LF_Carry

	//------------------------------------------------------------------------------------------------
	//! Pure: is the suspect close enough to the car to be put in.
	static bool InReach(float distanceMeters)
	{
		return distanceMeters <= PUT_IN_REACH;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the officer puts the suspect they hold on foot into this car. \return True when the
	//! hold moved into the car and the seat was requested.
	static bool PutIn(IEntity officer, IEntity vehicle)
	{
		if (!Replication.IsServer() || !officer || !vehicle)
			return false;

		string officerUid = LF_Utils.GetPlayerUid(officer);
		string prisonerUid = LF_JailEscort.GetOnFootPrisoner(officerUid);
		if (officerUid.IsEmpty() || prisonerUid.IsEmpty())
		{
			LF_Debug.Warn(FEATURE, string.Format("put-in refused: %1 holds no suspect on foot", officerUid));
			LF_Utils.NotifyEntity(officer, "#EL-Escort_NoPrisoner", "", 4.0);
			return false;
		}

		LF_PlayerAccount account;
		IEntity prisoner = LF_PoliceUtils.FindPlayerEntityByUid(prisonerUid, account);
		if (!prisoner)
		{
			LF_Debug.Warn(FEATURE, string.Format("put-in refused: suspect %1 is not connected", prisonerUid));
			LF_Utils.NotifyEntity(officer, "#EL-Escort_NoPrisoner", "", 4.0);
			return false;
		}

		float distance = vector.Distance(prisoner.GetOrigin(), vehicle.GetOrigin());
		if (!InReach(distance))
		{
			LF_Debug.Info(FEATURE, string.Format("put-in refused: %1 is %2 m from the car, reach %3", prisonerUid, distance, PUT_IN_REACH));
			LF_Utils.NotifyEntity(officer, "#LF-Interact_TooFar", "", 4.0);
			return false;
		}

		string vehicleId = LF_JailEscort.VehicleIdOf(vehicle);
		LF_JailEscort.EnsurePassengerSeat(vehicleId);
		if (!LF_JailEscort.Load(officerUid, prisonerUid, vehicleId))
		{
			LF_Utils.NotifyEntity(officer, LF_Carry.RefusalKey(LF_ECarryRefusal.NO_SEAT), "", 4.0);
			return false;
		}

		SCR_CompartmentAccessComponent access = SCR_CompartmentAccessComponent.Cast(prisoner.FindComponent(SCR_CompartmentAccessComponent));
		if (!access || !access.MoveInVehicle(vehicle, ECompartmentType.CARGO, true))
		{
			LF_Debug.Error(FEATURE, string.Format("put-in of %1 failed: the seat request was refused, the hold is back on foot", prisonerUid));
			LF_JailEscort.TakeOut(officerUid, prisonerUid);
			LF_Utils.NotifyEntity(officer, LF_Carry.RefusalKey(LF_ECarryRefusal.NO_SEAT), "", 4.0);
			return false;
		}

		LF_Debug.Info(FEATURE, string.Format("%1 put %2 in %3", officerUid, prisonerUid, vehicleId));
		if (GetGame().GetCallqueue())
			GetGame().GetCallqueue().CallLater(VerifySeat, SEAT_VERIFY_MS, false, prisoner, vehicle, 1);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The seat request is answered by the owning client a frame or more later and can be lost.
	//! Look again, ask again up to SEAT_TRIES.
	protected static void VerifySeat(IEntity prisoner, IEntity vehicle, int attempt)
	{
		if (!prisoner || !vehicle)
			return;

		CompartmentAccessComponent access = CompartmentAccessComponent.Cast(prisoner.FindComponent(CompartmentAccessComponent));
		if (access && access.IsInCompartment())
		{
			LF_Debug.Info(FEATURE, string.Format("%1 sits in the escort car (check %2)", LF_Utils.GetPlayerUid(prisoner), attempt));
			return;
		}

		// The hold left the car meanwhile (take-out, disconnect, cell): never seat them again.
		if (!LF_JailEscort.IsSeated(LF_Utils.GetPlayerUid(prisoner)))
			return;

		if (attempt >= SEAT_TRIES)
		{
			LF_Debug.Error(FEATURE, string.Format("%1 is still not in a seat after %2 checks", LF_Utils.GetPlayerUid(prisoner), attempt));
			return;
		}

		LF_Debug.Warn(FEATURE, string.Format("%1 is not seated yet (check %2), asking again", LF_Utils.GetPlayerUid(prisoner), attempt));
		SCR_CompartmentAccessComponent seatAccess = SCR_CompartmentAccessComponent.Cast(prisoner.FindComponent(SCR_CompartmentAccessComponent));
		if (seatAccess)
			seatAccess.MoveInVehicle(vehicle, ECompartmentType.CARGO, true);

		GetGame().GetCallqueue().CallLater(VerifySeat, SEAT_VERIFY_MS, false, prisoner, vehicle, attempt + 1);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the officer takes the seated suspect out of this car. They step out beside the car and
	//! stay held on foot; at the station the officer then books them with one of the booking actions
	//! (LF_JailBooking) instead of the take-out choosing the sentence. \return True when something changed.
	static bool TakeOut(IEntity officer, IEntity vehicle)
	{
		if (!Replication.IsServer() || !officer || !vehicle)
			return false;

		string officerUid = LF_Utils.GetPlayerUid(officer);
		string prisonerUid = LF_JailEscort.GetSeatedPrisoner(officerUid);
		if (officerUid.IsEmpty() || prisonerUid.IsEmpty())
		{
			LF_Debug.Warn(FEATURE, string.Format("take-out refused: %1 has no suspect seated", officerUid));
			LF_Utils.NotifyEntity(officer, "#EL-Escort_NoPrisoner", "", 4.0);
			return false;
		}

		LF_PlayerAccount account;
		IEntity prisoner = LF_PoliceUtils.FindPlayerEntityByUid(prisonerUid, account);
		if (!LF_JailEscort.TakeOut(officerUid, prisonerUid))
			return false;

		if (prisoner)
			LF_Jail.DismountPrisoner(prisoner, vehicle.CoordToParent(Vector(STEP_OUT_SIDE, 0, 0)));

		LF_Debug.Info(FEATURE, string.Format("%1 took %2 out of the escort car", officerUid, prisonerUid));
		if (LF_JailBooking.IsAtBooking(vehicle.GetOrigin()))
			LF_Utils.NotifyEntity(officer, "#LF-JailBook_AtStation", "", 6.0);
		return true;
	}
}
