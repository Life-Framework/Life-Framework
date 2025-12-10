class EL_GroupManager : ScriptedUserAction
{
	protected static EL_GroupManager s_Instance;
	protected ref map<string, EL_GroupComponent> m_mGroups = new map<string, EL_GroupComponent>();

	//------------------------------------------------------------------------------------------------
	static EL_GroupManager GetInstance()
	{
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	void EL_GroupManager()
	{
		s_Instance = this;
	}

	//------------------------------------------------------------------------------------------------
	void ~EL_GroupManager()
	{
		s_Instance = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Register a group component
	void RegisterGroup(EL_GroupComponent group)
	{
		if (group)
		{
			m_mGroups.Set(group.GetGroupName(), group);
			Print("Group registered: " + group.GetGroupName());
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Unregister a group component
	void UnregisterGroup(string groupName)
	{
		m_mGroups.Remove(groupName);
		Print("Group unregistered: " + groupName);
	}

	//------------------------------------------------------------------------------------------------
	//! Get group by name
	EL_GroupComponent GetGroup(string groupName)
	{
		return m_mGroups.Get(groupName);
	}

	//------------------------------------------------------------------------------------------------
	//! Get group by player UID
	EL_GroupComponent GetGroupByPlayer(string playerUID)
	{
		for (int i = 0; i < m_mGroups.Count(); i++)
		{
			EL_GroupComponent group = m_mGroups.GetElement(i);
			if (group && group.IsMember(playerUID))
			{
				return group;
			}
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Create new group
	EL_GroupComponent CreateGroup(string groupName, string leaderUID)
	{
		if (m_mGroups.Contains(groupName))
			return null;

		// Create new group component (assuming it's attached to an entity)
		// For now, return null - actual creation should be done in gamemode or prefab
		Print("Group creation requested: " + groupName + " by " + leaderUID);
		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Check if group name is available
	bool IsGroupNameAvailable(string groupName)
	{
		return !m_mGroups.Contains(groupName);
	}

	//------------------------------------------------------------------------------------------------
	//! Send invitation (basic implementation, no networking yet)
	bool SendInvitation(string groupName, string inviterUID, string inviteeUID)
	{
		EL_GroupComponent group = GetGroup(groupName);
		if (!group || !group.IsMember(inviterUID))
			return false;

		// TODO: Implement networking to send to invitee
		Print("Invitation sent from " + inviterUID + " to " + inviteeUID + " for group " + groupName);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Accept invitation
	bool AcceptInvitation(string groupName, string playerUID)
	{
		EL_GroupComponent group = GetGroup(groupName);
		if (!group)
			return false;

		return group.AddMember(playerUID);
	}

	//------------------------------------------------------------------------------------------------
	//! Get all groups
	array<EL_GroupComponent> GetAllGroups()
	{
		array<EL_GroupComponent> groups = new array<EL_GroupComponent>();
		for (int i = 0; i < m_mGroups.Count(); i++)
		{
			groups.Insert(m_mGroups.GetElement(i));
		}
		return groups;
	}

	//------------------------------------------------------------------------------------------------
	//! Get group count
	int GetGroupCount()
	{
		return m_mGroups.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Save all groups (EPF handles persistence automatically via components)
	void SaveAllGroups()
	{
		// EPF_PersistentScriptedState handles saving automatically
		Print("Groups persistence handled by EPF");
	}
};