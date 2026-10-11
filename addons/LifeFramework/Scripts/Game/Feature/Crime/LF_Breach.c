//------------------------------------------------------------------------------------------------
//! Forced entry. A configured charge opens an EL_LockComponent after a fuse and leaves it
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
}

//------------------------------------------------------------------------------------------------
class LF_Breach
{
	static const ResourceName CHARGE_PREFAB = "{33CBDE73AB48172A}Prefabs/Weapons/Explosives/DemoBlock_M112/DemoBlock_M112.et";

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
			EL_Debug.Error("Breach", "configure refused: empty target id");
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
		EL_Debug.Log("Breach", string.Format("configured %1 fuse=%2", targetId, fuseSeconds));
	}

	//------------------------------------------------------------------------------------------------
	//! Records that playerUid belongs to organizationId, so they can repair that org's door.
	static void SetOrganizationMember(string playerUid, string organizationId)
	{
		if (playerUid.IsEmpty())
		{
			EL_Debug.Error("Breach", "organization member refused: empty player uid");
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
	//! Arms a configured target for playerUid. Opens its EL_LockComponent after the fuse and
	//! leaves it open until Repair. Wanted and the police notify fire once, here, including
	//! when the fuse is still running. Returns false when the target is not configured, a
	//! charge is already on it, the player id is empty, or the charge prefab does not load.
	static bool Arm(string targetId, string playerUid)
	{
		if (targetId.IsEmpty() || playerUid.IsEmpty())
		{
			EL_Debug.Error("Breach", "arm refused: target id or player uid is empty");
			return false;
		}

		LF_BreachTarget target = Targets().Get(targetId);
		if (!target)
		{
			EL_Debug.Error("Breach", string.Format("arm refused: target %1 is not configured", targetId));
			return false;
		}

		if (target.m_bOpen || target.m_bChargeArmed)
		{
			EL_Debug.Log("Breach", string.Format("arm refused: target %1 already open or armed", targetId));
			return false;
		}

		if (!ChargePrefabLoads())
			return false;

		EL_PlayerAccount account = EL_PlayerAccountManager.GetOrCreate(playerUid);
		if (!account)
		{
			EL_Debug.Error("Breach", string.Format("arm refused: no account for %1", playerUid));
			return false;
		}

		account.IncreaseWantedLevel(1);
		EL_PlayerAccountManager accountManager = EL_PlayerAccountManager.GetInstance();
		if (accountManager)
			accountManager.SaveAndReleaseAccount(account);

		target.m_iPoliceNotifies = target.m_iPoliceNotifies + 1;
		EL_Utils.AlertPolice("#EL-Police_Alert_Breach", TargetOrigin(targetId));
		EL_Debug.Log("Breach", string.Format("armed %1 by %2 fuse=%3", targetId, playerUid, target.m_fFuseSeconds));

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
			EL_Debug.Error("Breach", "repair refused: target id or player uid is empty");
			return false;
		}

		LF_BreachTarget target = Targets().Get(targetId);
		if (!target)
		{
			EL_Debug.Error("Breach", string.Format("repair refused: target %1 is not configured", targetId));
			return false;
		}

		if (!PlayerMayRepair(target, playerUid))
		{
			EL_Debug.Log("Breach", string.Format("repair refused: %1 cannot repair %2", playerUid, targetId));
			return false;
		}

		target.m_bOpen = false;
		target.m_bChargeArmed = false;
		target.m_fElapsed = 0;
		WriteLock(targetId, true);
		EL_Debug.Log("Breach", string.Format("repaired %1 by %2", targetId, playerUid));
		return true;
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

		EL_LockComponent doorLock = FindLock(targetId);
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
			EL_Debug.Error("Breach", string.Format("export refused: target %1 is not configured", targetId));
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
			EL_Debug.Error("Breach", "apply refused: record is null");
			return false;
		}

		LF_BreachTarget target = Targets().Get(record.m_sTargetId);
		if (!target || !record.IsValidFor(target.m_sTargetId))
		{
			EL_Debug.Error("Breach", "apply refused: record does not match a configured target");
			return false;
		}

		target.m_bOpen = record.m_bOpen;
		target.m_bChargeArmed = false;
		target.m_fElapsed = 0;
		WriteLock(target.m_sTargetId, !record.m_bOpen);
		EL_Debug.Log("Breach", string.Format("apply-save %1 open=%2", target.m_sTargetId, record.m_bOpen));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool PlayerMayRepair(LF_BreachTarget target, string playerUid)
	{
		EL_PlayerAccount account = EL_PlayerAccountManager.GetOrCreate(playerUid);
		if (account && account.GetFaction() == EL_Faction.POLICE)
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
		EL_Debug.Log("Breach", string.Format("lock opened: %1", target.m_sTargetId));
	}

	//------------------------------------------------------------------------------------------------
	protected static bool ChargePrefabLoads()
	{
		Resource charge = Resource.Load(CHARGE_PREFAB);
		if (!charge.IsValid())
		{
			EL_Debug.Error("Breach", "charge prefab missing: " + CHARGE_PREFAB);
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static void WriteLock(string targetId, bool locked)
	{
		EL_LockComponent doorLock = FindLock(targetId);
		if (!doorLock)
			return;

		if (locked)
			doorLock.Lock();
		else
			doorLock.Unlock();
	}

	//------------------------------------------------------------------------------------------------
	protected static EL_LockComponent FindLock(string targetId)
	{
		if (!GetGame() || !GetGame().GetWorld())
			return null;

		IEntity entity = GetGame().GetWorld().FindEntityByName(targetId);
		if (!entity)
			return null;

		EL_LockComponent doorLock = EL_Component<EL_LockComponent>.Find(entity);
		if (!doorLock)
			EL_Debug.Error("Breach", string.Format("target %1 has no EL_LockComponent", targetId));

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

		return entity.GetOrigin();
	}

	//------------------------------------------------------------------------------------------------
	protected static void ScheduleFuse(string targetId, float fuseSeconds)
	{
		if (fuseSeconds <= 0)
			return;

		if (!GetGame() || !GetGame().GetWorld())
			return;

		IEntity entity = GetGame().GetWorld().FindEntityByName(targetId);
		if (!entity)
			return;

		LF_BreachTargetComponent component = EL_Component<LF_BreachTargetComponent>.Find(entity);
		if (!component)
		{
			EL_Debug.Error("Breach", string.Format("fuse timer skipped: %1 has no LF_BreachTargetComponent", targetId));
			return;
		}

		component.ScheduleFuse(fuseSeconds);
	}
}
