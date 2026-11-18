//------------------------------------------------------------------------------------------------
//! What a conversation can ask of the jobs (Feature/Dialogue names these hooks in a dialogue file;
//! the class name is the registration). Server only.
//------------------------------------------------------------------------------------------------
//! Action: the reply signs the player up for the delivery job its param names ("PARCEL"), exactly
//! as the counter's LF_JobSignUpAction does (#2740): the job must be a known delivery job, the
//! player's career level (LF_CivilianJob.CanSignUp) and standing (#2700/#2701, the `job:<JOB>` gate)
//! must allow it, then LF_PlayerJobComponent.SetJob puts the job on the body. The talk range
//! (LF_Dialogue.REACH) is the proximity check, as the action range is for the counter.
class LF_DialogueHook_JobSignUp : LF_DialogueHook
{
	//------------------------------------------------------------------------------------------------
	//! \return The job a reply's param names, or UNEMPLOYED when the param is no known delivery job.
	static LF_EJobType JobOf(string param)
	{
		if (param.IsEmpty())
			return LF_EJobType.UNEMPLOYED;

		LF_EJobType job = typename.StringToEnum(LF_EJobType, param);
		if (typename.EnumToString(LF_EJobType, job) != param || !LF_CivilianJob.IsDeliveryJob(job))
			return LF_EJobType.UNEMPLOYED;

		return job;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(IEntity player, IEntity npc, string param, notnull array<string> spoken)
	{
		LF_EJobType job = JobOf(param);
		if (job == LF_EJobType.UNEMPLOYED)
		{
			LF_Debug.Error("Jobs", string.Format("dialogue sign-up refused: '%1' is not a known delivery job", param));
			return;
		}

		if (LF_Systems.Refuses(LF_CivilianJob.SYSTEM, "dialogue job sign up"))
			return;

		if (!player)
		{
			LF_Debug.Error("Jobs", "dialogue sign-up refused: no player");
			return;
		}

		string uid = LF_Utils.GetPlayerUid(player);
		if (uid.IsEmpty())
		{
			LF_Debug.Error("Jobs", "dialogue sign-up refused: player has no account id");
			return;
		}

		string tierWord;
		string standing = LF_JobStandingGate.Refusal(uid, "job", job, tierWord);
		if (!standing.IsEmpty())
		{
			LF_Debug.Info("Jobs", string.Format("dialogue sign-up refused: %1 lacks the standing for %2", uid, LF_JobStandingGate.GateId("job", job)));
			LF_Utils.NotifyEntity(player, standing, LF_CivilianJob.NameKey(job), 6.0, tierWord);
			return;
		}

		if (!LF_CivilianJob.SignUp(uid, job))
		{
			LF_Debug.Info("Jobs", string.Format("dialogue sign-up refused: %1 is below the level for %2", uid, param));
			return;
		}

		LF_PlayerJobComponent jobs = LF_Component<LF_PlayerJobComponent>.Find(player);
		if (!jobs)
		{
			LF_Debug.Error("Jobs", "dialogue sign-up: player has no LF_PlayerJobComponent");
			return;
		}

		if (!jobs.SetJob(job))
		{
			LF_Debug.Error("Jobs", "dialogue sign-up: job component refused the job");
			return;
		}

		LF_Debug.Info("Jobs", string.Format("dialogue sign-up: %1 -> %2", uid, param));
		LF_Utils.NotifyEntity(player, LF_CivilianJob.SignedUpKey(job), LF_CivilianJob.NameKey(job), 5.0);
	}
}
