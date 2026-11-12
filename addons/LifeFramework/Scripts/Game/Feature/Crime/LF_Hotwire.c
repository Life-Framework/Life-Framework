//------------------------------------------------------------------------------------------------
//! Hotwiring (#412). A vehicle whose LF_VehicleLockComponent.m_bIgnitionLocked is on will not start
//! without its key. A thief seated at the wheel can hotwire it: a server-timed attempt with a
//! failure risk and noise. A success lets that one player start the engine and marks the vehicle
//! stolen on the server. A start with the real key never does.
//!
//! The start gate (SCR_CarControllerComponent.OnBeforeEngineStart, modded below) reads replicated
//! lock state, so client and server agree; only the server writes it (LF_Hotwire.Begin).
//!
//! Skill is the character level, as for the lockpick (LF_LockpickRules.SkillFromLevel). Nothing is
//! saved: a restart forgets which vehicles were hotwired or stolen.
//------------------------------------------------------------------------------------------------
enum LF_EHotwireCheck
{
	NONE,
	NO_IGNITION_LOCK,
	HAS_KEY,
	ALREADY,
	NOT_SEATED,
	BUSY
}

//------------------------------------------------------------------------------------------------
class LF_HotwireRules
{
	static const float BASE_SECONDS = 15;
	static const float MIN_SECONDS = 6;
	static const float BASE_SUCCESS = 0.5;
	static const float SUCCESS_PER_SKILL = 0.04;
	static const float MAX_SUCCESS = 0.9;
	static const float BASE_NOISE = 0.7;
	static const float NOISE_PER_SKILL = 0.04;
	static const float MIN_NOISE = 0.2;

	//------------------------------------------------------------------------------------------------
	static float Seconds(int skill)
	{
		skill = Math.Clamp(skill, 0, LF_LockpickRules.MAX_SKILL);
		return BASE_SECONDS - (BASE_SECONDS - MIN_SECONDS) * skill / LF_LockpickRules.MAX_SKILL;
	}

