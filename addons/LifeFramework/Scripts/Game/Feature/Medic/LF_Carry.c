//------------------------------------------------------------------------------------------------
enum LF_ECarryRefusal
{
	NONE,
	NO_PATIENT,
	SELF,
	NOT_DOWNED,
	NOT_ALLOWED,
	TOO_FAR,
	NO_SEAT
}

//------------------------------------------------------------------------------------------------
//! Load a downed player into a vehicle (#261, cut down 2026-11-15). The drag and carry hold that
//! used to come first is gone: it moved a client-owned body with the Game Master teleport every
//! 250 ms, which is what the player saw as a body teleporting around with no animation. Vanilla
//! has no carry or drag animation and no attach for a player character, and the ACE clips are
//! GPL (docs/research/animation-pipeline.md), so there is no smooth path to put back. What stays
//! is the vanilla casualty model (SCR_LoadCasualtySupportStationComponent): a medic standing by a
//! downed body loads it straight into the nearest free cargo seat within `m_fLoadRange`, through
//! `SCR_CompartmentAccessComponent.MoveInVehicle`, the compartment API every client and a late
//! joiner render from. Every load publishes LF_OnPlayerCarried with kind "load".
class LF_Carry
{
	static const string FEATURE = "Medic";
	static const string KIND_LOAD = "load";

	protected static const int LOAD_VERIFY_MS = 1500;
	protected static const int LOAD_TRIES = 4;

