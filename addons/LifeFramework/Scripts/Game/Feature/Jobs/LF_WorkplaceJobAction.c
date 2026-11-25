//------------------------------------------------------------------------------------------------
//! "Take the job: Miner" / "Quit the job" at a workplace desk whose job is a gathering job (MINER,
//! LUMBERJACK, FARMER): the desk at Coalman's Brow mine, the woodlot, the orchard. Before this no action
//! anywhere put such a job on a player, so the pickaxe and axe tills, which need the job, could never
//! sell (2026-11-22). Police and medic desks keep the punch clock (LF_DutyAction); delivery jobs keep
//! their counters (LF_JobSignUpAction). Server decides: the same standing gate as the counter and the
//! job dialogue (LF_JobStandingGate), then LF_PlayerJobComponent.SetJob with no free job kit (the tools are bought at the tills), every outcome logged.
//! The job lives on the body (LF_PlayerJobComponent); LF_WorkplaceJobs keeps which one the player took
//! (the "workplacejobs" save section, #4959), so a respawn and a restart give it back.
class LF_WorkplaceJob
{
	static const string FEATURE = "Jobs";

	//------------------------------------------------------------------------------------------------
	//! The job a desk of this job id signs a player up for: a gathering job, else UNEMPLOYED (a duty
	//! desk, a delivery counter, an id no job has).
	static LF_EJobType DeskJob(string job)
	{
		if (job.IsEmpty())
			return LF_EJobType.UNEMPLOYED;

		LF_EJobType type = typename.StringToEnum(LF_EJobType, job);
		if (typename.EnumToString(LF_EJobType, type) != job)
			return LF_EJobType.UNEMPLOYED;

		if (type == LF_EJobType.MINER || type == LF_EJobType.LUMBERJACK || type == LF_EJobType.FARMER)
			return type;

		return LF_EJobType.UNEMPLOYED;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: takes the desk's job, or quits it when the player already holds it.
	//! \return The job the body holds afterwards, or -1 when refused (logged).
	static int Toggle(IEntity player, LF_EJobType deskJob)
	{
		if (!Replication.IsServer() || !player || deskJob == LF_EJobType.UNEMPLOYED)
			return -1;

		if (LF_Systems.Refuses(LF_CivilianJob.SYSTEM, "workplace job"))
			return -1;

		string uid = LF_Utils.GetPlayerUID(player);
		if (uid.IsEmpty())
		{
			LF_Debug.Error(FEATURE, "workplace job refused: the player has no account id");
			return -1;
		}

		if (!LF_Utils.IsAliveState(player))
		{
			LF_Debug.Info(FEATURE, "workplace job refused for " + uid + ": not alive");
			return -1;
		}

		LF_PlayerJobComponent jobs = LF_Component<LF_PlayerJobComponent>.Find(player);
		if (!jobs)
		{
			LF_Debug.Error(FEATURE, "workplace job refused for " + uid + ": no LF_PlayerJobComponent");
			return -1;
		}

		string jobName = typename.EnumToString(LF_EJobType, deskJob);
		if (jobs.GetJob() == deskJob)
		{
			if (!jobs.SetJob(LF_EJobType.UNEMPLOYED))
			{
				LF_Debug.Warn(FEATURE, string.Format("%1 could not quit %2: the job component refused", uid, jobName));
				return -1;
			}

			LF_WorkplaceJobs.Forget(uid);
			LF_Debug.Info(FEATURE, string.Format("%1 quit %2 at the desk", uid, jobName));
			return LF_EJobType.UNEMPLOYED;
		}

		string tierWord;
		string standing = LF_JobStandingGate.Refusal(uid, "job", deskJob, tierWord);
		if (!standing.IsEmpty())
		{
			LF_Debug.Info(FEATURE, string.Format("workplace job refused: %1 lacks the standing for %2", uid, LF_JobStandingGate.GateId("job", deskJob)));
			LF_Utils.NotifyEntity(player, standing, "#LF-Job_DeskTakeName", 6.0, tierWord);
			return -1;
		}

		if (!jobs.SetJob(deskJob, false))
		{
			LF_Debug.Warn(FEATURE, string.Format("%1 could not take %2: the job component refused", uid, jobName));
			return -1;
		}

		LF_WorkplaceJobs.Note(uid, deskJob);
		LF_Debug.Info(FEATURE, string.Format("%1 took %2 at the desk", uid, jobName));
		return deskJob;
	}
}

//------------------------------------------------------------------------------------------------
//! The desk's action, on the workplace prefab's clock beside the desk (LF_Workplace.FindFor).
class LF_WorkplaceJobAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	protected LF_EJobType DeskJob()
	{
		LF_WorkplaceComponent workplace = LF_Workplace.FindFor(GetOwner());
		if (!workplace)
			return LF_EJobType.UNEMPLOYED;

		return LF_WorkplaceJob.DeskJob(workplace.GetJob());
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return DeskJob() != LF_EJobType.UNEMPLOYED && LF_Systems.IsEnabled(LF_CivilianJob.SYSTEM);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		LF_EJobType job = DeskJob();
		string jobName = LF_PlayerMenuSheet.SkillName(typename.EnumToString(LF_EJobType, job));
		IEntity user = SCR_PlayerController.GetLocalControlledEntity();
		LF_PlayerJobComponent jobs = LF_Component<LF_PlayerJobComponent>.Find(user);
		if (jobs && jobs.GetJob() == job)
			outName = LF_Terms.Text("#LF-Job_DeskQuit", jobName);
		else
			outName = LF_Terms.Text("#LF-Job_DeskTake", jobName);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		LF_EJobType job = DeskJob();
		if (job == LF_EJobType.UNEMPLOYED)
		{
			LF_Debug.Warn(LF_WorkplaceJob.FEATURE, "workplace job refused: this desk has no gathering job");
			return;
		}

		LF_WorkplaceJob.Toggle(pUserEntity, job);
	}
}
