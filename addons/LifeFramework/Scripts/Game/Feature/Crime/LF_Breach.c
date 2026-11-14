//------------------------------------------------------------------------------------------------
//! Forced entry. A configured charge opens an LF_LockComponent after a fuse and leaves it
//! open until repair. Vanilla charge detonation is not involved: the demo block is only the
//! item the action requires to exist.
//!
//! PUBLIC. Later scopes call Arm(targetId, playerUid). targetId is the stable world id
//! (Roadmap_Breach_Door on the release door). The wanted bump and the police notify happen
//! once, inside Arm, never from an offer refresh. Fuse 0 opens before Arm returns. A positive
//! fuse stays locked until Advance, and a placed world door also schedules that Advance.
//!
//! PERSISTENCE. Export copies the open flag. Apply writes it back through the same mutators
//! onto the live lock. The key is the target id, never an EntityID.
//------------------------------------------------------------------------------------------------
class LF_BreachRecord : Managed
{
	static const int VERSION = 1;

	string m_sTargetId;
	bool m_bOpen;

	//------------------------------------------------------------------------------------------------
	static LF_BreachRecord Create(string targetId, bool isOpen)
	{
		LF_BreachRecord record();
		record.m_sTargetId = targetId;
		record.m_bOpen = isOpen;
		return record;
	}

	//------------------------------------------------------------------------------------------------
	bool IsValidFor(string targetId)
	{
		if (m_sTargetId.IsEmpty() || targetId.IsEmpty())
			return false;

		return m_sTargetId == targetId;
	}
}

//------------------------------------------------------------------------------------------------
class LF_BreachTarget : Managed
{
	string m_sTargetId;
	float m_fFuseSeconds;
	bool m_bOpen;
	bool m_bChargeArmed;
	float m_fElapsed;
	int m_iPoliceNotifies;
	string m_sOwnerOrganization;
	//! Offence the arm reports. Empty is the plain break-in.
	string m_sOffence;
	//! The loud alarm even with officers on duty.
	bool m_bLoud;
	//! The robbery type's alarm (an LF_AlarmKind id). Empty takes the offence's kind, else the plain siren.
	string m_sAlarmKind;
}

//------------------------------------------------------------------------------------------------
class LF_Breach
{
	static const ResourceName CHARGE_PREFAB = "{33CBDE73AB48172A}Prefabs/Weapons/Explosives/DemoBlock_M112/DemoBlock_M112.et";
	//! The fence charge (#271), serial-stamped; it forces a door the same as the vanilla block.
	static const ResourceName FENCE_CHARGE_PREFAB = "{E78C99E5029D52A5}Prefabs/Items/Fence/LF_Charge.et";

	protected static ref map<string, ref LF_BreachTarget> s_mTargets;
	protected static ref map<string, string> s_mOrganizationMembers;

	//------------------------------------------------------------------------------------------------
	protected static map<string, ref LF_BreachTarget> Targets()
	{
		if (!s_mTargets)
			s_mTargets = new map<string, ref LF_BreachTarget>();

		return s_mTargets;
	}

	//------------------------------------------------------------------------------------------------
	protected static map<string, string> OrganizationMembers()
	{
		if (!s_mOrganizationMembers)
			s_mOrganizationMembers = new map<string, string>();

		return s_mOrganizationMembers;
	}

	//------------------------------------------------------------------------------------------------
	//! Registers a door, gate, or container that Arm is allowed to open.
	//! \param targetId Stable world id. Empty is refused.
	//! \param fuseSeconds Seconds until the lock opens. Zero opens inside Arm.
	//! \param ownerOrganization Organization that may repair this target. Empty means police only.
	static void Configure(string targetId, float fuseSeconds, string ownerOrganization)
	{
		if (targetId.IsEmpty())
		{
			LF_Debug.Error("Breach", "configure refused: empty target id");
			return;
		}

		LF_BreachTarget target = Targets().Get(targetId);
		if (!target)
		{
			target = new LF_BreachTarget();
			target.m_sTargetId = targetId;
			Targets().Set(targetId, target);
		}

		target.m_fFuseSeconds = fuseSeconds;
		target.m_sOwnerOrganization = ownerOrganization;
		// A re-configure (world reload, door re-streamed) drops any charge whose fuse timer died
		// with the old entity. The open flag is kept; Apply/Repair own it.
		target.m_bChargeArmed = false;
		target.m_fElapsed = 0;
		LF_Debug.Log("Breach", string.Format("configured %1 fuse=%2", targetId, fuseSeconds));
	}