	protected static int s_iLoadedCount;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_iLoadedCount = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the one rule the offer and the server share for loading a body.
	static LF_ECarryRefusal DecideLoad(bool hasPatient, bool isSelf, bool patientDowned, bool allowed, float distance, float range)
	{
		if (!hasPatient)
			return LF_ECarryRefusal.NO_PATIENT;

		if (isSelf)
			return LF_ECarryRefusal.SELF;

		if (!patientDowned)
			return LF_ECarryRefusal.NOT_DOWNED;

		if (!allowed)
			return LF_ECarryRefusal.NOT_ALLOWED;

		if (distance > range)
			return LF_ECarryRefusal.TOO_FAR;

		return LF_ECarryRefusal.NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: who may load. Anyone when config says so, else only an on-duty medic.
	static bool MayCarry(bool anyone, bool medicOnDuty)
	{
		return anyone || medicOnDuty;
	}

	//------------------------------------------------------------------------------------------------
	static string RefusalKey(LF_ECarryRefusal refusal)
	{
		if (refusal == LF_ECarryRefusal.NOT_ALLOWED)
			return "#LF-Carry_NotAllowed";

		if (refusal == LF_ECarryRefusal.TOO_FAR)
			return "#LF-Carry_TooFar";

		if (refusal == LF_ECarryRefusal.NO_SEAT)
			return "#LF-Carry_NoSeat";

		return "#LF-Carry_NotDowned";
	}

	//------------------------------------------------------------------------------------------------
	protected static string Uid(IEntity entity)
	{
		string uid = LF_Utils.GetPlayerUID(entity);
		if (uid.IsEmpty())
			return "npc";

		return uid;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsSeatedIn(IEntity entity)
	{
		return IsSeated(entity);
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsSeated(IEntity entity)
	{
		if (!entity)
			return false;

		CompartmentAccessComponent access = CompartmentAccessComponent.Cast(entity.FindComponent(CompartmentAccessComponent));
		return access && access.IsInCompartment();
	}

	//------------------------------------------------------------------------------------------------
	protected static bool CarrierMayCarry(IEntity carrier)
	{
		bool onDuty;
		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		if (accounts && carrier)
			onDuty = LF_MedicDuty.IsOnDuty(accounts.GetAccount(LF_Utils.GetPlayerUid(carrier)));

		return MayCarry(LF_MedicConfig.Get().m_bCarryAnyone, onDuty);
	}

	//------------------------------------------------------------------------------------------------
	//! Refusal for this pair, the seat aside. `allowed` is passed in so a client can use its
	//! replicated duty state.
	static LF_ECarryRefusal CheckLoad(IEntity carrier, IEntity patient, bool allowed)
	{
		if (!carrier || !patient)
			return DecideLoad(false, false, false, allowed, 0, 0);

		return DecideLoad(true, carrier == patient, LF_Downed.IsDowned(patient), allowed, vector.Distance(carrier.GetOrigin(), patient.GetOrigin()), LF_MedicConfig.Get().m_fCarryRange);
	}

	//------------------------------------------------------------------------------------------------
	//! Client and server: is Load offered to this user on this body? A downed body in reach, by
	//! someone who may, with a free passenger seat in load range.
	static bool CanOfferLoad(IEntity user, IEntity target)
	{
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(target);
		if (!character || !character.LF_IsDowned() || IsSeated(target))
			return false;

		bool allowed = LF_MedicConfig.Get().m_bCarryAnyone || LF_MedicDuty.IsOnDutyState(LF_AccountReplication.GetState(user));
		if (DecideLoad(true, user == target, true, allowed, vector.Distance(user.GetOrigin(), target.GetOrigin()), LF_MedicConfig.Get().m_fCarryRange) != LF_ECarryRefusal.NONE)
			return false;

		return FindSeatVehicle(user, target) != null;
	}

	//------------------------------------------------------------------------------------------------
	//! The nearest vehicle within load range that has a free passenger (cargo) seat.
	static IEntity FindSeatVehicle(IEntity carrier, IEntity patient)
	{
		CompartmentAccessComponent access = CompartmentAccessComponent.Cast(patient.FindComponent(CompartmentAccessComponent));
		if (!access)
			return null;

		array<IEntity> vehicles = LF_Utils.GetNearbyVehicles(carrier.GetOrigin(), LF_MedicConfig.Get().m_fLoadRange);
		IEntity best;
		float bestDistance = 100000;
		foreach (IEntity vehicle : vehicles)
		{
			if (!vehicle || !access.FindFreeCompartment(vehicle, ECompartmentType.CARGO, true))
				continue;

			float distance = vector.Distance(carrier.GetOrigin(), vehicle.GetOrigin());
			if (distance < bestDistance)
			{
				bestDistance = distance;
				best = vehicle;
			}
		}

		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: `carrier` puts the downed `patient` in the nearest free passenger seat in reach.
	static LF_ECarryRefusal Load(IEntity carrier, IEntity patient)
	{
		if (!Replication.IsServer())
			return LF_ECarryRefusal.NO_PATIENT;

		LF_ECarryRefusal refusal = CheckLoad(carrier, patient, CarrierMayCarry(carrier));
		if (refusal == LF_ECarryRefusal.NONE && IsSeated(patient))
			refusal = LF_ECarryRefusal.NOT_DOWNED;

		if (refusal != LF_ECarryRefusal.NONE)
		{
			LF_Debug.Info(FEATURE, string.Format("load of %1 by %2 refused: %3", Uid(patient), Uid(carrier), typename.EnumToString(LF_ECarryRefusal, refusal)));
			LF_Utils.NotifyEntity(carrier, RefusalKey(refusal), "", 4.0);
			return refusal;
		}

		IEntity vehicle = FindSeatVehicle(carrier, patient);
		if (!vehicle)
		{
			LF_Debug.Info(FEATURE, string.Format("load of %1 by %2 refused: no free passenger seat within %3 m", Uid(patient), Uid(carrier), LF_MedicConfig.Get().m_fLoadRange));
			LF_Utils.NotifyEntity(carrier, RefusalKey(LF_ECarryRefusal.NO_SEAT), "", 4.0);
			return LF_ECarryRefusal.NO_SEAT;
		}

		SCR_CompartmentAccessComponent access = SCR_CompartmentAccessComponent.Cast(patient.FindComponent(SCR_CompartmentAccessComponent));
		if (!access || !access.MoveInVehicle(vehicle, ECompartmentType.CARGO, true))
		{
			LF_Debug.Error(FEATURE, string.Format("load of %1 failed: the seat request was refused", Uid(patient)));
			LF_Utils.NotifyEntity(carrier, RefusalKey(LF_ECarryRefusal.NO_SEAT), "", 4.0);
			return LF_ECarryRefusal.NO_SEAT;
		}

		s_iLoadedCount++;
		LF_Debug.Info(FEATURE, string.Format("%1 loaded %2 into a vehicle", Uid(carrier), Uid(patient)));
		LF_EventBus.Publish(LF_OnPlayerCarried.Create(Uid(patient), Uid(carrier), KIND_LOAD, patient.GetOrigin()));
		if (GetGame().GetCallqueue())
			GetGame().GetCallqueue().CallLater(VerifyLoad, LOAD_VERIFY_MS, false, patient, vehicle, 1);

		return LF_ECarryRefusal.NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! The seat request is asked of the owning client and answered a frame or more later, and a
	//! request can be lost. Look again: ask again, up to LOAD_TRIES.
	protected static void VerifyLoad(IEntity patient, IEntity vehicle, int attempt)
	{
		if (!patient || !vehicle)
			return;

		if (IsSeated(patient))
		{
			LF_Debug.Info(FEATURE, string.Format("%1 sits in the vehicle (check %2), bleed-out paused: %3", Uid(patient), attempt, LF_BedDelivery.IsLoaded(patient)));
			return;
		}

		if (attempt >= LOAD_TRIES)
		{
			LF_Debug.Error(FEATURE, string.Format("%1 is still not in a seat after %2 checks", Uid(patient), attempt));
			return;
		}

		LF_Debug.Warn(FEATURE, string.Format("%1 is not seated yet (check %2), asking again", Uid(patient), attempt));
		SCR_CompartmentAccessComponent access = SCR_CompartmentAccessComponent.Cast(patient.FindComponent(SCR_CompartmentAccessComponent));
		if (access)
			access.MoveInVehicle(vehicle, ECompartmentType.CARGO, true);

		GetGame().GetCallqueue().CallLater(VerifyLoad, LOAD_VERIFY_MS, false, patient, vehicle, attempt + 1);
	}

	//------------------------------------------------------------------------------------------------
	static int GetLoadedCount()
	{
		return s_iLoadedCount;
	}
}

//------------------------------------------------------------------------------------------------
//! "Load into vehicle" on a player character (context LF_ID).
class LF_LoadPatientAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!LF_Systems.IsEnabled("Medic"))
			return false;

		return LF_Carry.CanOfferLoad(user, GetOwner());
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		return CanBeShownScript(user);
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (LF_Systems.Refuses("Medic", "LF_LoadPatientAction"))
			return;

		if (Replication.IsServer())
			LF_Carry.Load(pUserEntity, pOwnerEntity);
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of this file's statics (Core/LF_StaticReset.c runs every feature's hook).
modded class LF_StaticResetHooks7
{
	override void Run(notnull array<string> done, bool apply)
	{
		super.Run(done, apply);
		done.Insert("LF_Carry");
		if (apply)
			LF_Carry.ResetStatics();
	}
}
