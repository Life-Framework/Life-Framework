//------------------------------------------------------------------------------------------------
enum LF_ECarryMode
{
	NONE,
	DRAG,
	CARRY
}

//------------------------------------------------------------------------------------------------
enum LF_ECarryRefusal
{
	NONE,
	NO_PATIENT,
	SELF,
	NOT_DOWNED,
	NOT_ALLOWED,
	TOO_FAR,
	ALREADY_HELD,
	BUSY,
	NO_SEAT,
	NOT_HELD
}

//------------------------------------------------------------------------------------------------
//! One body a player is dragging or carrying. Server only. Entities are kept as ids.
class LF_CarryHold : Managed
{
	EntityID m_CarrierId;
	EntityID m_PatientId;
	string m_sCarrierUid;
	string m_sPatientUid;
	LF_ECarryMode m_eMode;
}

//------------------------------------------------------------------------------------------------
//! Drag and carry a downed player (#261). The server owns the hold: a shared 250 ms tick moves the
//! body to a point behind the carrier, the hold ends when the body is no longer downed, the carrier
//! goes down, dies, leaves or boards a vehicle, or the body falls too far behind. Drop and Load
//! (into the nearest free passenger seat) end it by hand. The hold is replicated on the body
//! (`SCR_ChimeraCharacter.LF_GetCarrierPlayerId`) so every client, a late joiner too, reads who
//! has it. No animation yet: the carry and drag animations in the ecosystem list (ACE) are GPL
//! and not ours to ship; the body is moved through the same server move the jail escort uses.
//! Every start, drop and load publishes LF_OnPlayerCarried.
class LF_Carry
{
	static const string FEATURE = "Medic";
	static const string KIND_DRAG = "drag";
	static const string KIND_CARRY = "carry";
	static const string KIND_DROP = "drop";
	static const string KIND_LOAD = "load";
	static const string KIND_DELIVER = "deliver";

	protected static const int TICK_MS = 250;
	protected static const int LOAD_VERIFY_MS = 1500;
	protected static const int LOAD_TRIES = 4;
	//! The body is only moved when it is this far (metres) from its spot.
	protected static const float MOVE_DEADZONE = 0.5;

	//! Keyed by the carrier's entity id string (one hold per carrier).
	protected static ref map<string, ref LF_CarryHold> s_mHolds;
	protected static bool s_bTicking;
	protected static int s_iLoadedCount;

