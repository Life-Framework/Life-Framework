//------------------------------------------------------------------------------------------------
//! The radar gun's rules (#4211): a measured speed against the road's limit gives one of three
//! speed classes, and each class names the law-book code the charge sheet will tick. Pure
//! functions, so the bands are tested without a car (LF_Test_RadarClasses).
class LF_RadarRules
{
	static const int CLASS_CLEAR = 0;
	static const int CLASS_MINOR = 1;
	static const int CLASS_ROAD = 2;
	static const int CLASS_SERIOUS = 3;

	//! Over the limit by at most this many km/h is minor, by at most ROAD_MAX is road, above is serious.
	static const int MINOR_MAX = 15;	// lf-fixed: the fines table bands in the police scope ("Fines and time")
	static const int ROAD_MAX = 30;	// lf-fixed: as MINOR_MAX

	//! Used when a place has no limit of its own (a missing or zero limit must not flag every car).
	static const int DEFAULT_LIMIT_KMH = 50;	// lf-fixed: the town limit; per-road limits come with the speed signs (PO-R5)

	//! The law-book code ids (#4151). The law-book cards own the amounts; the radar only names the code.
	static const string CODE_MINOR = "road.speeding.minor";
	static const string CODE_ROAD = "road.speeding";
	static const string CODE_SERIOUS = "road.speeding.serious";

	//------------------------------------------------------------------------------------------------
	//! \param speedKmh The measured speed, whole km/h.
	//! \param limitKmh The road's limit; 0 or less means the default.
	//! \return CLASS_CLEAR at or under the limit, else minor, road or serious by how far over.
	static int Classify(int speedKmh, int limitKmh)
	{
		if (limitKmh <= 0)
			limitKmh = DEFAULT_LIMIT_KMH;

		int over = speedKmh - limitKmh;
		if (over <= 0)
			return CLASS_CLEAR;

		if (over <= MINOR_MAX)
			return CLASS_MINOR;

		if (over <= ROAD_MAX)
			return CLASS_ROAD;

		return CLASS_SERIOUS;
	}

	//------------------------------------------------------------------------------------------------
	//! The law-book code for a class; empty for a clear reading.
	static string LawCode(int speedClass)
	{
		if (speedClass == CLASS_MINOR)
			return CODE_MINOR;

		if (speedClass == CLASS_ROAD)
			return CODE_ROAD;

		if (speedClass == CLASS_SERIOUS)
			return CODE_SERIOUS;

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! The toast key for a class.
	static string ReadingKey(int speedClass)
	{
		if (speedClass == CLASS_MINOR)
			return "#LF-Radar_Minor";

		if (speedClass == CLASS_ROAD)
			return "#LF-Radar_Road";

		if (speedClass == CLASS_SERIOUS)
			return "#LF-Radar_Serious";

		return "#LF-Radar_Clear";
	}

	//------------------------------------------------------------------------------------------------
	//! A velocity component length in m/s as whole km/h; the sign is the direction, so it is dropped.
	static int ToKmh(float metresPerSecond)
	{
		return Math.Round(Math.AbsFloat(metresPerSecond) * 3.6);
	}
}

//------------------------------------------------------------------------------------------------
//! The limit on the road where a car was read. Per-road limits arrive with the speed signs
//! (PO-R5); until then every reading is against the default.
class LF_RadarLimits
{
	//------------------------------------------------------------------------------------------------
	static int LimitAt(vector position)
	{
		return LF_RadarRules.DEFAULT_LIMIT_KMH;
	}
}

//------------------------------------------------------------------------------------------------
//! "Measure speed" on the radar gun: the officer aims at a car and reads its speed and class.
//! Offered to an on-duty officer; the server re-checks duty and runs the trace and the read.
class LF_RadarGunAction : ScriptedUserAction
{
	static const float RANGE = 150;	// lf-fixed: the gun's reach in metres; the police skill's "longer radar range" perk widens it later

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!LF_Systems.IsEnabled("Police"))
			return false;

		return LF_PlayerInteraction.IsOnDutyPolice(user);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		return LF_PlayerInteraction.IsOnDutyPolice(user);
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (LF_Systems.Refuses("Police", "LF_RadarGunAction"))
			return;

		if (!Replication.IsServer() || !pUserEntity)
			return;

		if (!LF_PoliceUtils.IsOnDutyOfficer(LF_PoliceUtils.GetAccountForEntity(pUserEntity)))
		{
			LF_Debug.Warn("Police", string.Format("radar refused: %1 is not an on-duty officer", pUserEntity));
			return;
		}

		Vehicle car = AimedVehicle(pUserEntity);
		if (!car)
		{
			LF_Utils.NotifyEntity(pUserEntity, "#LF-Radar_NoCar", "#EL-Police_Title", 3.0);
			return;
		}

		Physics physics = car.GetPhysics();
		if (!physics)
		{
			LF_Debug.Warn("Police", string.Format("radar: %1 has no physics, nothing to read", car));
			LF_Utils.NotifyEntity(pUserEntity, "#LF-Radar_NoCar", "#EL-Police_Title", 3.0);
			return;
		}

		int speed = LF_RadarRules.ToKmh(physics.GetVelocity().Length());
		int limit = LF_RadarLimits.LimitAt(car.GetOrigin());
		int speedClass = LF_RadarRules.Classify(speed, limit);
		LF_Debug.Info("Police", string.Format("radar: %1 read %2 km/h against %3, class %4 code '%5'", car, speed, limit, speedClass, LF_RadarRules.LawCode(speedClass)));
		LF_Utils.NotifyEntity(pUserEntity, LF_RadarRules.ReadingKey(speedClass), "#EL-Police_Title", 4.0, string.Format("%1/%2", speed, limit));
	}

	//------------------------------------------------------------------------------------------------
	//! The first vehicle on the officer's aim line within RANGE, else null.
	protected static Vehicle AimedVehicle(IEntity officer)
	{
		vector from = officer.GetOrigin() + Vector(0, 1.5, 0);
		vector dir = officer.GetTransformAxis(2);
		ChimeraCharacter chimera = ChimeraCharacter.Cast(officer);
		if (chimera)
		{
			CharacterControllerComponent controller = chimera.GetCharacterController();
			if (controller && controller.GetAimingComponent())
				dir = controller.GetAimingComponent().GetAimingDirectionWorld();
		}

		TraceParam trace = new TraceParam();
		trace.Start = from;
		trace.End = from + dir * LF_PoliceSkill.RadarRange(LF_Utils.GetPlayerUid(officer), RANGE); // long-radar perk (#4424)
		trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		trace.Exclude = officer;
		trace.LayerMask = EPhysicsLayerPresets.Projectile;
		GetGame().GetWorld().TraceMove(trace, null);

		IEntity hit = trace.TraceEnt;
		while (hit)
		{
			Vehicle car = Vehicle.Cast(hit);
			if (car)
				return car;

			hit = hit.GetParent();
		}

		return null;
	}
}
