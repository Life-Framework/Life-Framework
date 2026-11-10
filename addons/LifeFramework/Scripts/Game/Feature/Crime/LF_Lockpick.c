//------------------------------------------------------------------------------------------------
//! Lockpicking (#411). A lockpick works a locked LF_LockComponent door by hand: slower than a key,
//! quieter than a charge, and it can fail. The server decides the time, the roll, the wear and the
//! noise; a client only asks (the door action).
//!
//! SKILL. One lockpicking skill, read from the character's level (LF_PlayerLevelComponent, already
//! replicated and saved), so no new save data. Skill 0 is a level 1 character, MAX_SKILL a level 11+.
//!
//! USES. The pick is a stack (LF_QuantityComponent): the stack count is its uses. An opened lock and
//! a snapped pick each wear one use; a plain failed attempt wears none. At zero the item is gone.
//!
//! NOISE. The attempt is a crime scene only when it finishes: a success always files an
//! LF_WantedReports report (event bus, crime record, evidence, witnesses), a failure only when the
//! noise roll says it was heard. A report never raises wanted or radios the police by itself;
//! officers learn of it from a witness.
//------------------------------------------------------------------------------------------------
enum LF_ELockpickCheck
{
	NONE,
	NOT_PICKABLE,
	NOT_LOCKED,
	NO_PICK,
	BUSY
}

//------------------------------------------------------------------------------------------------
enum LF_ELockpickResult
{
	OPENED,
	FAILED,
	SNAPPED
}

//------------------------------------------------------------------------------------------------
//! The pure rules: no world, no entity. Tests drive these directly.
class LF_LockpickRules
{
	static const int MAX_SKILL = 10;
	static const float BASE_SECONDS = 20;
	static const float MIN_SECONDS = 6;
	static const float BASE_SUCCESS = 0.55;
	static const float SUCCESS_PER_SKILL = 0.04;
	static const float MAX_SUCCESS = 0.95;
	static const float BASE_SNAP = 0.35;
	static const float SNAP_PER_SKILL = 0.025;
	static const float MIN_SNAP = 0.1;
	static const float BASE_NOISE = 0.6;
	static const float NOISE_PER_SKILL = 0.04;
	static const float MIN_NOISE = 0.15;

	//------------------------------------------------------------------------------------------------
	//! \return The lockpicking skill (0 to MAX_SKILL) of a character of this level.
	static int SkillFromLevel(int level)
	{
		return Math.Clamp(level - 1, 0, MAX_SKILL);
	}

	//------------------------------------------------------------------------------------------------
	//! \return Seconds an attempt takes: BASE_SECONDS at skill 0 falling to MIN_SECONDS at MAX_SKILL.
	static float Seconds(int skill)
	{
		skill = Math.Clamp(skill, 0, MAX_SKILL);
		return BASE_SECONDS - (BASE_SECONDS - MIN_SECONDS) * skill / MAX_SKILL;
	}

	//------------------------------------------------------------------------------------------------
	static float SuccessChance(int skill)
	{
		skill = Math.Clamp(skill, 0, MAX_SKILL);
		return Math.Min(BASE_SUCCESS + SUCCESS_PER_SKILL * skill, MAX_SUCCESS);
	}

	//------------------------------------------------------------------------------------------------
	//! Chance a failed attempt snaps the pick.
	static float SnapChance(int skill)
	{
		skill = Math.Clamp(skill, 0, MAX_SKILL);
		return Math.Max(BASE_SNAP - SNAP_PER_SKILL * skill, MIN_SNAP);
	}

	//------------------------------------------------------------------------------------------------
	//! Chance a failed attempt is loud enough for a witness.
	static float NoiseChance(int skill)
	{
		skill = Math.Clamp(skill, 0, MAX_SKILL);
		return Math.Max(BASE_NOISE - NOISE_PER_SKILL * skill, MIN_NOISE);
	}

