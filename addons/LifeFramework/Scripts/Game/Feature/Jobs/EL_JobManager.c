class EL_JobManager : Managed
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
		if (amount <= 0 || !user)
			return;

		// Cash is the only payout; the bank balance moves only through the ATM.
		int paid = EL_MoneyUtils.GiveCash(user, amount);
		if (paid != amount)
			EL_Debug.Warn("Jobs", string.Format("job reward payout short: paid %1 of %2 (player %3)", paid, amount, user));

		// Show notification
		EL_Utils.Notify(WidgetManager.Translate("#EL-Job_Earned", paid), "#EL-Job_Reward", 3.0);
	}
};