	//------------------------------------------------------------------------------------------------
	//! How arming this target is reported (#274, #275): the offence key and whether the alarm is the
	//! loud one. Empty offence keeps the plain break-in.
	static void SetAlarm(string targetId, string offence, bool loud)
	{
		LF_BreachTarget target = Targets().Get(targetId);
		if (!target)
		{
			LF_Debug.Error("Breach", string.Format("alarm refused: target %1 is not configured", targetId));
			return;
		}

		target.m_sOffence = offence;
		target.m_bLoud = loud;
	}

	//------------------------------------------------------------------------------------------------
	//! Which alarm kind arming this target sounds (a bank vault's bell). Empty is the offence's kind.
	static void SetAlarmKind(string targetId, string alarmKind)
	{
		LF_BreachTarget target = Targets().Get(targetId);
		if (!target)
		{
			LF_Debug.Error("Breach", string.Format("alarm kind refused: target %1 is not configured", targetId));
			return;
		}

		target.m_sAlarmKind = alarmKind;
	}

	//------------------------------------------------------------------------------------------------
	static string AlarmKind(string targetId)
	{
		LF_BreachTarget target = Targets().Get(targetId);
		if (!target)
			return "";

		return target.m_sAlarmKind;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: locks the target again with no player check. The cash van is stocked afresh at its
	//! next stop and the armory store is refilled; neither is a player repair.
	static void Reset(string targetId)
	{
		LF_BreachTarget target = Targets().Get(targetId);
		if (!target)
			return;

		target.m_bOpen = false;
		target.m_bChargeArmed = false;
		target.m_fElapsed = 0;
		WriteLock(targetId, true);
		SyncTarget(target);
	}

	//------------------------------------------------------------------------------------------------
	//! Records that playerUid belongs to organizationId, so they can repair that org's door.
	static void SetOrganizationMember(string playerUid, string organizationId)
	{
		if (playerUid.IsEmpty())
		{
			LF_Debug.Error("Breach", "organization member refused: empty player uid");
			return;
		}

		OrganizationMembers().Set(playerUid, organizationId);
	}

	//------------------------------------------------------------------------------------------------
	//! Offer check. Does not raise wanted and does not notify police.
	static bool CanArm(string targetId)
	{
		LF_BreachTarget target = Targets().Get(targetId);
		if (!target)
			return false;

		if (target.m_bOpen || target.m_bChargeArmed)
			return false;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when elapsedSeconds has reached a fuse of fuseSeconds. A fuse of 0 is already elapsed.
	static bool FuseElapsed(float fuseSeconds, float elapsedSeconds)
	{
		if (fuseSeconds <= 0)
			return true;

		return elapsedSeconds >= fuseSeconds;
	}

	//------------------------------------------------------------------------------------------------
	//! Arms a configured target for playerUid. Opens its LF_LockComponent after the fuse and
	//! leaves it open until Repair. Wanted and the police notify fire once, here, including
	//! when the fuse is still running. Returns false when the target is not configured, a
	//! charge is already on it, the player id is empty, or the charge prefab does not load.
	//! \param chargeCarrier The player entity placing the charge. Every player-facing action
	//! passes it: one DemoBlock charge is taken from that inventory before anything happens,
	//! and Arm refuses when there is none. Null is the server-scripted arm (tests, events).
	static bool Arm(string targetId, string playerUid, IEntity chargeCarrier = null, IEntity suspect = null)
	{
		if (targetId.IsEmpty() || playerUid.IsEmpty())
		{
			LF_Debug.Error("Breach", "arm refused: target id or player uid is empty");
			return false;
		}

		LF_BreachTarget target = Targets().Get(targetId);
		if (!target)
		{
			LF_Debug.Error("Breach", string.Format("arm refused: target %1 is not configured", targetId));
			return false;
		}

		if (target.m_bOpen || target.m_bChargeArmed)
		{
			LF_Debug.Log("Breach", string.Format("arm refused: target %1 already open or armed", targetId));
			return false;
		}

		if (!ChargePrefabLoads())
			return false;

		LF_PlayerAccount account = LF_PlayerAccountManager.GetOrCreate(playerUid);
		if (!account)
		{
			LF_Debug.Error("Breach", string.Format("arm refused: no account for %1", playerUid));
			return false;
		}

		if (chargeCarrier && !TakeCharge(chargeCarrier, targetId))
			return false;

		string offence = target.m_sOffence;
		if (offence.IsEmpty())
			offence = LF_WantedReports.OFFENCE_BREAK_IN;

		LF_CrimeCatalog.ApplyWanted(account, playerUid, offence);
		LF_PlayerAccountManager accountManager = LF_PlayerAccountManager.GetInstance();
		if (accountManager)
			accountManager.SaveAndReleaseAccount(account);

		target.m_iPoliceNotifies = target.m_iPoliceNotifies + 1;
		// The alarm sounds at the door or vault and on-duty officers are radioed (LF_Dispatch).
		if (!suspect)
			suspect = chargeCarrier;

		LF_Dispatch.Report(playerUid, suspect, offence, TargetOrigin(targetId), -1, target.m_bLoud, target.m_sAlarmKind);
		LF_Debug.Log("Breach", string.Format("armed %1 by %2 fuse=%3", targetId, playerUid, target.m_fFuseSeconds));

		target.m_bChargeArmed = true;
		target.m_fElapsed = 0;

		if (FuseElapsed(target.m_fFuseSeconds, 0))
			Open(target);
		else
			ScheduleFuse(targetId, target.m_fFuseSeconds);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Moves a running fuse forward. Opens the lock once the configured fuse has elapsed.
	//! \return True when the target is open after the step.
	static bool Advance(string targetId, float elapsedSeconds)
	{
		LF_BreachTarget target = Targets().Get(targetId);
		if (!target)
			return false;

		if (!target.m_bChargeArmed)
			return target.m_bOpen;

		target.m_fElapsed = target.m_fElapsed + elapsedSeconds;
		if (FuseElapsed(target.m_fFuseSeconds, target.m_fElapsed))
			Open(target);

		return target.m_bOpen;
	}

	//------------------------------------------------------------------------------------------------
	//! Drops a charge whose fuse can no longer fire (timer could not be scheduled, or the door was
	//! deleted before it fired) so the target does not stay armed forever. The lock is unchanged.
	static void CancelCharge(string targetId, string reason)
	{
		LF_BreachTarget target = Targets().Get(targetId);
		if (!target || !target.m_bChargeArmed)
			return;

		target.m_bChargeArmed = false;
		target.m_fElapsed = 0;
		SyncTarget(target);
		LF_Debug.Error("Breach", string.Format("charge cancelled on %1: %2", targetId, reason));
	}

	//------------------------------------------------------------------------------------------------
	//! Drops every registered target and organization member. Called on game-mode start/end
	//! so a world reload in the same process does not inherit the previous world's state.
	static void ResetStatics()
	{
		s_mTargets = null;
		s_mOrganizationMembers = null;
	}

	//------------------------------------------------------------------------------------------------
	static bool CanRepair(string targetId, string playerUid)
	{
		if (playerUid.IsEmpty())
			return false;

		LF_BreachTarget target = Targets().Get(targetId);
		if (!target)
			return false;

		return PlayerMayRepair(target, playerUid);
	}

	//------------------------------------------------------------------------------------------------
	//! Locks the target again. Police can always repair. The owning organization can repair its
	//! own door. Anyone else is refused and the lock stays as it was.
	static bool Repair(string targetId, string playerUid)
	{
		if (targetId.IsEmpty() || playerUid.IsEmpty())
		{
			LF_Debug.Error("Breach", "repair refused: target id or player uid is empty");
			return false;
		}

		LF_BreachTarget target = Targets().Get(targetId);
		if (!target)
		{
			LF_Debug.Error("Breach", string.Format("repair refused: target %1 is not configured", targetId));
			return false;
		}

		if (!PlayerMayRepair(target, playerUid))
		{
			LF_Debug.Log("Breach", string.Format("repair refused: %1 cannot repair %2", playerUid, targetId));
			return false;
		}

		target.m_bOpen = false;
		target.m_bChargeArmed = false;
		target.m_fElapsed = 0;
		WriteLock(targetId, true);
		SyncTarget(target);
		LF_Debug.Log("Breach", string.Format("repaired %1 by %2", targetId, playerUid));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//------------------------------------------------------------------------------------------------
	//! True when arming this target sounds the loud alarm.
	static bool IsLoud(string targetId)
	{
		LF_BreachTarget target = Targets().Get(targetId);
		return target && target.m_bLoud;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsOpen(string targetId)
	{
		return !IsLocked(targetId);
	}

	//------------------------------------------------------------------------------------------------
	//! Live lock when the named entity is in the world. Otherwise the in-memory open flag.
	static bool IsLocked(string targetId)
	{
		LF_BreachTarget target = Targets().Get(targetId);
		if (!target)
			return true;

		LF_LockComponent doorLock = FindLock(targetId);
		if (doorLock)
			return doorLock.IsLocked();

		return !target.m_bOpen;
	}

	//------------------------------------------------------------------------------------------------
	static int GetPoliceNotifyCount(string targetId)
	{
		LF_BreachTarget target = Targets().Get(targetId);
		if (!target)
			return 0;

		return target.m_iPoliceNotifies;
	}

	//------------------------------------------------------------------------------------------------
	static LF_BreachRecord Export(string targetId)
	{
		LF_BreachTarget target = Targets().Get(targetId);
		if (!target)
		{
			LF_Debug.Error("Breach", string.Format("export refused: target %1 is not configured", targetId));
			return null;
		}

		return LF_BreachRecord.Create(targetId, target.m_bOpen);
	}

	//------------------------------------------------------------------------------------------------
	//! Writes a saved open/locked flag back onto the configured target and its lock.
	//! Idempotent. A record for another id is rejected.
	static bool Apply(LF_BreachRecord record)
	{
		if (!record)
		{
			LF_Debug.Error("Breach", "apply refused: record is null");
			return false;
		}

		LF_BreachTarget target = Targets().Get(record.m_sTargetId);
		if (!target || !record.IsValidFor(target.m_sTargetId))
		{
			LF_Debug.Error("Breach", "apply refused: record does not match a configured target");
			return false;
		}

		target.m_bOpen = record.m_bOpen;
		target.m_bChargeArmed = false;
		target.m_fElapsed = 0;
		WriteLock(target.m_sTargetId, !record.m_bOpen);
		SyncTarget(target);
		LF_Debug.Log("Breach", string.Format("apply-save %1 open=%2", target.m_sTargetId, record.m_bOpen));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool PlayerMayRepair(LF_BreachTarget target, string playerUid)
	{
		LF_PlayerAccount account = LF_PlayerAccountManager.GetOrCreate(playerUid);
		if (account && account.GetFaction() == LF_Faction.POLICE)
			return true;

		if (target.m_sOwnerOrganization.IsEmpty())
			return false;

		if (!OrganizationMembers().Contains(playerUid))
			return false;

		return OrganizationMembers().Get(playerUid) == target.m_sOwnerOrganization;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Open(LF_BreachTarget target)
	{
		target.m_bOpen = true;
		target.m_bChargeArmed = false;
		WriteLock(target.m_sTargetId, false);
		SyncTarget(target);
		LF_Debug.Log("Breach", string.Format("lock opened: %1", target.m_sTargetId));
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when user carries at least one breach charge.
	static bool HasCharge(IEntity user)
	{
		if (!user)
			return false;

		if (LF_InventoryUtils.GetAmount(user, CHARGE_PREFAB) >= 1 || LF_InventoryUtils.GetAmount(user, FENCE_CHARGE_PREFAB) >= 1)
			return true;

		return FindEquippedCharge(user) != null;
	}

	//------------------------------------------------------------------------------------------------
	//! The M112 is a weapon: picked up, it lands in the equipped-weapon storage, which the
	//! deposit-count GetAmount/RemoveAmount do not cover. Finds a charge there.
	protected static IEntity FindEquippedCharge(IEntity user)
	{
		EquipedWeaponStorageComponent weaponStorage = LF_Component<EquipedWeaponStorageComponent>.Find(user);
		if (!weaponStorage)
			return null;

		array<IEntity> weapons = {};
		weaponStorage.GetAll(weapons);
		foreach (IEntity weapon : weapons)
		{
			if (weapon && (LF_Utils.SamePrefab(LF_Utils.GetPrefabName(weapon), CHARGE_PREFAB) || LF_Utils.SamePrefab(LF_Utils.GetPrefabName(weapon), FENCE_CHARGE_PREFAB)))
				return weapon;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Removes one breach charge from user. Logs and refuses when there is none.
	protected static bool TakeCharge(IEntity user, string targetId)
	{
		if (!HasCharge(user))
		{
			LF_Debug.Warn("Breach", string.Format("arm refused: %1 carries no breach charge", targetId));
			return false;
		}

		int removed = 0;
		if (LF_InventoryUtils.GetAmount(user, FENCE_CHARGE_PREFAB) >= 1)
			removed = LF_InventoryUtils.RemoveAmount(user, FENCE_CHARGE_PREFAB, 1);

		if (removed != 1 && LF_InventoryUtils.GetAmount(user, CHARGE_PREFAB) >= 1)
			removed = LF_InventoryUtils.RemoveAmount(user, CHARGE_PREFAB, 1);

		if (removed != 1)
		{
			IEntity equipped = FindEquippedCharge(user);
			InventoryStorageManagerComponent storage = LF_InventoryUtils.GetResponsibleStorageManager(user);
			if (equipped && storage && storage.TryDeleteItem(equipped))
				removed = 1;
		}

		if (removed != 1)
		{
			LF_Debug.Error("Breach", string.Format("arm refused: %1 charge could not be taken (removed %2)", targetId, removed));
			return false;
		}

		LF_Debug.Log("Breach", string.Format("charge used on %1", targetId));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Mirrors the open/armed flags onto the placed door so client offers can read them.
	//! A target without a placed entity (logic tests) has nothing to mirror.
	protected static void SyncTarget(LF_BreachTarget target)
	{
		if (!target || !GetGame() || !GetGame().GetWorld())
			return;

		IEntity entity = GetGame().GetWorld().FindEntityByName(target.m_sTargetId);
		if (!entity)
			return;

		LF_BreachTargetComponent component = LF_Component<LF_BreachTargetComponent>.Find(entity);
		if (component)
			component.SetReplicatedState(target.m_bOpen, target.m_bChargeArmed);
	}

	//------------------------------------------------------------------------------------------------
	protected static bool ChargePrefabLoads()
	{
		Resource charge = Resource.Load(CHARGE_PREFAB);
		if (!charge.IsValid())
		{
			LF_Debug.Error("Breach", "charge prefab missing: " + CHARGE_PREFAB);
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static void WriteLock(string targetId, bool locked)
	{
		LF_LockComponent doorLock = FindLock(targetId);
		if (!doorLock)
			return;

		if (locked)
			doorLock.Lock();
		else
			doorLock.Unlock();
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_LockComponent FindLock(string targetId)
	{
		if (!GetGame() || !GetGame().GetWorld())
			return null;

		IEntity entity = GetGame().GetWorld().FindEntityByName(targetId);
		if (!entity)
			return null;

		LF_LockComponent doorLock = LF_Component<LF_LockComponent>.Find(entity);
		if (!doorLock)
			LF_Debug.Error("Breach", string.Format("target %1 has no LF_LockComponent", targetId));

		return doorLock;
	}

	//------------------------------------------------------------------------------------------------
	protected static vector TargetOrigin(string targetId)
	{
		vector origin = "0 0 0";
		if (!GetGame() || !GetGame().GetWorld())
			return origin;

		IEntity entity = GetGame().GetWorld().FindEntityByName(targetId);
		if (!entity)
			return origin;

		// World position: the cash van door is a child of the van and moves with it.
		vector transform[4];
		entity.GetWorldTransform(transform);
		return transform[3];
	}

	//------------------------------------------------------------------------------------------------
	protected static void ScheduleFuse(string targetId, float fuseSeconds)
	{
		if (fuseSeconds <= 0)
			return;

		if (!GetGame() || !GetGame().GetWorld())
		{
			CancelCharge(targetId, "fuse timer skipped: no world");
			return;
		}

		// An unplaced target is script-driven: its fuse advances only through Advance.
		IEntity entity = GetGame().GetWorld().FindEntityByName(targetId);
		if (!entity)
		{
			LF_Debug.Log("Breach", string.Format("fuse timer not scheduled: %1 is not a placed entity; Advance drives it", targetId));
			return;
		}

		LF_BreachTargetComponent component = LF_Component<LF_BreachTargetComponent>.Find(entity);
		if (!component)
		{
			CancelCharge(targetId, "fuse timer skipped: entity has no LF_BreachTargetComponent");
			return;
		}

		if (!component.ScheduleFuse(fuseSeconds))
		{
			CancelCharge(targetId, "fuse timer could not be scheduled");
			return;
		}

		component.SetReplicatedState(false, true);
	}
}
