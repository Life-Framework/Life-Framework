//------------------------------------------------------------------------------------------------
//! One building owned by an organization. Members unlock its door. The processing
//! machine runs only when that organization owns the hideout and has bought the
//! processing slot. A refusal is logged and the door or machine stays unused.
//!
//! The door identifier is the organization id, written onto the bound
//! LF_LockComponent. Export / Apply replace that owner and the lock flag.
//! Apply is idempotent. The game-mode serializer writes these records at version 3.
//------------------------------------------------------------------------------------------------
class LF_HideoutRecord : Managed
{
	static const int VERSION = 1;

	int m_iVersion;
	string m_sHideoutId;
	string m_sOrganizationId;
	bool m_bLocked;

	//------------------------------------------------------------------------------------------------
	static LF_HideoutRecord Create(string hideoutId, string organizationId, bool locked)
	{
		LF_HideoutRecord record();
		record.m_iVersion = VERSION;
		record.m_sHideoutId = hideoutId;
		record.m_sOrganizationId = organizationId;
		record.m_bLocked = locked;
		return record;
	}
}

//------------------------------------------------------------------------------------------------
//! Held ground on a hideout (#751, "heldground" payload, docs/architecture/record-shape-held-ground.md).
//! One row per hideout whose held ground is not the default, keyed by hideout id (a place id). The
//! holder is always the leasing organization: a row whose holder does not lease the site is dropped
//! on load (a lapsed lease or a disbanded organization loses its points). The member minimum and the
//! points per hour are owner config, never saved. A later field is a new payload, never a new member.
//------------------------------------------------------------------------------------------------
class LF_HeldGroundRow
{
	static const int VERSION = 1;

	int m_iVersion;
	string m_sHideoutId;
	string m_sHolderOrgId;
	int m_iHoldSeconds;
	int m_iHoldPoints;

	//------------------------------------------------------------------------------------------------
	static LF_HeldGroundRow Create(string hideoutId, string holderOrgId, int holdSeconds, int holdPoints)
	{
		LF_HeldGroundRow row();
		row.m_iVersion = VERSION;
		row.m_sHideoutId = hideoutId;
		row.m_sHolderOrgId = holderOrgId;
		row.m_iHoldSeconds = holdSeconds;
		row.m_iHoldPoints = holdPoints;
		return row;
	}
}

//------------------------------------------------------------------------------------------------
//! Idle clock of a hideout lease (#2573, "leaseidle" payload): the last real time (unix seconds) a
//! member of the holding organization was connected. One row per leased hideout, keyed by hideout id
//! (a place id). The limit is owner config (LF_OrgConfig.HideoutLeaseIdleDays), never saved. A later
//! field is a new payload, never a new member.
//------------------------------------------------------------------------------------------------
class LF_HideoutIdleRow
{
	static const int VERSION = 1;

	int m_iVersion;
	string m_sHideoutId;
	int m_iLastActive;

	//------------------------------------------------------------------------------------------------
	static LF_HideoutIdleRow Create(string hideoutId, int lastActive)
	{
		LF_HideoutIdleRow row();
		row.m_iVersion = VERSION;
		row.m_sHideoutId = hideoutId;
		row.m_iLastActive = lastActive;
		return row;
	}
}

//------------------------------------------------------------------------------------------------
class LF_HideoutState : Managed
{
	string m_sHideoutId;
	string m_sOrganizationId;
	bool m_bLocked;

	// Held ground (#751), saved in the "heldground" payload (LF_HeldGroundRow), not in "hideout".
	//! The organization the hold and points belong to; "" = nobody holds the site.
	string m_sHolderOrgId;
	//! Current uninterrupted hold, whole online seconds.
	int m_iHoldSeconds;
	//! Banked points, 0 to LF_OrgConfig.HeldGroundMaxPoints().
	int m_iHoldPoints;

	//! Unix seconds a member of the leasing organization was last connected (#2573), saved in the
	//! "leaseidle" payload (LF_HideoutIdleRow), not in "hideout"; 0 = no lease.
	int m_iLastActive;

	//! Runtime only, never saved: the organization this site's door is registered for as its base
	//! (LF_Places "org_base", #2504); "" = none registered.
	string m_sBaseOrgId;

	//------------------------------------------------------------------------------------------------
	void ResetHeldGround()
	{
		m_sHolderOrgId = "";
		m_iHoldSeconds = 0;
		m_iHoldPoints = 0;
	}

	//------------------------------------------------------------------------------------------------
	bool HasDefaultHeldGround()
	{
		return m_sHolderOrgId.IsEmpty() && m_iHoldSeconds == 0 && m_iHoldPoints == 0;
	}
}

//------------------------------------------------------------------------------------------------
class LF_Hideout
{
	//! The system a server switches hideouts off with (#1075). Leases and locks already saved are kept.
	static const string SYSTEM = "Hideouts";

	//! LF_Places kind of a hideout's door (LF_HideoutDoorComponent); the id is the hideout id.
	static const string PLACE_KIND_DOOR = "HideoutDoor";

