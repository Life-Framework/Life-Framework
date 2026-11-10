//------------------------------------------------------------------------------------------------
enum LF_EDeliveryRefusal
{
	NONE,
	NO_PATIENT,
	NOT_MEDIC,
	NOT_DOWNED,
	TOO_FAR
}

//------------------------------------------------------------------------------------------------
//! The ambulance run ends at a hospital bed (#262). A medic carries a downed patient (LF_Carry) to
//! a bed, or loads them into a stretcher vehicle (a prefab in `LF_MedicConfig.m_aStretcherVehicles`,
//! the flagship ambulance) and drives to the hospital; the Put in bed action on the bed
//! (`LF_BedDeliveryAction`) finds the patient, moves them onto the mattress and brings them back
//! through the revive path: `LF_Downed.Clear`, the reduced revive bill (`LF_TreatmentBills.
//! IssueForRevive`, `m_iReviveBillAmount`, against the wake bill of a death), and the events
//! LF_OnPlayerRevived then LF_OnPatientDelivered. A patient seated in a stretcher vehicle does not
//! bleed out (`IsLoaded`, read by `LF_BleedOut.Advance`). The server decides everything.
class LF_BedDelivery
{
	static const string FEATURE = "Medic";
	static const int STEP_OFF_MS = 3000;

	protected static int s_iDeliveredCount;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_iDeliveredCount = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the one rule the offer and the server share.
	static LF_EDeliveryRefusal Decide(bool hasPatient, bool patientDowned, bool medicOnDuty, float distance, float range)
	{
		if (!medicOnDuty)
			return LF_EDeliveryRefusal.NOT_MEDIC;

		if (!hasPatient)
			return LF_EDeliveryRefusal.NO_PATIENT;

		if (!patientDowned)
			return LF_EDeliveryRefusal.NOT_DOWNED;

		if (distance > range)
			return LF_EDeliveryRefusal.TOO_FAR;

		return LF_EDeliveryRefusal.NONE;
	}

