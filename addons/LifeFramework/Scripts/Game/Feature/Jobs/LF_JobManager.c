class LF_JobManager : Managed
{
	protected static ref LF_JobManager s_Instance;
	protected ref map<string, int> m_mScopeGatherRewards;
	protected ref map<string, int> m_mScopeProcessRewards;

	//------------------------------------------------------------------------------------------------
	static LF_JobManager GetInstance()
	{
		if (!s_Instance)
			s_Instance = new LF_JobManager();

		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	//! Does not register itself; only GetInstance() sets the singleton.
	void LF_JobManager()
	{
		m_mScopeGatherRewards = new map<string, int>();
		m_mScopeProcessRewards = new map<string, int>();
	}

	//------------------------------------------------------------------------------------------------
	void ~LF_JobManager()
	{
		if (s_Instance == this)
			s_Instance = null;
	}

	//------------------------------------------------------------------------------------------------
	void OnGatherCompleted(IEntity user, ResourceName gatheredItem)
	{
		// Job XP for the gather: the jobs config's gather award (#749).
		LF_JobLevels.Award(LF_Utils.GetPlayerUID(user), LF_JobLevels.ACTION_GATHER);

		// Give reward based on item
		int reward = GetGatherReward(gatheredItem);
		if (reward > 0)
		{
			GiveReward(user, reward);
		}
	}

	//------------------------------------------------------------------------------------------------
	void OnProcessCompleted(IEntity user, ResourceName processedItem)
	{
		// Give reward based on processing
		int reward = GetProcessReward(processedItem);
		if (reward > 0)
		{
			GiveReward(user, reward);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! A job scope may pay for one prefab. Until it does, gather and process
	//! mint nothing and the player sells the product to a trader.
	void SetScopeReward(ResourceName item, int gatherReward, int processReward)
	{
		if (!item || item.IsEmpty())
			return;

		if (!m_mScopeGatherRewards)
			m_mScopeGatherRewards = new map<string, int>();
		if (!m_mScopeProcessRewards)
			m_mScopeProcessRewards = new map<string, int>();

		string key = LF_Utils.PrefabGuid(item);
		if (gatherReward > 0)
			m_mScopeGatherRewards.Set(key, gatherReward);
		else if (m_mScopeGatherRewards.Contains(key))
			m_mScopeGatherRewards.Remove(key);

		if (processReward > 0)
			m_mScopeProcessRewards.Set(key, processReward);
		else if (m_mScopeProcessRewards.Contains(key))
			m_mScopeProcessRewards.Remove(key);
	}

	//------------------------------------------------------------------------------------------------
	void ClearScopeRewards()
	{
		m_mScopeGatherRewards = new map<string, int>();
		m_mScopeProcessRewards = new map<string, int>();
	}

	//------------------------------------------------------------------------------------------------
	int GetGatherReward(ResourceName item)
	{
		return ScopeReward(m_mScopeGatherRewards, item);
	}

	//------------------------------------------------------------------------------------------------
	int GetProcessReward(ResourceName item)
	{
		return ScopeReward(m_mScopeProcessRewards, item);
	}

	//------------------------------------------------------------------------------------------------
	protected int ScopeReward(map<string, int> rewards, ResourceName item)
	{
		if (!rewards || !item || item.IsEmpty())
			return 0;

		string key = LF_Utils.PrefabGuid(item);
		int reward;
		if (!rewards.Find(key, reward))
			return 0;

		if (reward < 0)
			return 0;

		return reward;
	}

	//------------------------------------------------------------------------------------------------
	protected void GiveReward(IEntity user, int amount)
	{
		if (amount <= 0 || !user)
			return;

		if (LF_Systems.Refuses(LF_CivilianJob.SYSTEM, "job reward"))
			return;

		// Cash is the only payout; the bank balance moves only through the ATM.
		int paid = LF_MoneyUtils.GiveCash(user, amount, "job-pay");
		if (paid != amount)
			LF_Debug.Warn("Jobs", string.Format("job reward payout short: paid %1 of %2 (player %3)", paid, amount, user));

		// Show notification
		LF_Utils.NotifyEntity(user, "#EL-Job_Earned", "#EL-Job_Reward", 3.0, paid.ToString());
	}
};