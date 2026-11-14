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
class LF_HideoutState : Managed
{
	string m_sHideoutId;
	string m_sOrganizationId;
	bool m_bLocked;
}

//------------------------------------------------------------------------------------------------
class LF_Hideout
{
	//! LF_Places kind of a hideout's door (LF_HideoutDoorComponent); the id is the hideout id.
	static const string PLACE_KIND_DOOR = "HideoutDoor";

	//! LF_Places kind and id of the door of the hideout organizations lease at the desk and
	//! contraband starts from (the door's "organization hideout" attribute, #1055).
	static const string PLACE_KIND_ORG_HIDEOUT = "OrgHideout";
	static const string PLACE_ID_ORG_HIDEOUT = "primary";

	protected static ref map<string, ref LF_HideoutState> s_mHideouts;
	protected static string s_sLastRefusal;

	//! Character id of an entity no player controls (a test fixture). A player always resolves
	//! through the account, never through this map.
	protected static ref map<EntityID, string> s_mBoundCharacters;

	//------------------------------------------------------------------------------------------------
	static void Reset()
	{
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

		state.m_sOrganizationId = organizationId;
		PushLock(state);
		LF_Debug.Log("Hideout", string.Format("assigned hideout=%1 org=%2", hideoutId, organizationId));
		return true;
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
		return true;
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
	//! Server: whether the character may use the hideout stash. Membership of the owning
	//! organization is the only key: the door's lock state (breached or open) does not matter.
	static bool AllowsStash(string characterId, string hideoutId, bool report)
	{
		if (!Server())
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
		foreach (LF_HideoutRecord record : records)
		{
			if (!record)
				continue;

			if (record.m_iVersion < 1 || record.m_sHideoutId.IsEmpty() || record.m_sOrganizationId.IsEmpty())
			{
				LF_Debug.Error("Hideout", "load skipped a record with no id or version");
				continue;
			}

			LF_HideoutState state = Ensure(record.m_sHideoutId);
			state.m_sOrganizationId = record.m_sOrganizationId;
			state.m_bLocked = record.m_bLocked;
			PushLock(state);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static bool EvaluateProcess(string characterId, string hideoutId, bool report)
	{
		if (!Server())
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

		LF_LockComponent lock = LF_Component<LF_LockComponent>.Find(door);
		if (!lock)
			return;

		if (!state.m_sOrganizationId.IsEmpty())
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