	//------------------------------------------------------------------------------------------------
	//! \param roll Uniform 0..1 against the success chance.
	//! \param snapRoll Uniform 0..1 against the snap chance, used only on a failure.
	static LF_ELockpickResult Resolve(int skill, float roll, float snapRoll)
	{
		if (roll < SuccessChance(skill))
			return LF_ELockpickResult.OPENED;

		if (snapRoll < SnapChance(skill))
			return LF_ELockpickResult.SNAPPED;

		return LF_ELockpickResult.FAILED;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Uses left after this result. An opened lock and a snap each wear one.
	static int Wear(int uses, LF_ELockpickResult result)
	{
		if (result == LF_ELockpickResult.FAILED)
			return uses;

		return Math.Max(uses - 1, 0);
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when this result is loud enough to file a crime report.
	static bool IsHeard(LF_ELockpickResult result, int skill, float noiseRoll)
	{
		if (result == LF_ELockpickResult.OPENED)
			return true;

		return noiseRoll < NoiseChance(skill);
	}

	//------------------------------------------------------------------------------------------------
	//! The offer and server gate for one attempt.
	static LF_ELockpickCheck Check(bool pickable, bool locked, bool hasPick, bool busy)
	{
		if (!pickable)
			return LF_ELockpickCheck.NOT_PICKABLE;

		if (!locked)
			return LF_ELockpickCheck.NOT_LOCKED;

		if (!hasPick)
			return LF_ELockpickCheck.NO_PICK;

		if (busy)
			return LF_ELockpickCheck.BUSY;

		return LF_ELockpickCheck.NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The localization key shown to the player for a refused check.
	static string RefusalKey(LF_ELockpickCheck check)
	{
		switch (check)
		{
			case LF_ELockpickCheck.NOT_PICKABLE: return "#LF-Lockpick_NotPickable";
			case LF_ELockpickCheck.NOT_LOCKED: return "#LF-Lockpick_NotLocked";
			case LF_ELockpickCheck.NO_PICK: return "#LF-Lockpick_NeedPick";
			case LF_ELockpickCheck.BUSY: return "#LF-Lockpick_Busy";
		}

		return "";
	}
}

//------------------------------------------------------------------------------------------------
//! One running attempt. The user is held by id, never by reference.
class LF_LockpickAttempt : Managed
{
	EntityID m_DoorId;
	EntityID m_UserId;
	string m_sPlayerUid;
	int m_iSkill;
	float m_fSeconds;
	float m_fElapsed;
}

//------------------------------------------------------------------------------------------------
class LF_Lockpick
{
	static const string FEATURE = "Lockpick";
	static const ResourceName PICK_PREFAB = "{6B1D3F5A92C47E08}Prefabs/Items/Fence/LF_Lockpick.et";
	static const float TICK_SECONDS = 0.5;
	//! The user must stay this close to the door (metres) or the attempt is dropped.
	static const float MAX_DISTANCE = 4;

	//! Test seam: a value >= 0 replaces the skill-based attempt time.
	static float s_fSecondsOverride = -1;
	//! Test seam: forces the roll (0..1) instead of a random one. Negative means random.
	static float s_fRollOverride = -1;

	protected static ref array<ref LF_LockpickAttempt> s_aAttempts;

	//------------------------------------------------------------------------------------------------
	protected static array<ref LF_LockpickAttempt> Attempts()
	{
		if (!s_aAttempts)
			s_aAttempts = new array<ref LF_LockpickAttempt>();

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
	protected static LF_LockpickAttempt FindAttempt(EntityID doorId)
	{
		foreach (LF_LockpickAttempt attempt : Attempts())
		{
			if (attempt.m_DoorId == doorId)
				return attempt;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsBusy(IEntity door)
	{
		if (!door)
			return false;

		return FindAttempt(door.GetID()) != null;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when user carries at least one lockpick.
	static bool HasPick(IEntity user)
	{
		if (!user)
			return false;

		return LF_InventoryUtils.GetAmount(user, PICK_PREFAB) >= 1;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The lockpicking skill of this character.
	static int SkillOf(IEntity user)
	{
		int level = 1;
		LF_PlayerLevelComponent levelComponent = LF_Component<LF_PlayerLevelComponent>.Find(user);
		if (levelComponent)
			level = levelComponent.GetLevel();

		return LF_LockpickRules.SkillFromLevel(level);
	}

	//------------------------------------------------------------------------------------------------
	//! A target is a house door (LF_LockComponent) or a vehicle (LF_VehicleLockComponent, #412).
	protected static bool IsLockKnown(IEntity target)
	{
		return LF_Component<LF_LockComponent>.Find(target) != null || LF_Component<LF_VehicleLockComponent>.Find(target) != null;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsLocked(IEntity target)
	{
		LF_LockComponent doorLock = LF_Component<LF_LockComponent>.Find(target);
		if (doorLock)
			return doorLock.IsLocked();

		LF_VehicleLockComponent vehicleLock = LF_Component<LF_VehicleLockComponent>.Find(target);
		return vehicleLock && vehicleLock.IsVehicleLocked();
	}

	//------------------------------------------------------------------------------------------------
	//! eturn True when the pick action should be listed on this target: a pick target that is locked.
	static bool ShowsPick(IEntity target)
	{
		return target && LF_Component<LF_LockpickTargetComponent>.Find(target) != null && IsLockKnown(target) && IsLocked(target);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: opens the lock of a door or vehicle. A vehicle's saved lock flag follows.
	protected static void Unlock(IEntity target)
	{
		LF_LockComponent doorLock = LF_Component<LF_LockComponent>.Find(target);
		if (doorLock)
		{
			doorLock.Unlock();
			return;
		}

		LF_VehicleLockComponent vehicleLock = LF_Component<LF_VehicleLockComponent>.Find(target);
		if (!vehicleLock)
			return;

		vehicleLock.SetLocked(false);
		LF_VehicleOwnership.NoteLockState(vehicleLock.GetVehicleIdentifier(), false);
	}

	//------------------------------------------------------------------------------------------------
	//! Offer check, used by the door action on client and server. Reads only.
	static LF_ELockpickCheck CheckDoor(IEntity door, IEntity user)
	{
		LF_LockpickTargetComponent target = LF_Component<LF_LockpickTargetComponent>.Find(door);
		bool known = IsLockKnown(door);
		bool pickable = target && known && target.IsPickable();
		bool locked = known && IsLocked(door);
		return LF_LockpickRules.Check(pickable, locked, HasPick(user), IsBusy(door));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: starts an attempt on a door. The attempt then runs on a timer for the skill-based
	//! time and is dropped when the user walks off or dies.
	//! \return True when an attempt started.
	static bool Begin(IEntity door, IEntity user)
	{
		if (!user)
		{
			LF_Debug.Error(FEATURE, "begin refused: door or user is null");
			return false;
		}

		return BeginAs(door, user, LF_Utils.GetPlayerUid(user));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: Begin with the picker's uid given (the seam a WORLD test drives; a spawned test
	//! character has no player behind it).
	static bool BeginAs(IEntity door, IEntity user, string playerUid)
	{
		if (!Replication.IsServer())
			return false;

		if (!door || !user)
		{
			LF_Debug.Error(FEATURE, "begin refused: door or user is null");
			return false;
		}

		if (playerUid.IsEmpty())
		{
			LF_Debug.Error(FEATURE, "begin refused: player has no account id");
			return false;
		}

		LF_ELockpickCheck check = CheckDoor(door, user);
		if (check != LF_ELockpickCheck.NONE)
		{
			LF_Debug.Info(FEATURE, string.Format("refused %1 at %2: %3", playerUid, door.GetName(), typename.EnumToString(LF_ELockpickCheck, check)));
			LF_Utils.NotifyEntity(user, LF_LockpickRules.RefusalKey(check), "#LF-Lockpick_Title", 3.0);
			return false;
		}

		LF_LockpickAttempt attempt = new LF_LockpickAttempt();
		attempt.m_DoorId = door.GetID();
		attempt.m_UserId = user.GetID();
		attempt.m_sPlayerUid = playerUid;
		attempt.m_iSkill = SkillOf(user);
		attempt.m_fSeconds = LF_LockpickRules.Seconds(attempt.m_iSkill);
		if (s_fSecondsOverride >= 0)
			attempt.m_fSeconds = s_fSecondsOverride;

		Attempts().Insert(attempt);
		LF_Debug.Info(FEATURE, string.Format("%1 starts picking %2: skill=%3 seconds=%4", playerUid, door.GetName(), attempt.m_iSkill, attempt.m_fSeconds));
		LF_Utils.NotifyEntity(user, "#LF-Lockpick_Started", "#LF-Lockpick_Title", 3.0);

		if (attempt.m_fSeconds <= 0)
			Complete(attempt);
		else
			GetGame().GetCallqueue().CallLater(Tick, TICK_SECONDS * 1000, false, attempt);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Timer step for one door's attempt.
	protected static void Tick(LF_LockpickAttempt attempt)
	{
		if (!attempt || Attempts().Find(attempt) == -1)
			return;

		IEntity door = GetGame().GetWorld().FindEntityByID(attempt.m_DoorId);
		IEntity user = GetGame().GetWorld().FindEntityByID(attempt.m_UserId);
		if (!door || !user)
		{
			Drop(attempt, null, "door or user is gone");
			return;
		}

		CharacterControllerComponent controller = LF_Component<CharacterControllerComponent>.Find(user);
		if (controller && controller.GetLifeState() != ECharacterLifeState.ALIVE)
		{
			Drop(attempt, user, "user is down");
			return;
		}

		if (vector.Distance(user.GetOrigin(), door.GetOrigin()) > MAX_DISTANCE)
		{
			Drop(attempt, user, "user walked away");
			return;
		}

		if (!HasPick(user))
		{
			Drop(attempt, user, "the pick left the picker's inventory");
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
	protected static void Drop(LF_LockpickAttempt attempt, IEntity user, string reason)
	{
		Attempts().RemoveItem(attempt);
		LF_Debug.Info(FEATURE, string.Format("attempt by %1 dropped: %2", attempt.m_sPlayerUid, reason));
		if (user)
			LF_Utils.NotifyEntity(user, "#LF-Lockpick_Cancelled", "#LF-Lockpick_Title", 3.0);
	}

	//------------------------------------------------------------------------------------------------
	//! Test seam: finish the running attempt on this door now, as the timer would when it runs out.
	//! \return False when nobody is picking the door.
	static bool CompleteNow(IEntity door)
	{
		if (!door)
			return false;

		LF_LockpickAttempt attempt = FindAttempt(door.GetID());
		if (!attempt)
			return false;

		Complete(attempt);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Rolls the attempt, wears the pick, opens the lock, and files the report when it was heard.
	protected static void Complete(LF_LockpickAttempt attempt)
	{
		Attempts().RemoveItem(attempt);

		IEntity door = GetGame().GetWorld().FindEntityByID(attempt.m_DoorId);
		IEntity user = GetGame().GetWorld().FindEntityByID(attempt.m_UserId);
		if (!door || !user || !IsLockKnown(door))
		{
			LF_Debug.Error(FEATURE, "attempt finished but the door or user is gone");
			return;
		}

		// The door may have been opened (a key, a charge) while the pick worked.
		if (!IsLocked(door))
		{
			LF_Debug.Info(FEATURE, string.Format("attempt by %1 ends: %2 is already open", attempt.m_sPlayerUid, door.GetName()));
			return;
		}

		// The pick is checked only at Begin otherwise: a picker who drops or hands it over mid-attempt
		// would open the lock and never wear it.
		if (!HasPick(user))
		{
			Drop(attempt, user, "the pick left the picker's inventory");
			return;
		}

		float roll = Math.RandomFloat01();
		if (s_fRollOverride >= 0)
			roll = s_fRollOverride;

		LF_ELockpickResult result = LF_LockpickRules.Resolve(attempt.m_iSkill, roll, Math.RandomFloat01());
		if (result == LF_ELockpickResult.OPENED)
			Unlock(door);

		int wear = 0;
		if (LF_LockpickRules.Wear(1, result) < 1)
			wear = LF_InventoryUtils.RemoveAmount(user, PICK_PREFAB, 1);

		LF_Debug.Info(FEATURE, string.Format("%1 on %2: %3 (skill %4, roll %5, wear %6)", attempt.m_sPlayerUid, door.GetName(),
			typename.EnumToString(LF_ELockpickResult, result), attempt.m_iSkill, roll, wear));

		string messageKey = "#LF-Lockpick_Opened";
		if (result == LF_ELockpickResult.FAILED)
			messageKey = "#LF-Lockpick_Failed";
		else if (result == LF_ELockpickResult.SNAPPED)
			messageKey = "#LF-Lockpick_Snapped";

		LF_Utils.NotifyEntity(user, messageKey, "#LF-Lockpick_Title", 3.0);

		if (LF_LockpickRules.IsHeard(result, attempt.m_iSkill, Math.RandomFloat01()))
			LF_WantedReports.Report(attempt.m_sPlayerUid, user, LF_WantedReports.OFFENCE_LOCKPICK);
	}
}
