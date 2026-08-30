// LF_QuestHooks.c - additive seams that route existing gameplay signals into the
// quest system. Gather/process flow through EL_JobManager already; a modded
// override forwards them without touching the job logic.

modded class EL_JobManager
{
	//------------------------------------------------------------------------------------------------
	override void OnGatherCompleted(IEntity user, ResourceName gatheredItem)
	{
		super.OnGatherCompleted(user, gatheredItem);
		LF_QuestManager questManager = LF_QuestManager.GetInstance();
		if (questManager)
			questManager.ReportGather(user, gatheredItem);
	}

	//------------------------------------------------------------------------------------------------
	override void OnProcessCompleted(IEntity user, ResourceName processedItem)
	{
		super.OnProcessCompleted(user, processedItem);
		LF_QuestManager questManager = LF_QuestManager.GetInstance();
		if (questManager)
			questManager.ReportProcess(user, processedItem);
	}
}