	//! LF_Places kind and id of the door of the hideout organizations lease at the desk and
	//! contraband starts from (the door's "organization hideout" attribute, #1055).
	static const string PLACE_KIND_ORG_HIDEOUT = "OrgHideout";
	static const string PLACE_ID_ORG_HIDEOUT = "primary";

	//! A door a member opened is relocked once no member stands this close to it (checked every sweep).
	static const float RELOCK_RADIUS_M = 40;	// lf-fixed: door safety radius, not a balance knob
	static const int RELOCK_SWEEP_MS = 15000;	// lf-fixed: sweep period of the relock check

	//! A lease idle longer than LF_OrgConfig.HideoutLeaseIdleDays() lapses (#2573); the check runs this often.
	static const int IDLE_SWEEP_MS = 300000;	// lf-fixed: sweep period of the idle-lease check, not a balance knob
	static const int DAY_SECONDS = 86400;	// lf-fixed: seconds in a day
	//! A saved clock already past the limit at load means the server was down, so no member could connect:
	//! the lease gets this much more time instead of lapsing before its members can join.
	static const int IDLE_LOAD_GRACE_S = 86400;	// lf-fixed: one day of grace after downtime

	protected static ref map<string, ref LF_HideoutState> s_mHideouts;
	protected static string s_sLastRefusal;
	protected static bool s_bRelockScheduled;
	protected static bool s_bIdleScheduled;
	protected static ref LF_HideoutEmptiedListener s_Emptied;

	//! Character id of an entity no player controls (a test fixture). A player always resolves
	//! through the account, never through this map.
	protected static ref map<EntityID, string> s_mBoundCharacters;

	//------------------------------------------------------------------------------------------------
	//! World end (LF_Hideout_StaticResetHook): forgets every lease, bound fixture and the relock timer
	//! so a same-process world reload starts empty. The next world's doors bind their sites again.
	static void ResetStatics()
	{
		CancelRelockSweep();
		CancelIdleSweep();
		s_sLastRefusal = "";
		s_mBoundCharacters = null;
		s_mHideouts = null;
		if (s_Emptied)
			LF_EventBus.Unsubscribe(LF_OnOrganizationEmptied, s_Emptied);

		s_Emptied = null;
	}

	//------------------------------------------------------------------------------------------------
	static void Reset()
	{
		CancelRelockSweep();
		s_sLastRefusal = "";
		s_mBoundCharacters = null;
		if (!s_mHideouts)
			return;

		foreach (string hideoutId, LF_HideoutState state : s_mHideouts)
		{
			if (!state)
				continue;

			state.m_sOrganizationId = "";
			state.m_bLocked = true;
			state.m_iLastActive = 0;
			state.ResetHeldGround();
			PushLock(state);
		}
	}

	//------------------------------------------------------------------------------------------------
	static string GetLastRefusal()
	{
		return s_sLastRefusal;
	}

	//------------------------------------------------------------------------------------------------
	//! Binds a character id to an entity no player controls, so a fixture can stand in for a member.
	static void BindCharacter(IEntity entity, string characterId)
	{
		if (!entity)
			return;

		if (!s_mBoundCharacters)
			s_mBoundCharacters = new map<EntityID, string>();

		s_mBoundCharacters.Set(entity.GetID(), characterId);
	}

	//------------------------------------------------------------------------------------------------
	static string CharacterIdOf(IEntity user)
	{
		if (!user)
			return "";

		string uid = LF_Utils.GetPlayerUid(user);
		if (uid.IsEmpty())
		{
			if (s_mBoundCharacters && s_mBoundCharacters.Contains(user.GetID()))
				return s_mBoundCharacters.Get(user.GetID());

			return "";
		}

		LF_PlayerAccountManager manager = LF_PlayerAccountManager.GetInstance();
		if (!manager)
			return "";

		LF_PlayerAccount account = manager.GetAccount(uid);
		if (!account)
			return "";

		LF_PlayerCharacter character = account.GetActiveCharacter();
		if (!character)
			return "";

		return character.GetId();
	}

	//------------------------------------------------------------------------------------------------
	//! Id of the placed organization hideout, the key its state is saved under; "" when the map
	//! places none (a lease then refuses and contraband reports no owner).
	static string OrgHideoutId()
	{
		IEntity door = LF_Places.Find(PLACE_KIND_ORG_HIDEOUT, PLACE_ID_ORG_HIDEOUT);
		if (!door)
			return "";

		LF_HideoutDoorComponent component = LF_Component<LF_HideoutDoorComponent>.Find(door);
		if (!component)
			return "";

		return component.GetHideoutIdAttribute();
	}

