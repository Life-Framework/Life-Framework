class EL_JobManager : ScriptedUserAction
{
	protected static ref EL_JobManager s_Instance;

	//------------------------------------------------------------------------------------------------
	static EL_JobManager GetInstance()
	{
		if (!s_Instance)
			s_Instance = new EL_JobManager();

		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	void EL_JobManager()
	{
		s_Instance = this;
	}

	//------------------------------------------------------------------------------------------------
	void ~EL_JobManager()
	{
		s_Instance = null;
	}

	//------------------------------------------------------------------------------------------------
	void OnGatherCompleted(IEntity user, ResourceName gatheredItem)
	{
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
	protected int GetGatherReward(ResourceName item)
	{
		// No direct rewards for gathering - sell to traders instead
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	protected int GetProcessReward(ResourceName item)
	{
		// No direct rewards for processing - sell to traders instead
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	protected void GiveReward(IEntity user, int amount)
	{
		// Add to bank account
		EL_ATMManager atmManager = EL_ATMManager.GetInstance();
		string playerUid = EL_Utils.GetPlayerUid(user);
		atmManager.Deposit(playerUid, amount);

		// Show notification
		EL_Utils.Notify(string.Format("#EL-Job_Earned", amount), "#EL-Job_Reward", 3.0);
	}
};