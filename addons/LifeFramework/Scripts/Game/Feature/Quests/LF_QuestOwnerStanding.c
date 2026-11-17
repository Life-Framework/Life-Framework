// LF_QuestOwnerStanding.c - the live character behind LF_QuestStanding (read by accept and by the conversation offer).

//------------------------------------------------------------------------------------------------
//! The live character's level, job and faction: the one reader behind accept and the conversation offer.
class LF_QuestOwnerStanding : LF_QuestStanding
{
	protected IEntity m_Owner;
	protected LF_QuestManager m_Manager;

	//------------------------------------------------------------------------------------------------
	void LF_QuestOwnerStanding(IEntity owner, LF_QuestManager manager)
	{
		m_Owner = owner;
		m_Manager = manager;
	}

	//------------------------------------------------------------------------------------------------
	override int LevelFor(LF_QuestDefinition def)
	{
		return LF_JobLevels.LevelOfBody(m_Owner, LF_QuestTrackerComponent.JobOf(def));
	}

	//------------------------------------------------------------------------------------------------
	override LF_EJobType JobType()
	{
		LF_PlayerJobComponent jobComp = LF_Component<LF_PlayerJobComponent>.Find(m_Owner);
		if (jobComp)
			return jobComp.GetJob();

		return LF_EJobType.UNEMPLOYED;
	}

	//------------------------------------------------------------------------------------------------
	override string FactionKey()
	{
		return m_Manager.GetCharacterFaction(m_Owner);
	}
}
