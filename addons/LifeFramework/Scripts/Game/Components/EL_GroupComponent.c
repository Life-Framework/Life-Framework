class EL_GroupComponentClass : ScriptComponentClass
{
};

class EL_GroupComponent : EPF_PersistentScriptedState
{
    [Attribute("", UIWidgets.ResourceNamePicker, "Group data resource", "et")]
    protected ResourceName m_GroupDataResource;

    protected ref array<string> m_Members = new array<string>();
    protected string m_GroupName;
    protected string m_LeaderUID;
    protected int m_MaxMembers = 8;
    protected int m_Bank = 0;

    // EPF persistence is handled by inheriting EPF_PersistentScriptedState
    // Load persisted data (EPF will populate values on load)
    void OnPostInit(IEntity owner)
    {
        // Note: EPF_PersistentScriptedState does not define OnPostInit in base,
        // so do not call super. Just perform local initialization.
        LoadGroupData();
    }

    // Initialize group
    void InitGroup(string name, string leaderUID)
    {
        m_GroupName = name;
        m_LeaderUID = leaderUID;
        m_Members.Insert(leaderUID);
        SaveGroupData();
    }

    // Load group data
    void LoadGroupData()
    {
        // EPF persistence for this class is handled through a save data mapping
        // (EPF_PersistentScriptedStateSettings / EPF_ScriptedStateSaveData).
        // At runtime the EPF system will apply saved values to the instance fields.
        // Ensure arrays/fields have sensible defaults when not yet populated.
        if (!m_Members)
            m_Members = new array<string>();
        if (!m_GroupName)
            m_GroupName = "";
        if (!m_LeaderUID)
            m_LeaderUID = "";
    }

    // Save group data
    void SaveGroupData()
    {
        // EPF persistence is handled by a paired save-data class/setting.
        // This method is kept as a semantic helper; actual persistence
        // occurs via EPF_SaveData mappings. No runtime API calls here.
    }

    // Add member
    bool AddMember(string uid)
    {
        if (m_Members.Count() >= m_MaxMembers)
            return false;

        if (m_Members.Contains(uid))
            return false;

        m_Members.Insert(uid);
        SaveGroupData();
        return true;
    }

    // Remove member
    bool RemoveMember(string uid)
    {
        if (uid == m_LeaderUID)
            return false; // Can't remove leader

        m_Members.RemoveItem(uid);
        SaveGroupData();
        return true;
    }

    // Set new leader
    void SetLeader(string uid)
    {
        if (m_Members.Contains(uid))
        {
            m_LeaderUID = uid;
            SaveGroupData();
        }
    }

    // Getters
    string GetGroupName() { return m_GroupName; }
    string GetLeaderUID() { return m_LeaderUID; }
    array<string> GetMembers() { return m_Members; }
    int GetMaxMembers() { return m_MaxMembers; }
    int GetBank() { return m_Bank; }

    // Check if player is member
    bool IsMember(string uid)
    {
        return m_Members.Contains(uid);
    }

    // Deposit money to group bank
    bool DepositMoney(int amount, string playerUID)
    {
        if (!IsMember(playerUID))
            return false;

        // TODO: Deduct from player money
        // For now, assume success
        m_Bank += amount;
        SaveGroupData();
        return true;
    }

    // Withdraw money from group bank
    bool WithdrawMoney(int amount, string playerUID)
    {
        if (!IsMember(playerUID))
            return false;

        if (m_Bank < amount)
            return false;

        // TODO: Add to player money
        m_Bank -= amount;
        SaveGroupData();
        return true;
    }

};