	//------------------------------------------------------------------------------------------------
	protected static map<string, ref LF_CarryHold> Holds()
	{
		if (!s_mHolds)
			s_mHolds = new map<string, ref LF_CarryHold>();

		return s_mHolds;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		if (s_bTicking && GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(Tick);

		s_mHolds = null;
		s_bTicking = false;
		s_iLoadedCount = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the one rule the offer and the server share for starting a hold.
	static LF_ECarryRefusal DecideStart(bool hasPatient, bool isSelf, bool patientDowned, bool patientHeld, bool carrierBusy, bool allowed, float distance, float range)
	{
		if (!hasPatient)
			return LF_ECarryRefusal.NO_PATIENT;

		if (isSelf)
			return LF_ECarryRefusal.SELF;

		if (!patientDowned)
			return LF_ECarryRefusal.NOT_DOWNED;

		if (patientHeld)
			return LF_ECarryRefusal.ALREADY_HELD;

		if (carrierBusy)
			return LF_ECarryRefusal.BUSY;

		if (!allowed)
			return LF_ECarryRefusal.NOT_ALLOWED;

		if (distance > range)
			return LF_ECarryRefusal.TOO_FAR;

		return LF_ECarryRefusal.NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: who may drag or carry. Anyone when config says so, else only an on-duty medic.
	static bool MayCarry(bool anyone, bool medicOnDuty)
	{
		return anyone || medicOnDuty;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: where the body goes, `distance` metres behind the carrier along the carrier's facing.
	static vector FollowPoint(vector carrierPos, vector carrierForward, float distance)
	{
		vector flat = Vector(carrierForward[0], 0, carrierForward[2]);
		if (flat.LengthSq() < 0.0001)
			return carrierPos;

		flat.Normalize();
		return carrierPos - flat * distance;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: should the hold end this tick? Returns a reason, or "" to keep it.
	static string EndReason(bool carrierAlive, bool carrierSeated, bool patientAlive, bool patientDowned, float distance, float releaseDistance)
	{
		if (!carrierAlive)
			return "carrier gone or down";

		if (carrierSeated)
			return "carrier boarded a vehicle";

		if (!patientAlive)
			return "body gone";

		if (!patientDowned)
			return "no longer downed";

		if (distance > releaseDistance)
			return "body fell behind";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	static string RefusalKey(LF_ECarryRefusal refusal)
	{
		if (refusal == LF_ECarryRefusal.NOT_ALLOWED)
			return "#LF-Carry_NotAllowed";

		if (refusal == LF_ECarryRefusal.TOO_FAR)
			return "#LF-Carry_TooFar";

		if (refusal == LF_ECarryRefusal.ALREADY_HELD)
			return "#LF-Carry_Held";

		if (refusal == LF_ECarryRefusal.BUSY)
			return "#LF-Carry_Busy";

		if (refusal == LF_ECarryRefusal.NO_SEAT)
			return "#LF-Carry_NoSeat";

		if (refusal == LF_ECarryRefusal.NOT_HELD)
			return "#LF-Carry_NotHeld";

		return "#LF-Carry_NotDowned";
	}

	//------------------------------------------------------------------------------------------------
	protected static string KeyOf(IEntity carrier)
	{
		return carrier.GetID().ToString();
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
	protected static bool CarrierBusy(IEntity carrier)
	{
		if (!s_mHolds || !carrier)
			return false;

		return s_mHolds.Contains(KeyOf(carrier));
	}

	//------------------------------------------------------------------------------------------------
	protected static bool PatientHeld(IEntity patient)
	{
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(patient);
		return character && character.LF_GetCarrierPlayerId() != 0;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsSeatedIn(IEntity entity)
	{
		return IsSeated(entity);
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsSeated(IEntity entity)
	{
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
	//! Refusal for this pair. `allowed` is passed in so a client can use its replicated duty state.
	static LF_ECarryRefusal CheckStart(IEntity carrier, IEntity patient, bool allowed)
	{
		if (!carrier || !patient)
			return DecideStart(false, false, false, false, false, allowed, 0, 0);

		if (carrier != patient && IsSeated(carrier))
			return LF_ECarryRefusal.BUSY;

		return DecideStart(true, carrier == patient, LF_Downed.IsDowned(patient), PatientHeld(patient), CarrierBusy(carrier), allowed, vector.Distance(carrier.GetOrigin(), patient.GetOrigin()), LF_MedicConfig.Get().m_fCarryRange);
	}

	//------------------------------------------------------------------------------------------------
	//! Client and server: is Drag or Carry offered to this user on this body?
	static bool CanOfferStart(IEntity user, IEntity target)
	{
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(target);
		if (!character || !character.LF_IsDowned() || character.LF_GetCarrierPlayerId() != 0)
			return false;

		bool allowed = LF_MedicConfig.Get().m_bCarryAnyone || LF_MedicDuty.IsOnDutyState(LF_AccountReplication.GetState(user));
		LF_ECarryRefusal refusal = DecideStart(true, user == target, true, false, false, allowed, vector.Distance(user.GetOrigin(), target.GetOrigin()), LF_MedicConfig.Get().m_fCarryRange);
		return refusal == LF_ECarryRefusal.NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! Client and server: is Drop or Load offered to this user on this body? Only to the carrier.
	static bool CanOfferHeld(IEntity user, IEntity target)
	{
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(target);
		if (!character || !character.LF_IsDowned())
			return false;

		int userId = LF_PlayerInteraction.PlayerIdOf(user);
		return userId != 0 && character.LF_GetCarrierPlayerId() == userId;
	}

	//------------------------------------------------------------------------------------------------
	//! Client and server: is Load offered? Held by this user and a free passenger seat in reach.
	static bool CanOfferLoad(IEntity user, IEntity target)
	{
		if (!CanOfferHeld(user, target))
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
	//! Server: `carrier` starts dragging or carrying `patient`.
	static LF_ECarryRefusal Start(IEntity carrier, IEntity patient, LF_ECarryMode mode)
	{
		if (!Replication.IsServer())
			return LF_ECarryRefusal.NO_PATIENT;

		LF_ECarryRefusal refusal = CheckStart(carrier, patient, CarrierMayCarry(carrier));
		if (refusal != LF_ECarryRefusal.NONE)
		{
			LF_Debug.Info(FEATURE, string.Format("carry of %1 by %2 refused: %3", Uid(patient), Uid(carrier), typename.EnumToString(LF_ECarryRefusal, refusal)));
			LF_Utils.NotifyEntity(carrier, RefusalKey(refusal), "", 4.0);
			return refusal;
		}

		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(patient);
		int carrierId = LF_PlayerInteraction.PlayerIdOf(carrier);
		if (!character)
		{
			LF_Debug.Error(FEATURE, string.Format("carry refused: the patient %1 is not a character", Uid(patient)));
			return LF_ECarryRefusal.NO_PATIENT;
		}

		// A carrier with no player behind them (a test stand-in) is marked -1: still held.
		if (carrierId == 0)
			carrierId = -1;

		LF_CarryHold hold = new LF_CarryHold();
		hold.m_CarrierId = carrier.GetID();
		hold.m_PatientId = patient.GetID();
		hold.m_sCarrierUid = Uid(carrier);
		hold.m_sPatientUid = Uid(patient);
		hold.m_eMode = mode;
		Holds().Set(KeyOf(carrier), hold);
		character.LF_SetCarrier(carrierId, mode);
		Arm();

		string kind = KIND_DRAG;
		if (mode == LF_ECarryMode.CARRY)
			kind = KIND_CARRY;

		LF_Debug.Info(FEATURE, string.Format("%1 starts the %2 of %3", hold.m_sCarrierUid, kind, hold.m_sPatientUid));
		LF_EventBus.Publish(LF_OnPlayerCarried.Create(hold.m_sPatientUid, hold.m_sCarrierUid, kind, patient.GetOrigin()));
		return LF_ECarryRefusal.NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the carrier lets the body go where it is.
	static LF_ECarryRefusal Drop(IEntity carrier)
	{
		if (!Replication.IsServer() || !carrier || !s_mHolds)
			return LF_ECarryRefusal.NOT_HELD;

		LF_CarryHold hold = s_mHolds.Get(KeyOf(carrier));
		if (!hold)
		{
			LF_Utils.NotifyEntity(carrier, RefusalKey(LF_ECarryRefusal.NOT_HELD), "", 4.0);
			return LF_ECarryRefusal.NOT_HELD;
		}

		End(hold, KIND_DROP, "dropped by the carrier");
		return LF_ECarryRefusal.NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the body `carrier` is holding right now, or null.
	static IEntity HeldPatient(IEntity carrier)
	{
		if (!s_mHolds || !carrier)
			return null;

		LF_CarryHold hold = s_mHolds.Get(KeyOf(carrier));
		if (!hold)
			return null;

		return GetGame().GetWorld().FindEntityByID(hold.m_PatientId);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: end the hold `carrier` has because the body is being put in a hospital bed (#262).
	static void EndForDelivery(IEntity carrier)
	{
		if (!s_mHolds || !carrier)
			return;

		LF_CarryHold hold = s_mHolds.Get(KeyOf(carrier));
		if (hold)
			End(hold, KIND_DELIVER, "put in a hospital bed");
	}

	//------------------------------------------------------------------------------------------------
	//! Server: put the carried body in the nearest free passenger seat in reach.
	static LF_ECarryRefusal Load(IEntity carrier)
	{
		if (!Replication.IsServer() || !carrier || !s_mHolds)
			return LF_ECarryRefusal.NOT_HELD;

		LF_CarryHold hold = s_mHolds.Get(KeyOf(carrier));
		if (!hold)
		{
			LF_Utils.NotifyEntity(carrier, RefusalKey(LF_ECarryRefusal.NOT_HELD), "", 4.0);
			return LF_ECarryRefusal.NOT_HELD;
		}

		IEntity patient = GetGame().GetWorld().FindEntityByID(hold.m_PatientId);
		IEntity vehicle;
		if (patient)
			vehicle = FindSeatVehicle(carrier, patient);

		if (!vehicle)
		{
			LF_Debug.Info(FEATURE, string.Format("load of %1 by %2 refused: no free passenger seat within %3 m", hold.m_sPatientUid, hold.m_sCarrierUid, LF_MedicConfig.Get().m_fLoadRange));
			LF_Utils.NotifyEntity(carrier, RefusalKey(LF_ECarryRefusal.NO_SEAT), "", 4.0);
			return LF_ECarryRefusal.NO_SEAT;
		}

		SCR_CompartmentAccessComponent access = SCR_CompartmentAccessComponent.Cast(patient.FindComponent(SCR_CompartmentAccessComponent));
		if (!access || !access.MoveInVehicle(vehicle, ECompartmentType.CARGO, true))
		{
			LF_Debug.Error(FEATURE, string.Format("load of %1 failed: the seat request was refused", hold.m_sPatientUid));
			LF_Utils.NotifyEntity(carrier, RefusalKey(LF_ECarryRefusal.NO_SEAT), "", 4.0);
			return LF_ECarryRefusal.NO_SEAT;
		}

		s_iLoadedCount++;
		End(hold, KIND_LOAD, "loaded into a vehicle");
		if (GetGame().GetCallqueue())
			GetGame().GetCallqueue().CallLater(VerifyLoad, LOAD_VERIFY_MS, false, patient, vehicle, 1);

		return LF_ECarryRefusal.NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! The seat request is asked of the owning client and answered a frame or more later, and a
	//! request can be lost (the body was just moved). Look again: ask again, up to LOAD_TRIES.
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
	protected static void End(LF_CarryHold hold, string kind, string reason)
	{
		IEntity patient = GetGame().GetWorld().FindEntityByID(hold.m_PatientId);
		vector position;
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(patient);
		if (character)
		{
			character.LF_SetCarrier(0, LF_ECarryMode.NONE);
			position = patient.GetOrigin();
		}

		IEntity carrier = GetGame().GetWorld().FindEntityByID(hold.m_CarrierId);
		if (carrier)
			Holds().Remove(KeyOf(carrier));
		else
			RemoveByCarrierId(hold.m_CarrierId);

		LF_Debug.Info(FEATURE, string.Format("%1 let go of %2 (%3): %4", hold.m_sCarrierUid, hold.m_sPatientUid, kind, reason));
		LF_EventBus.Publish(LF_OnPlayerCarried.Create(hold.m_sPatientUid, hold.m_sCarrierUid, kind, position));
	}

	//------------------------------------------------------------------------------------------------
	protected static void RemoveByCarrierId(EntityID carrierId)
	{
		string found;
		foreach (string key, LF_CarryHold hold : Holds())
		{
			if (hold.m_CarrierId == carrierId)
				found = key;
		}

		if (!found.IsEmpty())
			Holds().Remove(found);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Arm()
	{
		if (s_bTicking || !GetGame() || !GetGame().GetCallqueue())
			return;

		s_bTicking = true;
		GetGame().GetCallqueue().CallLater(Tick, TICK_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	//! One shared tick: follow or end every hold; stops itself when none are left.
	protected static void Tick()
	{
		if (!s_mHolds || s_mHolds.IsEmpty())
		{
			s_bTicking = false;
			GetGame().GetCallqueue().Remove(Tick);
			return;
		}

		LF_MedicConfig config = LF_MedicConfig.Get();
		array<ref LF_CarryHold> snapshot = {};
		foreach (string key, LF_CarryHold hold : s_mHolds)
			snapshot.Insert(hold);

		foreach (LF_CarryHold hold : snapshot)
		{
			IEntity carrier = GetGame().GetWorld().FindEntityByID(hold.m_CarrierId);
			IEntity patient = GetGame().GetWorld().FindEntityByID(hold.m_PatientId);
			bool carrierAlive = false;
			bool carrierSeated = false;
			if (carrier)
			{
				carrierAlive = !LF_Downed.IsDowned(carrier);
				SCR_CharacterControllerComponent controller = SCR_CharacterControllerComponent.Cast(carrier.FindComponent(SCR_CharacterControllerComponent));
				if (controller && controller.GetLifeState() != ECharacterLifeState.ALIVE)
					carrierAlive = false;

				carrierSeated = IsSeated(carrier);
			}

			float distance = 0;
			if (carrier && patient)
				distance = vector.Distance(carrier.GetOrigin(), patient.GetOrigin());

			string reason = EndReason(carrierAlive, carrierSeated, patient != null, patient && LF_Downed.IsDowned(patient), distance, config.m_fCarryReleaseDistance);
			if (!reason.IsEmpty())
			{
				End(hold, KIND_DROP, reason);
				continue;
			}

			float behind = config.m_fDragDistance;
			if (hold.m_eMode == LF_ECarryMode.CARRY)
				behind = config.m_fCarryDistance;

			vector transform[4];
			carrier.GetWorldTransform(transform);
			vector target = FollowPoint(carrier.GetOrigin(), transform[2], behind);
			target[1] = carrier.GetOrigin()[1];
			if (vector.Distance(patient.GetOrigin(), target) > MOVE_DEADZONE)
				LF_Jail.MoveBody(patient, target);
		}
	}

	//------------------------------------------------------------------------------------------------
	static int GetHoldCount()
	{
		if (!s_mHolds)
			return 0;

		return s_mHolds.Count();
	}

	//------------------------------------------------------------------------------------------------
	static int GetLoadedCount()
	{
		return s_iLoadedCount;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a player left; end any hold they are in either side of. Disconnect while downed is
	//! handled by the bleed-out; this only keeps the hold table clean.
	static void ReleaseEntity(IEntity entity)
	{
		if (!s_mHolds || !entity)
			return;

		array<ref LF_CarryHold> snapshot = {};
		foreach (string key, LF_CarryHold hold : s_mHolds)
			snapshot.Insert(hold);

		foreach (LF_CarryHold hold : snapshot)
		{
			if (hold.m_CarrierId == entity.GetID() || hold.m_PatientId == entity.GetID())
				End(hold, KIND_DROP, "a party left");
		}
	}
}

//------------------------------------------------------------------------------------------------
//! The hold on the body, replicated: the player id of whoever has it (0 none) and the mode.
modded class SCR_ChimeraCharacter
{
	[RplProp()]
	protected int m_iLF_CarrierPlayerId;

	[RplProp()]
	protected int m_iLF_CarryMode;

	//------------------------------------------------------------------------------------------------
	void LF_SetCarrier(int carrierPlayerId, LF_ECarryMode mode)
	{
		if (!Replication.IsServer())
			return;

		if (m_iLF_CarrierPlayerId == carrierPlayerId && m_iLF_CarryMode == mode)
			return;

		m_iLF_CarrierPlayerId = carrierPlayerId;
		m_iLF_CarryMode = mode;
		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	int LF_GetCarrierPlayerId()
	{
		return m_iLF_CarrierPlayerId;
	}

	//------------------------------------------------------------------------------------------------
	LF_ECarryMode LF_GetCarryMode()
	{
		return m_iLF_CarryMode;
	}
}

//------------------------------------------------------------------------------------------------
//! Shared shape of the four carry entries on a player character (context LF_ID).
class LF_CarryActionBase : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	bool Offered(IEntity user)
	{
		return false;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return Offered(user);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		return Offered(user);
	}
}

//------------------------------------------------------------------------------------------------
class LF_DragAction : LF_CarryActionBase
{
	override bool Offered(IEntity user)
	{
		return LF_Carry.CanOfferStart(user, GetOwner());
	}

	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (Replication.IsServer())
			LF_Carry.Start(pUserEntity, pOwnerEntity, LF_ECarryMode.DRAG);
	}
}

//------------------------------------------------------------------------------------------------
class LF_CarryAction : LF_CarryActionBase
{
	override bool Offered(IEntity user)
	{
		return LF_Carry.CanOfferStart(user, GetOwner());
	}

	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (Replication.IsServer())
			LF_Carry.Start(pUserEntity, pOwnerEntity, LF_ECarryMode.CARRY);
	}
}

//------------------------------------------------------------------------------------------------
class LF_DropAction : LF_CarryActionBase
{
	override bool Offered(IEntity user)
	{
		return LF_Carry.CanOfferHeld(user, GetOwner());
	}

	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (Replication.IsServer())
			LF_Carry.Drop(pUserEntity);
	}
}

//------------------------------------------------------------------------------------------------
class LF_LoadPatientAction : LF_CarryActionBase
{
	override bool Offered(IEntity user)
	{
		return LF_Carry.CanOfferLoad(user, GetOwner());
	}

	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (Replication.IsServer())
			LF_Carry.Load(pUserEntity);
	}
}