	//------------------------------------------------------------------------------------------------
	static string RefusalKey(LF_EDeliveryRefusal refusal)
	{
		if (refusal == LF_EDeliveryRefusal.NOT_MEDIC)
			return "#LF-Bed_NotMedic";

		if (refusal == LF_EDeliveryRefusal.TOO_FAR)
			return "#LF-Bed_TooFar";

		return "#LF-Bed_NoPatient";
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: is `name` ("{GUID}path") the same resource as `configured`? Compares the GUID part.
	static bool SameResource(string name, string configured)
	{
		if (name.Length() < 18 || configured.Length() < 18)
			return false;

		return name.Substring(0, 18) == configured.Substring(0, 18);
	}

	//------------------------------------------------------------------------------------------------
	//! The vehicle's prefab, or one it inherits, is in the config's stretcher list.
	static bool IsStretcherVehicle(IEntity vehicle)
	{
		if (!vehicle)
			return false;

		EntityPrefabData data = vehicle.GetPrefabData();
		if (!data)
			return false;

		array<ResourceName> stretchers = LF_MedicConfig.Get().m_aStretcherVehicles;
		BaseContainer prefab = data.GetPrefab();
		while (prefab)
		{
			string name = prefab.GetResourceName();
			foreach (ResourceName configured : stretchers)
			{
				if (SameResource(name, configured))
					return true;
			}

			prefab = prefab.GetAncestor();
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! A character is seated in a stretcher vehicle (its bleed-out is paused).
	static bool IsLoaded(IEntity entity)
	{
		if (!entity)
			return false;

		IEntity vehicle = CompartmentAccessComponent.GetVehicleIn(entity);
		return vehicle && IsStretcherVehicle(vehicle);
	}

	//------------------------------------------------------------------------------------------------
	//! The patient this medic can put in `bed`: the body they hold, else a downed occupant of a
	//! stretcher vehicle parked within `m_fBedVehicleRange` of the bed.
	static IEntity FindPatient(IEntity medic, IEntity bed)
	{
		IEntity held = LF_Carry.HeldPatient(medic);
		if (held)
			return held;

		array<IEntity> vehicles = LF_Utils.GetNearbyVehicles(bed.GetOrigin(), LF_MedicConfig.Get().m_fBedVehicleRange);
		foreach (IEntity vehicle : vehicles)
		{
			if (!IsStretcherVehicle(vehicle))
				continue;

			SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
			if (!compartments)
				continue;

			array<IEntity> occupants = {};
			compartments.GetOccupants(occupants);
			foreach (IEntity occupant : occupants)
			{
				if (occupant && LF_Downed.IsDowned(occupant))
					return occupant;
			}
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: put `patient` in `bed` for `medic`, whose duty the caller resolved.
	//! \return NONE when the patient was revived in the bed, else why not.
	static LF_EDeliveryRefusal Deliver(IEntity medic, IEntity bed, IEntity patient, bool medicOnDuty)
	{
		if (!Replication.IsServer() || !medic || !bed)
			return LF_EDeliveryRefusal.NO_PATIENT;

		string medicUid = Uid(medic);
		string patientUid = Uid(patient);
		bool hasPatient = patient != null && patient != medic;
		bool downed = hasPatient && LF_Downed.IsDowned(patient);
		LF_EDeliveryRefusal refusal = Decide(hasPatient, downed, medicOnDuty, vector.Distance(medic.GetOrigin(), bed.GetOrigin()), LF_MedicConfig.Get().m_fBedRange);
		LF_HospitalBedComponent bedComponent = LF_HospitalBedComponent.Cast(bed.FindComponent(LF_HospitalBedComponent));
		if (refusal == LF_EDeliveryRefusal.NONE && !bedComponent)
			refusal = LF_EDeliveryRefusal.NO_PATIENT;

		if (refusal != LF_EDeliveryRefusal.NONE)
		{
			LF_Debug.Info(FEATURE, string.Format("bed delivery of %1 by %2 refused: %3", patientUid, medicUid, typename.EnumToString(LF_EDeliveryRefusal, refusal)));
			LF_Utils.NotifyEntity(medic, RefusalKey(refusal), "", 4.0);
			return refusal;
		}

		LF_Carry.EndForDelivery(medic);
		vector transform[4];
		bedComponent.GetWakeTransform(transform);
		LF_Jail.MoveBody(patient, transform[3]);
		if (!LF_Downed.Clear(patient))
			return LF_EDeliveryRefusal.NOT_DOWNED;

		s_iDeliveredCount++;
		LF_Debug.Info(FEATURE, string.Format("player %1 put in a hospital bed by medic %2", patientUid, medicUid));
		LF_EventBus.Publish(LF_OnPlayerRevived.Create(patientUid, medicUid, transform[3]));
		int billNumber;
		LF_TreatmentBillRecord bill = LF_TreatmentBills.IssueForRevive(medic, patient);
		if (bill)
			billNumber = bill.m_iNumber;

		LF_EventBus.Publish(LF_OnPatientDelivered.Create(patientUid, medicUid, billNumber, transform[3]));
		LF_Utils.NotifyEntity(medic, "#LF-Bed_Done", "", 4.0);
		LF_Utils.NotifyEntity(patient, "#LF-Revive_Revived", "", 6.0);
		if (GetGame().GetCallqueue())
			GetGame().GetCallqueue().CallLater(StepOff, STEP_OFF_MS, false, patient, bedComponent.GetStandPosition());

		return LF_EDeliveryRefusal.NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! The woken patient steps down from the mattress to the floor beside the bed.
	protected static void StepOff(IEntity patient, vector position)
	{
		if (!patient || LF_Downed.IsDowned(patient))
			return;

		LF_Jail.MoveBody(patient, position);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the action's entry. Finds the patient and reads the medic's duty from the account cache.
	static LF_EDeliveryRefusal Perform(IEntity medic, IEntity bed)
	{
		bool onDuty;
		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		if (accounts && medic)
			onDuty = LF_MedicDuty.IsOnDuty(accounts.GetAccount(LF_Utils.GetPlayerUid(medic)));

		return Deliver(medic, bed, FindPatient(medic, bed), onDuty);
	}

	//------------------------------------------------------------------------------------------------
	//! Client and server: is the action offered to this user? The patient is the server's to find.
	static bool CanOffer(IEntity user)
	{
		return user && LF_MedicDuty.IsOnDutyState(LF_AccountReplication.GetState(user));
	}

	//------------------------------------------------------------------------------------------------
	static int GetDeliveredCount()
	{
		return s_iDeliveredCount;
	}

	//------------------------------------------------------------------------------------------------
	protected static string Uid(IEntity entity)
	{
		string uid = LF_Utils.GetPlayerUID(entity);
		if (uid.IsEmpty())
			return "npc";

		return uid;
	}
}

//------------------------------------------------------------------------------------------------
//! "Put patient in bed" on a hospital bed (context LF_HospitalBed). Shown to an on-duty medic; the
//! server finds the patient and decides in LF_BedDelivery.Perform.
class LF_BedDeliveryAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return LF_BedDelivery.CanOffer(user);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		return LF_BedDelivery.CanOffer(user);
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		LF_BedDelivery.Perform(pUserEntity, pOwnerEntity);
	}
}
