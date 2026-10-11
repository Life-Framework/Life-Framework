//------------------------------------------------------------------------------------------------
//! One building owned by an organization. Members unlock its door. The processing
//! machine runs only when that organization owns the hideout and has bought the
//! processing slot. A refusal is logged and the door or machine stays unused.
//!
//! The door identifier is the organization id, written onto the bound
//! EL_LockComponent. Export / Apply replace that owner and the lock flag.
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
	string m_sDoorName;
	bool m_bLocked;
}

//------------------------------------------------------------------------------------------------
class LF_Hideout
{
	static const string HIDEOUT_ID = "Roadmap_Hideout";

	protected static ref map<string, ref LF_HideoutState> s_mHideouts;
	protected static string s_sLastRefusal;

	//------------------------------------------------------------------------------------------------
	static void Reset()
	{
		s_sLastRefusal = "";
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
	static string CharacterIdOf(IEntity user)
	{
		if (!user)
			return "";

		string uid = EL_Utils.GetPlayerUid(user);
		if (uid.IsEmpty())
			return "";

		EL_PlayerAccountManager manager = EL_PlayerAccountManager.GetInstance();
		if (!manager)
			return "";

		EL_PlayerAccount account = manager.GetAccount(uid);
		if (!account)
			return "";

		EL_PlayerCharacter character = account.GetActiveCharacter();
		if (!character)
			return "";

		return character.GetId();
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

		string doorName = door.GetName();
		if (doorName.IsEmpty())
		{
			Refuse("door bind refused: entity has no name");
			return;
		}

		EL_LockComponent lock = EL_Component<EL_LockComponent>.Find(door);
		if (!lock)
		{
			Refuse(string.Format("door %1 has no EL_LockComponent", doorName));
			return;
		}

		LF_HideoutState state = Ensure(hideoutId);
		state.m_sDoorName = doorName;
		if (!state.m_sOrganizationId.IsEmpty())
		{
			PushLock(state);
			EL_Debug.Log("Hideout", string.Format("door bound %1 hideout=%2 owner=%3", doorName, hideoutId, state.m_sOrganizationId));
			return;
		}

		state.m_bLocked = lock.IsLocked();
		EL_Debug.Log("Hideout", string.Format("door bound %1 hideout=%2", doorName, hideoutId));
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

		EL_Organization org = EL_OrganizationManager.Get(organizationId);
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
		EL_Debug.Log("Hideout", string.Format("assigned hideout=%1 org=%2", hideoutId, organizationId));
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
		EL_Debug.Log("Hideout", string.Format("door unlocked hideout=%1 character=%2", hideoutId, characterId));
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
		EL_Debug.Log("Hideout", string.Format("door locked hideout=%1 character=%2", hideoutId, characterId));
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

		EL_Organization org = EL_OrganizationManager.Get(state.m_sOrganizationId);
		if (!org || !org.IsMember(characterId))
		{
			if (report)
				Refuse("door refused: not a member");
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
				EL_Debug.Error("Hideout", "load skipped a record with no id or version");
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
		EL_Organization org = EL_OrganizationManager.GetByCharacter(characterId);
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
		if (!state || state.m_sDoorName.IsEmpty())
			return;

		if (!GetGame() || !GetGame().GetWorld())
			return;

		IEntity door = GetGame().GetWorld().FindEntityByName(state.m_sDoorName);
		if (!door)
			return;

		EL_LockComponent lock = EL_Component<EL_LockComponent>.Find(door);
		if (!lock)
			return;

		if (!state.m_sOrganizationId.IsEmpty())
			lock.SetHouseIdentifier(state.m_sOrganizationId);

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

		EL_Debug.Error("Hideout", message);
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
