//------------------------------------------------------------------------------------------------
//! Why a "Push it back over" was refused (#3333).
enum LF_EUnflipCheck
{
	NONE,
	NO_VEHICLE,
	NOT_OVERTURNED,
	OCCUPIED,
	COOLDOWN,
	TOO_FAR
}

//------------------------------------------------------------------------------------------------
//! One player puts an overturned, empty car back on its wheels (#3333). The server checks that the car
//! lies on its side or roof, that nobody is inside, that the player is within reach and that the car
//! was not pushed back over a moment ago, then sets it upright on the ground where it lies.
class LF_VehicleUnflip
{
	static const string FEATURE = "VehicleUnflip";
	//! The car's up axis points up by less than this (world up = 1) when it lies on its side or roof.
	static const float UPRIGHT_BELOW = 0.5; // lf-fixed: geometry threshold
	static const float REACH_METRES = 6.0; // lf-fixed: a push-over reach, not a server tunable
	static const int COOLDOWN_MS = 30000; // lf-fixed: anti-spam delay on one car, not a server tunable
	//! Lift above the ground the car is set down from, so the wheels settle instead of clipping.
	static const float DROP_HEIGHT = 0.6; // lf-fixed: set-down clearance

	protected static ref map<string, int> s_mLastUnflip = new map<string, int>();

	//------------------------------------------------------------------------------------------------
	//! The pure rule. lastMs < 0 means never pushed over.
	static LF_EUnflipCheck Rules(float upDot, bool occupied, float distance, int lastMs, int nowMs)
	{
		if (upDot >= UPRIGHT_BELOW)
			return LF_EUnflipCheck.NOT_OVERTURNED;

		if (occupied)
			return LF_EUnflipCheck.OCCUPIED;

		if (distance > REACH_METRES)
			return LF_EUnflipCheck.TOO_FAR;

		if (lastMs >= 0 && nowMs - lastMs < COOLDOWN_MS)
			return LF_EUnflipCheck.COOLDOWN;

		return LF_EUnflipCheck.NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! How far the car's up axis points up: 1 on its wheels, 0 on its side, -1 on its roof.
	static float UpDot(notnull IEntity vehicle)
	{
		vector transform[4];
		vehicle.GetWorldTransform(transform);
		return transform[1][1];
	}

	//------------------------------------------------------------------------------------------------
	static bool IsOverturned(notnull IEntity vehicle)
	{
		return UpDot(vehicle) < UPRIGHT_BELOW;
	}

	//------------------------------------------------------------------------------------------------
	static bool HasOccupant(notnull IEntity vehicle)
	{
		SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
		if (!compartments)
			return false;

		array<IEntity> occupants = {};
		compartments.GetOccupants(occupants);
		if (!occupants.IsEmpty())
			return true;

		// Someone still climbing in counts: a seat whose occupant is attached but not yet reported.
		array<BaseCompartmentSlot> slots = {};
		compartments.GetCompartments(slots);
		foreach (BaseCompartmentSlot slot : slots)
		{
			if (slot.GetOccupant() || slot.AttachedOccupant())
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected static string KeyOf(notnull IEntity vehicle)
	{
		RplComponent rpl = RplComponent.Cast(vehicle.FindComponent(RplComponent));
		if (rpl)
			return "v:" + rpl.Id().ToString();

		return "e:" + vehicle.GetID().ToString();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: checks the car and, when allowed, sets it upright. distance is the player's distance to the
	//! car (0 for a caller that already checked reach). Returns NONE when the car was put back on its wheels.
	static LF_EUnflipCheck Try(IEntity vehicle, float distance, int nowMs)
	{
		if (!vehicle)
		{
			LF_Debug.Error(FEATURE, "unflip refused: no vehicle");
			return LF_EUnflipCheck.NO_VEHICLE;
		}

		string key = KeyOf(vehicle);
		int lastMs = -1;
		s_mLastUnflip.Find(key, lastMs);
		LF_EUnflipCheck check = Rules(UpDot(vehicle), HasOccupant(vehicle), distance, lastMs, nowMs);
		if (check != LF_EUnflipCheck.NONE)
		{
			LF_Debug.Info(FEATURE, string.Format("unflip of %1 refused: %2", key, typename.EnumToString(LF_EUnflipCheck, check)));
			return check;
		}

		SetUpright(vehicle);
		s_mLastUnflip.Set(key, nowMs);
		LF_Debug.Info(FEATURE, string.Format("%1 set back on its wheels", key));
		return check;
	}

	//------------------------------------------------------------------------------------------------
	//! Same spot, same heading, wheels down, a little above the ground; the car stops moving.
	protected static void SetUpright(notnull IEntity vehicle)
	{
		vector transform[4];
		vehicle.GetWorldTransform(transform);

		// Heading from the car's forward axis flattened to the ground; on its nose the right axis stands in.
		float yaw = 0;
		vector forward = transform[2];
		forward[1] = 0;
		if (forward.Length() > 0.1)
		{
			yaw = forward.ToYaw();
		}
		else
		{
			vector right = transform[0];
			right[1] = 0;
			yaw = right.ToYaw() - 90;
		}

		vector at = vehicle.GetOrigin();
		float ground = GetGame().GetWorld().GetSurfaceY(at[0], at[2]);
		at[1] = ground + DROP_HEIGHT;

		vector upright[4];
		Math3D.AnglesToMatrix(Vector(yaw, 0, 0), upright);
		upright[3] = at;
		vehicle.SetWorldTransform(upright);
		vehicle.Update();

		Physics physics = vehicle.GetPhysics();
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
		s_mLastUnflip.Clear();
	}

	//------------------------------------------------------------------------------------------------
	static string RefusalKey(LF_EUnflipCheck check)
	{
		if (check == LF_EUnflipCheck.OCCUPIED)
			return "#LF-Unflip_Occupied";

		if (check == LF_EUnflipCheck.COOLDOWN)
			return "#LF-Unflip_Cooldown";

		if (check == LF_EUnflipCheck.TOO_FAR)
			return "#LF-Unflip_TooFar";

		return "#LF-Unflip_NotOverturned";
	}
}

//------------------------------------------------------------------------------------------------
//! "Push it back over": shows on an empty vehicle lying on its side or roof. The server re-checks everything.
class LF_VehicleUnflipAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!LF_Systems.IsEnabled("Vehicles"))
			return false;

		IEntity vehicle = GetOwner();
		if (!vehicle || !LF_VehicleUnflip.IsOverturned(vehicle))
			return false;

		return !LF_VehicleUnflip.HasOccupant(vehicle);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (LF_Systems.Refuses("Vehicles", "LF_VehicleUnflipAction"))
			return;

		if (!Replication.IsServer() || !pOwnerEntity || !pUserEntity)
			return;

		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(pUserEntity);
		float distance = vector.Distance(pUserEntity.GetOrigin(), pOwnerEntity.GetOrigin());
		int nowMs = GetGame().GetWorld().GetWorldTime();
		LF_EUnflipCheck check = LF_VehicleUnflip.Try(pOwnerEntity, distance, nowMs);
		if (check == LF_EUnflipCheck.NONE)
			return;

		LF_NotificationManagerComponent.NotifyPlayer(playerId, "#LF-Unflip_Action", LF_VehicleUnflip.RefusalKey(check), 4.0, LF_ENotificationType.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#LF-Unflip_Action";
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_VehicleUnflip's statics (LF_StaticReset finds it by reflection).
class LF_VehicleUnflip_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_VehicleUnflip";
	}

	override void Apply()
	{
		LF_VehicleUnflip.ResetStatics();
	}
}