	//------------------------------------------------------------------------------------------------
	static void BindDoor(string hideoutId, IEntity door)
	{
		if (!Replication.IsServer())
			return;

		if (hideoutId.IsEmpty() || !door)
		{
			Refuse("door bind refused: missing hideout or entity");
			return;
		}

		LF_LockComponent lock = LF_Component<LF_LockComponent>.Find(door);
		if (!lock)
		{
			Refuse(string.Format("door of hideout %1 has no LF_LockComponent", hideoutId));
			return;
		}

		// The door is found again by component under the hideout id, never by its entity name.
		if (!LF_Places.Register(PLACE_KIND_DOOR, hideoutId, door))
		{
			Refuse(string.Format("door bind refused: hideout %1 already has a door", hideoutId));
			return;
		}

		LF_HideoutState state = Ensure(hideoutId);
		if (!state.m_sOrganizationId.IsEmpty())
		{
			PushLock(state);
			LF_Debug.Log("Hideout", string.Format("door bound hideout=%1 owner=%2", hideoutId, state.m_sOrganizationId));
			return;
		}

		state.m_bLocked = lock.IsLocked();
		LF_Debug.Log("Hideout", string.Format("door bound hideout=%1", hideoutId));
	}

	//------------------------------------------------------------------------------------------------
	static bool Assign(string hideoutId, string organizationId)
	{
		if (!Server())
			return false;

		if (LF_Systems.Refuses(SYSTEM, "hideout lease " + hideoutId))
			return false;

		if (hideoutId.IsEmpty() || organizationId.IsEmpty())
		{
			Refuse("assign refused: empty id");
			return false;
		}

		LF_Organization org = LF_OrganizationManager.Get(organizationId);
		if (!org)
		{
			Refuse("assign refused: no organization");
			return false;
		}

		string ownedHideout = HideoutOf(organizationId);
		if (!ownedHideout.IsEmpty() && ownedHideout != hideoutId)
		{
			Refuse("assign refused: organization already owns a hideout");
			return false;
		}

		LF_HideoutState state = Ensure(hideoutId);
		if (!state.m_sOrganizationId.IsEmpty() && state.m_sOrganizationId != organizationId)
		{
			Refuse("assign refused: hideout already owned");
			return false;
		}

		if (state.m_sOrganizationId != organizationId || state.m_iLastActive <= 0)
			state.m_iLastActive = System.GetUnixTime();

		state.m_sOrganizationId = organizationId;
		PushLock(state);
		LF_Debug.Log("Hideout", string.Format("assigned hideout=%1 org=%2", hideoutId, organizationId));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Ends a lease (server): the site has no owner, locks, and loses its held ground (#751: points
	//! are lost when the lease lapses).
	//! \return False when the site has no lease.
	static bool Release(string hideoutId, string reason = "released")
	{
		if (!Server())
			return false;

		LF_HideoutState state = Find(hideoutId);
		if (!state || state.m_sOrganizationId.IsEmpty())
		{
			Refuse("release refused: hideout not leased");
			return false;
		}

		string organizationId = state.m_sOrganizationId;
		state.m_sOrganizationId = "";
		state.m_bLocked = true;
		state.m_iLastActive = 0;
		state.ResetHeldGround();
		PushLock(state);
		LF_Debug.Info("Hideout", string.Format("released hideout=%1 org=%2 reason=%3", hideoutId, organizationId, reason));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: an organization with no member left (LF_OnOrganizationEmptied, #2571) holds nothing:
	//! every site it leases is released.
	//! \return How many sites were released.
	static int ReleaseAllOf(string organizationId, string reason)
	{
		if (!s_mHideouts || organizationId.IsEmpty())
			return 0;

		array<string> sites = {};
		foreach (string hideoutId, LF_HideoutState state : s_mHideouts)
		{
			if (state && state.m_sOrganizationId == organizationId)
				sites.Insert(hideoutId);
		}

		int released;
		foreach (string site : sites)
		{
			if (Release(site, reason))
				released++;
		}

		return released;
	}

	//------------------------------------------------------------------------------------------------
	//! Subscribes the empty-organization release to the bus. Idempotent: the bus refuses a second
	//! subscription of the same subscriber, and the subscriber is kept for the world's life.
	protected static void WatchEmptiedOrganizations()
	{
		if (!s_Emptied)
			s_Emptied = new LF_HideoutEmptiedListener();

		LF_EventBus.Subscribe(LF_OnOrganizationEmptied, s_Emptied);
	}

	//------------------------------------------------------------------------------------------------
	static string GetOwner(string hideoutId)
	{
		LF_HideoutState state = Find(hideoutId);
		if (!state)
			return "";

		return state.m_sOrganizationId;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsLocked(string hideoutId)
	{
		LF_HideoutState state = Find(hideoutId);
		if (!state)
			return true;

		return state.m_bLocked;
	}

	//------------------------------------------------------------------------------------------------
	static bool TryUnlock(string characterId, string hideoutId)
	{
		if (!AllowsUnlock(characterId, hideoutId, true))
			return false;

		LF_HideoutState state = Ensure(hideoutId);
		state.m_bLocked = false;
		PushLock(state);
		LF_Debug.Log("Hideout", string.Format("door unlocked hideout=%1 character=%2", hideoutId, characterId));
		ScheduleRelockSweep();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: relocks a site's door when no member of the owning organization stands within
	//! RELOCK_RADIUS_M of it, so a door a member opened does not stay open for everyone once they leave.
	//! \param membersNear Members in reach of the door; a negative number counts them in the world.
	//! \return True when the door was relocked.
	static bool RelockIfAbandoned(string hideoutId, int membersNear = -1)
	{
		if (!Server())
			return false;

		LF_HideoutState state = Find(hideoutId);
		if (!state || state.m_bLocked || state.m_sOrganizationId.IsEmpty())
			return false;

		if (membersNear < 0)
			membersNear = CountMembersNear(state);

		if (membersNear > 0)
			return false;

		state.m_bLocked = true;
		PushLock(state);
		LF_Debug.Info("Hideout", string.Format("door relocked hideout=%1: no member is near it", hideoutId));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Members of the site's owner standing within RELOCK_RADIUS_M of its door, counted on the server.
	protected static int CountMembersNear(LF_HideoutState state)
	{
		IEntity door = LF_Places.Find(PLACE_KIND_DOOR, state.m_sHideoutId);
		PlayerManager players = GetGame().GetPlayerManager();
		LF_Organization org = LF_OrganizationManager.Get(state.m_sOrganizationId);
		if (!door || !players || !org)
			return 0;

		array<int> playerIds = {};
		players.GetPlayers(playerIds);
		int near;
		foreach (int playerId : playerIds)
		{
			IEntity body = players.GetPlayerControlledEntity(playerId);
			if (!body || vector.Distance(body.GetOrigin(), door.GetOrigin()) > RELOCK_RADIUS_M)
				continue;

			string characterId = CharacterIdOf(body);
			if (!characterId.IsEmpty() && org.IsMember(characterId))
				near++;
		}

		return near;
	}

	//------------------------------------------------------------------------------------------------
	protected static void ScheduleRelockSweep()
	{
		if (s_bRelockScheduled || !GetGame() || !GetGame().GetCallqueue())
			return;

		s_bRelockScheduled = true;
		GetGame().GetCallqueue().CallLater(RelockSweep, RELOCK_SWEEP_MS, false);
	}

	//------------------------------------------------------------------------------------------------
	//! True while a relock sweep is queued (tests read it).
	static bool IsRelockScheduled()
	{
		return s_bRelockScheduled;
	}

	//------------------------------------------------------------------------------------------------
	protected static void CancelRelockSweep()
	{
		if (s_bRelockScheduled && GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(RelockSweep);

		s_bRelockScheduled = false;
	}

	//------------------------------------------------------------------------------------------------
	//! One pass over the open doors; it schedules itself again while any door stays open.
	protected static void RelockSweep()
	{
		s_bRelockScheduled = false;
		if (!s_mHideouts)
			return;

		bool stillOpen;
		foreach (string hideoutId, LF_HideoutState state : s_mHideouts)
		{
			if (!state || state.m_bLocked || state.m_sOrganizationId.IsEmpty())
				continue;

			if (!RelockIfAbandoned(hideoutId))
				stillOpen = true;
		}

		if (stillOpen)
			ScheduleRelockSweep();
	}

	//------------------------------------------------------------------------------------------------
	static bool LockDoor(string characterId, string hideoutId)
	{
		if (!AllowsUnlock(characterId, hideoutId, true))
			return false;

		LF_HideoutState state = Ensure(hideoutId);
		state.m_bLocked = true;
		PushLock(state);
		LF_Debug.Log("Hideout", string.Format("door locked hideout=%1 character=%2", hideoutId, characterId));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static bool AllowsUnlock(string characterId, string hideoutId, bool report)
	{
		if (!Server())
			return false;

		if (LF_Systems.Refuses(SYSTEM, "hideout door " + hideoutId))
			return false;

		if (characterId.IsEmpty())
		{
			if (report)
				Refuse("door refused: not a member");
			return false;
		}

		LF_HideoutState state = Find(hideoutId);
		if (!state || state.m_sOrganizationId.IsEmpty())
		{
			if (report)
				Refuse("door refused: hideout has no owner");
			return false;
		}

		LF_Organization org = LF_OrganizationManager.Get(state.m_sOrganizationId);
		if (!org || !org.IsMember(characterId))
		{
			if (report)
				Refuse("door refused: not a member");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: whether the character may use the hideout stash. Membership of the owning organization
	//! and the rank's stash right are the key: the door's lock state (breached or open) does not matter.
	static bool AllowsStash(string characterId, string hideoutId, bool report)
	{
		if (!Server())
			return false;

		if (LF_Systems.Refuses(SYSTEM, "hideout stash " + hideoutId))
			return false;

		LF_HideoutState state = Find(hideoutId);
		if (!state || state.m_sOrganizationId.IsEmpty())
		{
			if (report)
				Refuse(string.Format("stash refused: hideout %1 has no owner", hideoutId));
			return false;
		}

		LF_Organization org = LF_OrganizationManager.Get(state.m_sOrganizationId);
		if (characterId.IsEmpty() || !org || !org.IsMember(characterId))
		{
			if (report)
				Refuse("stash refused: not a member");
			return false;
		}

		// The rank's stash right (LF_EOrganizationPermission.USE_STASH): every shipped rung has it, and an
		// owner who takes it off a rank in the config shuts that rank out of the stash.
		if (!org.Allows(characterId, LF_EOrganizationPermission.USE_STASH))
		{
			if (report)
				Refuse("stash refused: the rank lacks the stash right");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	static bool CanProcess(string characterId, string hideoutId)
	{
		return EvaluateProcess(characterId, hideoutId, true);
	}

	//------------------------------------------------------------------------------------------------
	static bool CanProcessUser(IEntity user, string hideoutId)
	{
		string characterId = CharacterIdOf(user);
		if (characterId.IsEmpty())
		{
			Refuse("process refused: no character");
			return false;
		}

		return CanProcess(characterId, hideoutId);
	}

	//------------------------------------------------------------------------------------------------
	static array<ref LF_HideoutRecord> ExportAll()
	{
		array<ref LF_HideoutRecord> records = new array<ref LF_HideoutRecord>();
		if (!s_mHideouts)
			return records;

		foreach (string hideoutId, LF_HideoutState state : s_mHideouts)
		{
			if (!state || state.m_sOrganizationId.IsEmpty())
				continue;

			records.Insert(LF_HideoutRecord.Create(state.m_sHideoutId, state.m_sOrganizationId, state.m_bLocked));
		}

		return records;
	}

	//------------------------------------------------------------------------------------------------
	static void ApplyAll(notnull array<ref LF_HideoutRecord> records)
	{
		// Every site loads unheld; the "heldground" payload (#751) puts the held ground back. A save
		// from before it never reaches that payload, so this is what an old save loads with.
		if (s_mHideouts)
		{
			foreach (string heldId, LF_HideoutState held : s_mHideouts)
			{
				if (held)
					held.ResetHeldGround();
			}
		}

		set<string> loaded = new set<string>();
		bool anyOpen = false;
		foreach (LF_HideoutRecord record : records)
		{
			if (!record)
				continue;

			if (record.m_iVersion < 1 || record.m_sHideoutId.IsEmpty() || record.m_sOrganizationId.IsEmpty())
			{
				LF_Debug.Error("Hideout", "load skipped a record with no id or version");
				continue;
			}

			loaded.Insert(record.m_sHideoutId);
			LF_HideoutState state = Ensure(record.m_sHideoutId);
			state.m_sOrganizationId = record.m_sOrganizationId;
			state.m_bLocked = record.m_bLocked;
			// Every lease loads with its clock at "now"; the "leaseidle" payload (#2573) puts the saved time
			// back. A save from before it never reaches that payload, so no lease lapses on the first boot after.
			state.m_iLastActive = System.GetUnixTime();
			PushLock(state);
			if (!state.m_bLocked)
				anyOpen = true;
		}

		// A gate saved open has no member at it after a restart; only TryUnlock started the sweep, so it
		// stayed open for anyone until a member next used it. The sweep relocks it once nobody is near.
		if (anyOpen)
			ScheduleRelockSweep();

		// Idempotent: a site leased live but absent from the save is unleased by this load, as a
		// fresh load would have left it.
		if (!s_mHideouts)
			return;

		foreach (string leasedId, LF_HideoutState leased : s_mHideouts)
		{
			if (!leased || leased.m_sOrganizationId.IsEmpty() || loaded.Contains(leasedId))
				continue;

			LF_Debug.Info("Hideout", string.Format("load released hideout=%1: the save has no lease for it", leasedId));
			leased.m_sOrganizationId = "";
			leased.m_bLocked = true;
			leased.m_iLastActive = 0;
			leased.ResetHeldGround();
			PushLock(leased);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The "leaseidle" payload (#2573): the idle clock of every leased hideout.
	static array<ref LF_HideoutIdleRow> ExportIdle()
	{
		array<ref LF_HideoutIdleRow> rows = new array<ref LF_HideoutIdleRow>();
		if (!s_mHideouts)
			return rows;

		foreach (string hideoutId, LF_HideoutState state : s_mHideouts)
		{
			if (!state || state.m_sOrganizationId.IsEmpty() || state.m_iLastActive <= 0)
				continue;

			rows.Insert(LF_HideoutIdleRow.Create(state.m_sHideoutId, state.m_iLastActive));
		}

		return rows;
	}

	//------------------------------------------------------------------------------------------------
	//! Puts each saved idle clock back on its leased site (runs after "hideout" in the same load, which
	//! seeded every lease at "now"). A row whose site has no lease is dropped; a stamp in the future
	//! reads as now; a stamp already past the limit means the server was down, so the lease gets
	//! IDLE_LOAD_GRACE_S more instead of lapsing before its members can connect. Idempotent.
	//! \return The number of rows applied.
	static int ApplyIdle(array<ref LF_HideoutIdleRow> rows)
	{
		if (!rows)
			return 0;

		int now = System.GetUnixTime();
		int limit = LF_OrgConfig.HideoutLeaseIdleDays() * DAY_SECONDS;
		int applied = 0;
		foreach (LF_HideoutIdleRow row : rows)
		{
			if (!row || row.m_iVersion < 1 || row.m_sHideoutId.IsEmpty())
			{
				LF_Debug.Error("Hideout", "idle load skipped a row with no id or version");
				continue;
			}

			LF_HideoutState state = Find(LF_SaveIds.Resolve(LF_SaveIds.KIND_PLACE, row.m_sHideoutId));
			if (!state || state.m_sOrganizationId.IsEmpty())
			{
				LF_Debug.Warn("Hideout", string.Format("idle clock of %1 dropped: the site has no lease", row.m_sHideoutId));
				continue;
			}

			int lastActive = row.m_iLastActive;
			if (lastActive <= 0 || lastActive > now)
				lastActive = now;

			if (limit > 0 && now - lastActive > limit)
			{
				LF_Debug.Warn("Hideout", string.Format("idle clock of %1 is %2 days old at load (server downtime): the lease gets one more day", state.m_sHideoutId, (now - lastActive) / DAY_SECONDS));
				lastActive = now - limit + IDLE_LOAD_GRACE_S;
			}

			state.m_iLastActive = lastActive;
			applied++;
		}

		return applied;
	}

	//------------------------------------------------------------------------------------------------
	//! Unix seconds a member of the site's leasing organization was last seen connected; 0 = no lease.
	static int LastActiveOf(string hideoutId)
	{
		LF_HideoutState state = Find(hideoutId);
		if (!state || state.m_sOrganizationId.IsEmpty())
			return 0;

		return state.m_iLastActive;
	}

	//------------------------------------------------------------------------------------------------
	//! Sets a leased site's idle clock (server): the seam the sweep tests and the load use.
	//! \return False when the site has no lease.
	static bool SetLastActive(string hideoutId, int lastActive)
	{
		if (!Server())
			return false;

		LF_HideoutState state = Find(hideoutId);
		if (!state || state.m_sOrganizationId.IsEmpty())
			return false;

		state.m_iLastActive = lastActive;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one pass over the leases (#2573). A lease whose organization has a member connected restarts
	//! its clock at `now`; any other lease idle longer than LF_OrgConfig.HideoutLeaseIdleDays() is
	//! released (0 days = never). Logs `released hideout=<id> org=<org> reason=idle <n> days`.
	//! \param nowUnix Unix seconds to judge by; 0 reads the clock.
	//! \param online Character ids counted as connected; null reads the connected players.
	//! \return How many leases were released.
	static int SweepIdle(int nowUnix = 0, array<string> online = null)
	{
		if (!Replication.IsServer() || !s_mHideouts)
			return 0;

		// A server that switched hideouts off keeps its leases as saved (#1075).
		if (!LF_Systems.IsEnabled(SYSTEM))
			return 0;

		int now = nowUnix;
		if (now <= 0)
			now = System.GetUnixTime();

		array<string> connected = online;
		ref array<string> read;
		if (!connected)
		{
			read = new array<string>();
			CollectOnlineCharacters(read);
			connected = read;
		}

		int limit = LF_OrgConfig.HideoutLeaseIdleDays() * DAY_SECONDS;
		array<string> lapsed = {};
		array<int> idleDays = {};
		foreach (string hideoutId, LF_HideoutState state : s_mHideouts)
		{
			if (!state || state.m_sOrganizationId.IsEmpty())
				continue;

			if (AnyMemberOnline(state.m_sOrganizationId, connected) || state.m_iLastActive <= 0)
			{
				state.m_iLastActive = now;
				continue;
			}

			if (limit > 0 && now - state.m_iLastActive > limit)
			{
				lapsed.Insert(hideoutId);
				idleDays.Insert((now - state.m_iLastActive) / DAY_SECONDS);
			}
		}

		int released;
		foreach (int index, string lapsedId : lapsed)
		{
			if (Release(lapsedId, string.Format("idle %1 days", idleDays.Get(index))))
				released++;
		}

		return released;
	}

	//------------------------------------------------------------------------------------------------
	//! True when a member of the organization is in `online`.
	protected static bool AnyMemberOnline(string organizationId, notnull array<string> online)
	{
		LF_Organization org = LF_OrganizationManager.Get(organizationId);
		if (!org)
			return false;

		foreach (string characterId : online)
		{
			if (org.IsMember(characterId))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the active character id of every connected player.
	protected static void CollectOnlineCharacters(notnull array<string> online)
	{
		PlayerManager players = GetGame().GetPlayerManager();
		if (!players)
			return;

		array<int> playerIds = {};
		players.GetPlayers(playerIds);
		foreach (int playerId : playerIds)
		{
			string characterId = CharacterIdOf(players.GetPlayerControlledEntity(playerId));
			if (!characterId.IsEmpty())
				online.Insert(characterId);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void ScheduleIdleSweep()
	{
		if (s_bIdleScheduled || !Replication.IsServer() || !GetGame() || !GetGame().GetCallqueue())
			return;

		s_bIdleScheduled = true;
		GetGame().GetCallqueue().CallLater(IdleSweepTick, IDLE_SWEEP_MS, false);
	}

	//------------------------------------------------------------------------------------------------
	protected static void CancelIdleSweep()
	{
		if (s_bIdleScheduled && GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(IdleSweepTick);

		s_bIdleScheduled = false;
	}

	//------------------------------------------------------------------------------------------------
	//! The timed sweep: one pass, then it queues itself again for the life of the world.
	protected static void IdleSweepTick()
	{
		s_bIdleScheduled = false;
		SweepIdle();
		ScheduleIdleSweep();
	}

	//------------------------------------------------------------------------------------------------
	//! The "heldground" payload (#751): one row per hideout whose held ground is not the default.
	//! An unheld site writes no row.
	static array<ref LF_HeldGroundRow> ExportHeldGround()
	{
		array<ref LF_HeldGroundRow> rows = new array<ref LF_HeldGroundRow>();
		if (!s_mHideouts)
			return rows;

		foreach (string hideoutId, LF_HideoutState state : s_mHideouts)
		{
			if (!state || state.HasDefaultHeldGround())
				continue;

			rows.Insert(LF_HeldGroundRow.Create(state.m_sHideoutId, state.m_sHolderOrgId, state.m_iHoldSeconds, state.m_iHoldPoints));
		}

		return rows;
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces the held ground of every hideout: each starts unheld, then each row is written onto its
	//! site. Runs after "organizations" and "hideout" in the same load. A row is dropped (logged) when
	//! its site has no lease, when its organization is gone, or when that organization no longer
	//! leases the site; bad numbers are corrected; one bad row never stops the others. Idempotent.
	//! \return The number of rows applied.
	static int ApplyHeldGround(array<ref LF_HeldGroundRow> rows)
	{
		if (s_mHideouts)
		{
			foreach (string hideoutId, LF_HideoutState state : s_mHideouts)
			{
				if (state)
					state.ResetHeldGround();
			}
		}

		if (!rows)
			return 0;

		int applied = 0;
		foreach (LF_HeldGroundRow row : rows)
		{
			if (!row || row.m_iVersion < 1 || row.m_sHideoutId.IsEmpty())
			{
				LF_Debug.Error("Hideout", "held ground load skipped a row with no id or version");
				continue;
			}

			if (SetHeldGround(row.m_sHideoutId, row.m_sHolderOrgId, row.m_iHoldSeconds, row.m_iHoldPoints))
				applied++;
		}

		return applied;
	}

	//------------------------------------------------------------------------------------------------
	//! Sets one site's held ground (server). The writer is the scoring tick (#751 behaviour); bad
	//! values are corrected as on load. An empty holder clears the site.
	//! \return False (the site is left as it was) off the server, or when the holder is gone or does
	//! not lease the site.
	static bool SetHeldGround(string hideoutId, string holderOrgId, int holdSeconds, int holdPoints)
	{
		if (!Server())
			return false;

		LF_HideoutState state = Find(hideoutId);
		if (holderOrgId.IsEmpty())
		{
			if (state)
				state.ResetHeldGround();
			return true;
		}

		if (!LF_OrganizationManager.Get(holderOrgId))
		{
			LF_Debug.Warn("Hideout", string.Format("held ground at %1 dropped: organization %2 is gone", hideoutId, holderOrgId));
			return false;
		}

		if (!state || state.m_sOrganizationId != holderOrgId)
		{
			LF_Debug.Warn("Hideout", string.Format("held ground at %1 dropped: organization %2 does not lease it", hideoutId, holderOrgId));
			return false;
		}

		if (holdSeconds < 0)
		{
			LF_Debug.Error("Hideout", string.Format("held ground at %1: negative hold %2 s reads as 0", hideoutId, holdSeconds));
			holdSeconds = 0;
		}

		int maxPoints = LF_OrgConfig.HeldGroundMaxPoints();
		if (holdPoints < 0)
		{
			LF_Debug.Error("Hideout", string.Format("held ground at %1: negative points %2 read as 0", hideoutId, holdPoints));
			holdPoints = 0;
		}
		else if (holdPoints > maxPoints)
		{
			// A lowered cap is an owner choice, not damage.
			LF_Debug.Warn("Hideout", string.Format("held ground at %1: %2 points clamped to the cap %3", hideoutId, holdPoints, maxPoints));
			holdPoints = maxPoints;
		}

		state.m_sHolderOrgId = holderOrgId;
		state.m_iHoldSeconds = holdSeconds;
		state.m_iHoldPoints = holdPoints;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The site's held ground as a row (a copy), or null when the site is unknown.
	static LF_HeldGroundRow HeldGroundOf(string hideoutId)
	{
		LF_HideoutState state = Find(hideoutId);
		if (!state)
			return null;

		return LF_HeldGroundRow.Create(state.m_sHideoutId, state.m_sHolderOrgId, state.m_iHoldSeconds, state.m_iHoldPoints);
	}

	//------------------------------------------------------------------------------------------------
	protected static bool EvaluateProcess(string characterId, string hideoutId, bool report)
	{
		if (!Server())
			return false;

		if (LF_Systems.Refuses(SYSTEM, "hideout machine " + hideoutId))
			return false;

		string ownerId = GetOwner(hideoutId);
		LF_Organization org = LF_OrganizationManager.GetByCharacter(characterId);
		if (ownerId.IsEmpty() || !org || org.GetId() != ownerId)
		{
			if (report)
				Refuse("process refused: organization does not own the hideout");
			return false;
		}

		if (!org.HasProcessingSlot())
		{
			if (report)
				Refuse("process refused: no processing upgrade");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static void PushLock(LF_HideoutState state)
	{
		if (!state)
			return;

		// The door is placed by LF_HideoutDoorComponent under the hideout id; none placed yet is a no-op.
		IEntity door = LF_Places.Find(PLACE_KIND_DOOR, state.m_sHideoutId);
		if (!door)
			return;

		SyncOrgBase(state, door);

		LF_LockComponent lock = LF_Component<LF_LockComponent>.Find(door);
		if (!lock)
			return;

		// An empty id on a released site makes the door read "Unowned" (LF_HideoutDoorRules.View) and
		// matches no house (LF_HouseLockAction.GetHouse), so a former owner's id never lingers (#2571).
		lock.SetHouseIdentifier(state.m_sOrganizationId);

		LF_HideoutDoorComponent doorComponent = LF_Component<LF_HideoutDoorComponent>.Find(door);
		if (doorComponent)
		{
			string ownerName;
			LF_Organization owner = LF_OrganizationManager.Get(state.m_sOrganizationId);
			if (owner)
				ownerName = owner.GetName();

			doorComponent.SetOwnerName(ownerName);
		}

		lock.SetLocked(state.m_bLocked);
	}

	//------------------------------------------------------------------------------------------------
	//! A leased hideout is its organization's base for placement (#2504): the door is registered in
	//! LF_Places as "org_base" under the organization id, and the place goes when the lease does. Runs
	//! from PushLock, so every lease change, load and door bind keeps it right, and is idempotent.
	//! Server only. An organization that leases a second site takes its base over (last lease wins).
	protected static void SyncOrgBase(notnull LF_HideoutState state, notnull IEntity door)
	{
		if (!Replication.IsServer())
			return;

		string baseKind = LF_PlacementZones.ORG_BASE_KIND;
		if (!state.m_sBaseOrgId.IsEmpty() && state.m_sBaseOrgId != state.m_sOrganizationId)
		{
			if (LF_Places.Find(baseKind, state.m_sBaseOrgId) == door)
				LF_Places.Unregister(baseKind, state.m_sBaseOrgId);

			state.m_sBaseOrgId = "";
		}

		if (state.m_sOrganizationId.IsEmpty() || state.m_sBaseOrgId == state.m_sOrganizationId)
			return;

		IEntity held = LF_Places.Find(baseKind, state.m_sOrganizationId);
		if (held && held != door)
		{
			LF_Debug.Info("Hideout", string.Format("base of org=%1 moves to hideout=%2: the last lease wins", state.m_sOrganizationId, state.m_sHideoutId));
			LF_Places.Unregister(baseKind, state.m_sOrganizationId);
		}

		if (LF_Places.Register(baseKind, state.m_sOrganizationId, door))
			state.m_sBaseOrgId = state.m_sOrganizationId;
	}

	//------------------------------------------------------------------------------------------------
	protected static string HideoutOf(string organizationId)
	{
		if (!s_mHideouts || organizationId.IsEmpty())
			return "";

		foreach (string hideoutId, LF_HideoutState state : s_mHideouts)
		{
			if (!state)
				continue;

			if (state.m_sOrganizationId == organizationId)
				return state.m_sHideoutId;
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_HideoutState Ensure(string hideoutId)
	{
		if (!s_mHideouts)
			s_mHideouts = new map<string, ref LF_HideoutState>();

		WatchEmptiedOrganizations();
		ScheduleIdleSweep();

		LF_HideoutState state = s_mHideouts.Get(hideoutId);
		if (!state)
		{
			state = new LF_HideoutState();
			state.m_sHideoutId = hideoutId;
			state.m_bLocked = true;
			s_mHideouts.Set(hideoutId, state);
		}

		return state;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_HideoutState Find(string hideoutId)
	{
		if (!s_mHideouts || hideoutId.IsEmpty())
			return null;

		return s_mHideouts.Get(hideoutId);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Refuse(string message)
	{
		bool repeat = s_sLastRefusal == message;
		s_sLastRefusal = message;
		if (repeat)
			return;

		LF_Debug.Error("Hideout", message);
	}

	//------------------------------------------------------------------------------------------------
	protected static bool Server()
	{
		if (Replication.IsServer())
			return true;

		Refuse("rejected: not the server");
		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! Bus subscriber (#2571): an organization that lost its last member loses its hideout lease.
class LF_HideoutEmptiedListener : LF_EventSubscriber
{
	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnOrganizationEmptied emptied = LF_OnOrganizationEmptied.Cast(busEvent);
		if (!emptied)
			return false;

		LF_Hideout.ReleaseAllOf(emptied.m_sOrgId, "organization emptied: " + emptied.m_sReason);
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_Hideout's statics (registered at script load; Core/LF_StaticReset.c).
class LF_Hideout_StaticResetHook : LF_StaticResetHook
{
	protected static bool s_bRegistered = LF_StaticReset.Register(new LF_Hideout_StaticResetHook());

	override string Owner()
	{
		return "LF_Hideout";
	}

	override void Apply()
	{
		LF_Hideout.ResetStatics();
	}
}