	//------------------------------------------------------------------------------------------------
	static float SuccessChance(int skill)
	{
		skill = Math.Clamp(skill, 0, LF_LockpickRules.MAX_SKILL);
		return Math.Min(BASE_SUCCESS + SUCCESS_PER_SKILL * skill, MAX_SUCCESS);
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the roll hotwires the ignition.
	static bool Succeeds(int skill, float roll)
	{
		return roll < SuccessChance(skill);
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when this result is loud enough to file a report. A success always is.
	static bool IsHeard(bool success, int skill, float noiseRoll)
	{
		if (success)
			return true;

		skill = Math.Clamp(skill, 0, LF_LockpickRules.MAX_SKILL);
		return noiseRoll < Math.Max(BASE_NOISE - NOISE_PER_SKILL * skill, MIN_NOISE);
	}

	//------------------------------------------------------------------------------------------------
	static LF_EHotwireCheck Check(bool ignitionLocked, bool hasKey, bool alreadyHotwired, bool seatedAtWheel, bool busy)
	{
		if (!ignitionLocked)
			return LF_EHotwireCheck.NO_IGNITION_LOCK;

		if (hasKey)
			return LF_EHotwireCheck.HAS_KEY;

		if (alreadyHotwired)
			return LF_EHotwireCheck.ALREADY;

		if (!seatedAtWheel)
			return LF_EHotwireCheck.NOT_SEATED;

		if (busy)
			return LF_EHotwireCheck.BUSY;

		return LF_EHotwireCheck.NONE;
	}

	//------------------------------------------------------------------------------------------------
	static string RefusalKey(LF_EHotwireCheck check)
	{
		switch (check)
		{
			case LF_EHotwireCheck.NO_IGNITION_LOCK: return "#LF-Hotwire_NotNeeded";
			case LF_EHotwireCheck.HAS_KEY: return "#LF-Hotwire_HasKey";
			case LF_EHotwireCheck.ALREADY: return "#LF-Hotwire_Already";
			case LF_EHotwireCheck.NOT_SEATED: return "#LF-Hotwire_NotSeated";
			case LF_EHotwireCheck.BUSY: return "#LF-Hotwire_Busy";
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! The engine start gate. A vehicle with a free ignition always starts; a locked one starts for a
	//! key holder, or for the player who hotwired it.
	static bool MayStart(bool ignitionLocked, bool hasKey, int hotwiredPlayerId, int driverPlayerId)
	{
		if (!ignitionLocked || hasKey)
			return true;

		return hotwiredPlayerId >= 0 && hotwiredPlayerId == driverPlayerId;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when an engine start marks the vehicle stolen: its ignition is locked and the
	//! driver does not hold its key (so the start came from a hotwire).
	static bool StartsStolen(bool ignitionLocked, bool hasKey)
	{
		return ignitionLocked && !hasKey;
	}
}

//------------------------------------------------------------------------------------------------
class LF_HotwireAttempt : Managed
{
	EntityID m_VehicleId;
	EntityID m_UserId;
	string m_sPlayerUid;
	int m_iSkill;
	float m_fSeconds;
	float m_fElapsed;
}

//------------------------------------------------------------------------------------------------
class LF_Hotwire
{
	static const string FEATURE = "Hotwire";
	static const float TICK_SECONDS = 0.5;

	//! Test seam: a value >= 0 replaces the skill-based attempt time.
	static float s_fSecondsOverride = -1;
	//! Test seam: forces the roll (0..1). Negative means random.
	static float s_fRollOverride = -1;

	protected static ref array<ref LF_HotwireAttempt> s_aAttempts;

	//------------------------------------------------------------------------------------------------
	protected static array<ref LF_HotwireAttempt> Attempts()
	{
		if (!s_aAttempts)
			s_aAttempts = new array<ref LF_HotwireAttempt>();

		return s_aAttempts;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_aAttempts = null;
		s_fSecondsOverride = -1;
		s_fRollOverride = -1;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsBusy(IEntity vehicle)
	{
		if (!vehicle)
			return false;

		foreach (LF_HotwireAttempt attempt : Attempts())
		{
			if (attempt.m_VehicleId == vehicle.GetID())
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected static int PlayerIdOf(IEntity user)
	{
		if (!user || !GetGame() || !GetGame().GetPlayerManager())
			return -1;

		return GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(user);
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when user sits in the pilot seat of vehicle.
	static bool IsAtWheel(IEntity vehicle, IEntity user)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(user);
		if (!character || !vehicle)
			return false;

		CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
		if (!access || access.GetVehicleIn(user) != vehicle)
			return false;

		return PilotCompartmentSlot.Cast(access.GetCompartment()) != null;
	}

	//------------------------------------------------------------------------------------------------
	//! Offer check for the hotwire action. Reads only.
	static LF_EHotwireCheck CheckVehicle(IEntity vehicle, IEntity user)
	{
		LF_VehicleLockComponent vehicleLock = LF_Component<LF_VehicleLockComponent>.Find(vehicle);
		if (!vehicleLock)
			return LF_EHotwireCheck.NO_IGNITION_LOCK;

		int playerId = PlayerIdOf(user);
		bool already = playerId >= 0 && vehicleLock.GetHotwiredPlayerId() == playerId;
		return LF_HotwireRules.Check(vehicleLock.IsIgnitionLocked(), vehicleLock.UserHasValidKey(user), already, IsAtWheel(vehicle, user), IsBusy(vehicle));
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the hotwire action should be listed for this user at this vehicle.
	static bool ShowsHotwire(IEntity vehicle, IEntity user)
	{
		LF_VehicleLockComponent vehicleLock = LF_Component<LF_VehicleLockComponent>.Find(vehicle);
		if (!vehicleLock || !vehicleLock.IsIgnitionLocked())
			return false;

		return IsAtWheel(vehicle, user);
	}

	//------------------------------------------------------------------------------------------------
	//! The engine start gate for the seated driver. Reads replicated state, so it agrees on client and server.
	static bool MayStartEngine(IEntity vehicle, IEntity driver)
	{
		LF_VehicleLockComponent vehicleLock = LF_Component<LF_VehicleLockComponent>.Find(vehicle);
		if (!vehicleLock || !vehicleLock.IsIgnitionLocked())
			return true;

		bool hasKey = driver && vehicleLock.UserHasValidKey(driver);
		bool may = LF_HotwireRules.MayStart(true, hasKey, vehicleLock.GetHotwiredPlayerId(), PlayerIdOf(driver));
		if (!may)
			LF_Utils.Notify("#LF-Hotwire_NoKey", "#LF-Hotwire_Title", 3.0);

		return may;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: an engine started. An unkeyed start on a locked-ignition vehicle marks it stolen.
	static void NoteStart(IEntity vehicle, IEntity driver)
	{
		if (!Replication.IsServer())
			return;

		LF_VehicleLockComponent vehicleLock = LF_Component<LF_VehicleLockComponent>.Find(vehicle);
		if (!vehicleLock || !driver)
			return;

		bool hasKey = vehicleLock.UserHasValidKey(driver);
		if (!LF_HotwireRules.StartsStolen(vehicleLock.IsIgnitionLocked(), hasKey))
			return;

		if (!vehicleLock.IsStolen())
			LF_Debug.Info(FEATURE, string.Format("%1 started without its key: marked stolen", vehicleLock.GetVehicleIdentifier()));

		vehicleLock.SetStolen(true);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: starts a hotwire attempt.
	static bool Begin(IEntity vehicle, IEntity user)
	{
		if (!Replication.IsServer())
			return false;

		if (!vehicle || !user)
		{
			LF_Debug.Error(FEATURE, "begin refused: vehicle or user is null");
			return false;
		}

		string playerUid = LF_Utils.GetPlayerUid(user);
		if (playerUid.IsEmpty())
		{
			LF_Debug.Error(FEATURE, "begin refused: player has no account id");
			return false;
		}

		LF_EHotwireCheck check = CheckVehicle(vehicle, user);
		if (check != LF_EHotwireCheck.NONE)
		{
			LF_Debug.Info(FEATURE, string.Format("refused %1: %2", playerUid, typename.EnumToString(LF_EHotwireCheck, check)));
			LF_Utils.NotifyEntity(user, LF_HotwireRules.RefusalKey(check), "#LF-Hotwire_Title", 3.0);
			return false;
		}

		LF_HotwireAttempt attempt = new LF_HotwireAttempt();
		attempt.m_VehicleId = vehicle.GetID();
		attempt.m_UserId = user.GetID();
		attempt.m_sPlayerUid = playerUid;
		attempt.m_iSkill = LF_Lockpick.SkillOf(user);
		attempt.m_fSeconds = LF_HotwireRules.Seconds(attempt.m_iSkill);
		if (s_fSecondsOverride >= 0)
			attempt.m_fSeconds = s_fSecondsOverride;

		Attempts().Insert(attempt);
		LF_Debug.Info(FEATURE, string.Format("%1 starts hotwiring: skill=%2 seconds=%3", playerUid, attempt.m_iSkill, attempt.m_fSeconds));
		LF_Utils.NotifyEntity(user, "#LF-Hotwire_Started", "#LF-Hotwire_Title", 3.0);

		if (attempt.m_fSeconds <= 0)
			Complete(attempt);
		else
			GetGame().GetCallqueue().CallLater(Tick, TICK_SECONDS * 1000, false, attempt);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Tick(LF_HotwireAttempt attempt)
	{
		if (!attempt || Attempts().Find(attempt) == -1)
			return;

		IEntity vehicle = GetGame().GetWorld().FindEntityByID(attempt.m_VehicleId);
		IEntity user = GetGame().GetWorld().FindEntityByID(attempt.m_UserId);
		if (!vehicle || !user)
		{
			Drop(attempt, null, "vehicle or user is gone");
			return;
		}

		CharacterControllerComponent controller = LF_Component<CharacterControllerComponent>.Find(user);
		if (controller && controller.GetLifeState() != ECharacterLifeState.ALIVE)
		{
			Drop(attempt, user, "user is down");
			return;
		}

		if (!IsAtWheel(vehicle, user))
		{
			Drop(attempt, user, "user left the wheel");
			return;
		}

		attempt.m_fElapsed = attempt.m_fElapsed + TICK_SECONDS;
		if (attempt.m_fElapsed >= attempt.m_fSeconds)
		{
			Complete(attempt);
			return;
		}

		GetGame().GetCallqueue().CallLater(Tick, TICK_SECONDS * 1000, false, attempt);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Drop(LF_HotwireAttempt attempt, IEntity user, string reason)
	{
		Attempts().RemoveItem(attempt);
		LF_Debug.Info(FEATURE, string.Format("attempt by %1 dropped: %2", attempt.m_sPlayerUid, reason));
		if (user)
			LF_Utils.NotifyEntity(user, "#LF-Hotwire_Cancelled", "#LF-Hotwire_Title", 3.0);
	}

	//------------------------------------------------------------------------------------------------
	//! A stolen car is a crime: the thief's wanted level rises by the catalog's amount so the board entry
	//! (reported when the engine is heard) never reads "wanted level 0". Returns the new level.
	static int ApplyTheft(LF_PlayerAccount account, string playerUid, bool success)
	{
		if (!account || !success)
			return 0;

		return LF_CrimeCatalog.ApplyWanted(account, playerUid, LF_WantedReports.OFFENCE_VEHICLE_THEFT);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Complete(LF_HotwireAttempt attempt)
	{
		Attempts().RemoveItem(attempt);

		IEntity vehicle = GetGame().GetWorld().FindEntityByID(attempt.m_VehicleId);
		IEntity user = GetGame().GetWorld().FindEntityByID(attempt.m_UserId);
		LF_VehicleLockComponent vehicleLock;
		if (vehicle)
			vehicleLock = LF_Component<LF_VehicleLockComponent>.Find(vehicle);

		if (!vehicle || !user || !vehicleLock)
		{
			LF_Debug.Error(FEATURE, "attempt finished but the vehicle or user is gone");
			return;
		}

		float roll = Math.RandomFloat01();
		if (s_fRollOverride >= 0)
			roll = s_fRollOverride;

		bool success = LF_HotwireRules.Succeeds(attempt.m_iSkill, roll);
		if (success)
		{
			vehicleLock.SetHotwiredBy(PlayerIdOf(user));
			vehicleLock.SetStolen(true);
			ApplyTheft(LF_RobAction.FindAccount(attempt.m_sPlayerUid), attempt.m_sPlayerUid, true);
		}

		LF_Debug.Info(FEATURE, string.Format("%1 on %2: success=%3 (skill %4, roll %5)", attempt.m_sPlayerUid, vehicleLock.GetVehicleIdentifier(), success, attempt.m_iSkill, roll));

		string messageKey = "#LF-Hotwire_Failed";
		if (success)
			messageKey = "#LF-Hotwire_Done";

		LF_Utils.NotifyEntity(user, messageKey, "#LF-Hotwire_Title", 3.0);

		if (LF_HotwireRules.IsHeard(success, attempt.m_iSkill, Math.RandomFloat01()))
			LF_WantedReports.Report(attempt.m_sPlayerUid, user, LF_WantedReports.OFFENCE_VEHICLE_THEFT);
	}
}

//------------------------------------------------------------------------------------------------
//! Held action at the wheel: asks the server to start a hotwire. LF_Hotwire.Begin re-checks.
class LF_HotwireAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return LF_Hotwire.ShowsHotwire(GetOwner(), user);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		LF_EHotwireCheck check = LF_Hotwire.CheckVehicle(GetOwner(), user);
		if (check == LF_EHotwireCheck.NONE)
			return true;

		SetCannotPerformReason(LF_HotwireRules.RefusalKey(check));
		return false;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		LF_Hotwire.Begin(pOwnerEntity, pUserEntity);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#LF-Hotwire_Action";
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The engine start gate. A vehicle with m_bIgnitionLocked starts only for a key holder or the player
//! who hotwired it; every other vehicle starts as before.
modded class SCR_CarControllerComponent
{
	//------------------------------------------------------------------------------------------------
	protected IEntity LF_Driver()
	{
		PilotCompartmentSlot pilot = GetPilotCompartmentSlot();
		if (!pilot)
			return null;

		return pilot.GetOccupant();
	}

	//------------------------------------------------------------------------------------------------
	override bool OnBeforeEngineStart()
	{
		if (!super.OnBeforeEngineStart())
			return false;

		return LF_Hotwire.MayStartEngine(GetOwner(), LF_Driver());
	}

	//------------------------------------------------------------------------------------------------
	override void OnEngineStart()
	{
		super.OnEngineStart();

		LF_Hotwire.NoteStart(GetOwner(), LF_Driver());
	}
}
