//------------------------------------------------------------------------------------------------
//! The server half of the vehicle lock ("no free cars"). The vanilla get-in action asks
//! LF_VehicleLockComponent.IsLocked on the client before it offers a seat; this guard asks the same rule
//! (MayEnter) on the server when a player's character lands in the driver's seat, and puts a character the rule
//! refuses back out beside the car, so a modified client cannot drive off in a locked, service or job car.
//! Only the driver's seat is guarded, also when it is reached by a seat change inside the car: a patient
//! carried into an ambulance or a suspect put in a police car takes a cargo seat by script and stays, but
//! never drives. The holder of the car's key passes even a locked car (they could unlock it). Non-player
//! characters (event scene crews, test fixtures) are left alone: the guard is about players.
//------------------------------------------------------------------------------------------------
class LF_VehicleEntryGuard
{
	static const string FEATURE = "VehicleLock";
	protected static const int RETRY_MS = 500; // lf-fixed: next-frame-ish retry, not a gameplay tunable
	protected static const int MAX_TRIES = 5; // lf-fixed: an anti-cheat retry, not a gameplay tunable

	protected static ref array<EntityID> s_aPutOut;
	protected static ref array<int> s_aTries;
	protected static int s_iRefused;

	//------------------------------------------------------------------------------------------------
	//! \return How many entries the guard refused this session (tests and the harness read it).
	static int RefusedCount()
	{
		return s_iRefused;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: whether a seat event is an entry the guard must refuse.
	//! \param isPlayer The character is a player's.
	//! \param pilot The seat is the driver's.
	//! \param hasLock The vehicle carries LF_VehicleLockComponent.
	//! \param allowed That lock lets this character drive (MayAllowDriver).
	static bool Refuses(bool isPlayer, bool pilot, bool hasLock, bool allowed)
	{
		if (!isPlayer || !pilot || !hasLock)
			return false;

		return !allowed;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the lock's entry rule, or the key for a car that is only locked (no service or job car).
	static bool MayAllowDriver(LF_VehicleLockComponent vehicleLock, IEntity character)
	{
		if (vehicleLock.MayEnter(character))
			return true;

		if (vehicleLock.IsServiceVehicle() || vehicleLock.GetJobDriverPlayerId() > 0)
			return false;

		return vehicleLock.UserHasValidKey(character);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a character just took a seat in `vehicle`. \return True when the entry was refused.
	static bool OnEntered(IEntity character, IEntity vehicle, BaseCompartmentSlot slot)
	{
		if (!Replication.IsServer() || !character || !vehicle)
			return false;

		LF_VehicleLockComponent vehicleLock = LF_Component<LF_VehicleLockComponent>.Find(vehicle);
		int playerId = LF_PlayerInteraction.PlayerIdOf(character);
		bool pilot = PilotCompartmentSlot.Cast(slot) != null;
		bool allowed = true;
		if (vehicleLock && pilot && playerId > 0)
			allowed = MayAllowDriver(vehicleLock, character);

		if (!Refuses(playerId > 0, pilot, vehicleLock != null, allowed))
			return false;

		s_iRefused++;
		LF_Debug.Warn(FEATURE, string.Format("entry refused: player %1 took the wheel of %2 without the right to (locked, service or job car); put out", playerId, vehicleLock.GetVehicleIdentifier()));
		LF_NotificationManagerComponent.NotifyPlayer(playerId, "#EL-VehicleLock_Title", "#EL-VehicleLock_Reason", 3.0, LF_ENotificationType.WARNING);

		// Not from inside the seat event that brought them in.
		Queue(character.GetID(), 0);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Queue(EntityID characterId, int tries)
	{
		if (!s_aPutOut)
			s_aPutOut = {};

		if (!s_aTries)
			s_aTries = {};

		if (s_aPutOut.Contains(characterId))
			return;

		if (s_aPutOut.IsEmpty() && GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().CallLater(PutOutPending, RETRY_MS, false);

		s_aPutOut.Insert(characterId);
		s_aTries.Insert(tries);
	}

	//------------------------------------------------------------------------------------------------
	protected static void PutOutPending()
	{
		if (!s_aPutOut || !s_aTries)
			return;

		array<EntityID> pending = {};
		array<int> tries = {};
		pending.Copy(s_aPutOut);
		tries.Copy(s_aTries);
		s_aPutOut.Clear();
		s_aTries.Clear();
		foreach (int i, EntityID characterId : pending)
		{
			if (!PutOut(characterId))
				continue;

			if (tries[i] + 1 < MAX_TRIES)
				Queue(characterId, tries[i] + 1);
			else
				LF_Debug.Error(FEATURE, "a refused rider was asked out 5 times and is still in: the owning client is not moving them");
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Asks once. \return True while the character still sits in a vehicle that refuses them (try again).
	protected static bool PutOut(EntityID characterId)
	{
		if (!GetGame() || !GetGame().GetWorld())
			return false;

		ChimeraCharacter character = ChimeraCharacter.Cast(GetGame().GetWorld().FindEntityByID(characterId));
		if (!character)
			return false;

		CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
		if (!access || !PilotCompartmentSlot.Cast(access.GetCompartment()))
			return false;

		IEntity vehicle = access.GetCompartment().GetVehicle();
		LF_VehicleLockComponent vehicleLock = LF_Component<LF_VehicleLockComponent>.Find(vehicle);
		if (!vehicleLock || MayAllowDriver(vehicleLock, character))
			return false;

		// Beside the vehicle, as LF_CuffEnforcement puts a cuffed driver out: a body this machine owns gets out
		// here; a body a remote client owns is moved through the vanilla Game Master path, which asks the owner.
		vector transform[4];
		character.GetWorldTransform(transform);
		transform[3] = vehicle.GetOrigin() + vehicle.GetTransformAxis(0).Normalized() * -2.5;

		RplComponent rpl = RplComponent.Cast(character.FindComponent(RplComponent));
		if (!rpl || rpl.IsOwner())
		{
			access.GetOutVehicle_NoDoor(transform, false, true);
		}
		else
		{
			SCR_EditableCharacterComponent editable = SCR_EditableCharacterComponent.Cast(character.FindComponent(SCR_EditableCharacterComponent));
			if (!editable)
			{
				LF_Debug.Error(FEATURE, "a refused rider cannot be put out: no SCR_EditableCharacterComponent to ask the owner");
				return false;
			}

			editable.SetTransform(transform);
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(PutOutPending);

		s_aPutOut = null;
		s_aTries = null;
		s_iRefused = 0;
	}
}

//------------------------------------------------------------------------------------------------
//! The seat event fires on the server for every character, whichever machine asked to get in.
modded class SCR_CompartmentAccessComponent
{
	//------------------------------------------------------------------------------------------------
	override protected void OnCompartmentEntered(IEntity targetEntity, BaseCompartmentManagerComponent manager, int mgrID, int slotID, bool move)
	{
		super.OnCompartmentEntered(targetEntity, manager, mgrID, slotID, move);
		if (Replication.IsServer())
			LF_VehicleEntryGuard.OnEntered(GetOwner(), targetEntity, GetCompartment());
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_VehicleEntryGuard's statics (LF_StaticReset finds it by reflection).
class LF_VehicleEntryGuard_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_VehicleEntryGuard";
	}

	override void Apply()
	{
		LF_VehicleEntryGuard.ResetStatics();
	}
}
