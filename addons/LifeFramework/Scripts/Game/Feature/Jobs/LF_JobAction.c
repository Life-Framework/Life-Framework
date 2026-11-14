//------------------------------------------------------------------------------------------------
//! Workplace actions. Offer checks do not pay and do not grant XP.
//------------------------------------------------------------------------------------------------
class LF_JobSignUpAction : ScriptedUserAction
{
	[Attribute("5", UIWidgets.ComboBox, "Job", "", ParamEnumArray.FromEnum(LF_EJobType))]
	protected LF_EJobType m_eJob;

	//------------------------------------------------------------------------------------------------
	LF_EJobType GetJob()
	{
		return m_eJob;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return LF_Systems.IsEnabled(LF_CivilianJob.SYSTEM) && LF_CivilianJob.IsDeliveryJob(m_eJob);
	}

	//------------------------------------------------------------------------------------------------
	//! The career lives on the server. A client reads the level replicated on the player's
	//! LF_PlayerJobComponent, or fuel and taxi stay greyed on a dedicated client forever.
	protected bool OfferOpen(IEntity user, string uid)
	{
		if (Replication.IsServer())
			return LF_CivilianJob.CanSignUp(uid, m_eJob);

		int level = 1;
		LF_PlayerJobComponent jobs = LF_Component<LF_PlayerJobComponent>.Find(user);
		if (jobs)
			level = Math.Max(1, jobs.GetJobLevel());

		return LF_CivilianJob.IsDeliveryJob(m_eJob) && level >= LF_CivilianJob.RequiredLevel(m_eJob);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		string uid = LF_Utils.GetPlayerUid(user);
		if (uid.IsEmpty())
		{
			SetCannotPerformReason("#EL-Job_NotSignedUp");
			return false;
		}

		if (!OfferOpen(user, uid))
		{
			SetCannotPerformReason("#LF-Job_MoreRunsFirst");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		if (LF_Systems.Refuses(LF_CivilianJob.SYSTEM, "job sign up"))
			return;

		string uid = LF_Utils.GetPlayerUid(pUserEntity);
		if (uid.IsEmpty())
		{
			LF_Debug.Error("Jobs", "signup action refused: player has no account id");
			return;
		}

		if (!LF_CivilianJob.SignUp(uid, m_eJob))
			return;

		LF_PlayerJobComponent jobs = LF_Component<LF_PlayerJobComponent>.Find(pUserEntity);
		if (!jobs)
		{
			LF_Debug.Error("Jobs", "signup action: player has no LF_PlayerJobComponent");
			return;
		}

		if (!jobs.SetJob(m_eJob))
			LF_Debug.Error("Jobs", "signup action: job component refused the workplace");

		// The title names the job; the body says what to do next, at this same counter.
		LF_Utils.NotifyEntity(pUserEntity, LF_CivilianJob.SignedUpKey(m_eJob), LF_CivilianJob.NameKey(m_eJob), 5.0);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = SignUpKey(m_eJob);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected string SignUpKey(LF_EJobType job)
	{
		if (job == LF_EJobType.FUEL_DELIVERY)
			return "#EL-Job_SignUp_Fuel";
		if (job == LF_EJobType.TAXI)
			return "#EL-Job_SignUp_Taxi";

		return "#EL-Job_SignUp_Courier";
	}
}

//------------------------------------------------------------------------------------------------
class LF_JobStartRunAction : ScriptedUserAction
{
	[Attribute("5", UIWidgets.ComboBox, "Job", "", ParamEnumArray.FromEnum(LF_EJobType))]
	protected LF_EJobType m_eJob;

	//------------------------------------------------------------------------------------------------
	LF_EJobType GetJob()
	{
		return m_eJob;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return LF_Systems.IsEnabled(LF_CivilianJob.SYSTEM) && LF_CivilianJob.IsDeliveryJob(m_eJob);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		string refusal;
		if (Replication.IsServer())
		{
			string uid = LF_Utils.GetPlayerUid(user);
			if (uid.IsEmpty() || LF_CivilianJob.GetJob(uid) != m_eJob)
				refusal = "#EL-Job_NotSignedUp";
			else if (LF_CivilianJob.HasActiveRun(uid))
				refusal = "#EL-Job_NoActiveRun";
		}
		else
		{
			// The sign-ups live on the server. A dedicated client reads the job replicated on the
			// player's LF_PlayerJobComponent (set at sign-up and again when a restored sign-up is
			// put on a spawned body); reading the server map greyed "Pick up parcel" for every
			// player on a hosted server. The server re-checks everything in PerformAction.
			LF_EJobType bodyJob = LF_EJobType.UNEMPLOYED;
			LF_PlayerJobComponent jobs = LF_Component<LF_PlayerJobComponent>.Find(user);
			if (jobs)
				bodyJob = jobs.GetJob();

			refusal = ClientRefusal(bodyJob, m_eJob);
		}

		if (refusal.IsEmpty())
			return true;

		SetCannotPerformReason(refusal);
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Client offer rule: open when the job replicated on the body is this counter's job.
	//! \return "" when the run can start, else the reason key shown greyed.
	static string ClientRefusal(LF_EJobType bodyJob, LF_EJobType counterJob)
	{
		if (!LF_CivilianJob.IsDeliveryJob(counterJob) || bodyJob != counterJob)
			return "#EL-Job_NotSignedUp";

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		if (LF_Systems.Refuses(LF_CivilianJob.SYSTEM, "job start run"))
			return;

		string uid = LF_Utils.GetPlayerUid(pUserEntity);
		if (uid.IsEmpty())
		{
			LF_Debug.Error("Jobs", "start action refused: player has no account id");
			return;
		}

		if (!LF_CivilianJob.StartRun(uid, m_eJob))
			return;

		if (!LF_CivilianJob.GiveCargo(pUserEntity, m_eJob))
		{
			LF_CivilianJob.AbandonRun(uid);
			return;
		}

		// A first-time courier does not know where the drop is; the toast says.
		string started = LF_CivilianJob.RunStartedKey(m_eJob);
		if (!started.IsEmpty())
			LF_Utils.NotifyEntity(pUserEntity, started, LF_CivilianJob.NameKey(m_eJob), 8.0);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		if (m_eJob == LF_EJobType.FUEL_DELIVERY)
			outName = "#EL-Job_Pickup_Fuel";
		else if (m_eJob == LF_EJobType.TAXI)
			outName = "#EL-Job_Start_Taxi";
		else
			outName = "#EL-Job_Pickup_Parcel";

		return true;
	}
}

//------------------------------------------------------------------------------------------------
class LF_JobDropoffAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return LF_Systems.IsEnabled(LF_CivilianJob.SYSTEM);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (LF_CivilianJob.RunOfferOpen(user))
			return true;

		SetCannotPerformReason("#EL-Job_NoActiveRun");
		return false;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		if (LF_Systems.Refuses(LF_CivilianJob.SYSTEM, "job drop off"))
			return;

		string uid = LF_Utils.GetPlayerUid(pUserEntity);
		if (uid.IsEmpty())
		{
			LF_Debug.Error("Jobs", "dropoff action refused: player has no account id");
			return;
		}

		LF_EJobType finished = LF_CivilianJob.GetActiveJob(uid);
		if (!LF_CivilianJob.TakeCargo(pUserEntity, finished))
			return;

		int levelBefore = LF_CivilianJob.GetLevel(uid);
		int paid = LF_CivilianJob.CompleteRun(uid);
		if (paid <= 0)
			return;

		LF_CivilianJob.HandCash(pUserEntity, paid);

		LF_PlayerJobComponent jobs = LF_Component<LF_PlayerJobComponent>.Find(pUserEntity);
		if (jobs)
			jobs.PullCareer();
		else
			LF_Debug.Error("Jobs", "dropoff action: player has no LF_PlayerJobComponent");

		LF_Utils.NotifyEntity(pUserEntity, "#EL-Job_Paid", LF_CivilianJob.NameKey(finished), 4.0, paid.ToString());

		// Hidden progression: no level text. When the experience opens new work, the clerk says so.
		LF_EJobType opened = LF_CivilianJob.NewlyOpened(levelBefore, LF_CivilianJob.GetLevel(uid));
		if (opened != LF_EJobType.UNEMPLOYED)
			LF_Utils.NotifyEntity(pUserEntity, "#LF-Job_NewWork", LF_CivilianJob.NameKey(opened), 6.0);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#EL-Job_Dropoff";
		return true;
	}
}

//------------------------------------------------------------------------------------------------
class LF_JobAbandonAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return LF_Systems.IsEnabled(LF_CivilianJob.SYSTEM);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (LF_CivilianJob.RunOfferOpen(user))
			return true;

		SetCannotPerformReason("#EL-Job_NoActiveRun");
		return false;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		if (LF_Systems.Refuses(LF_CivilianJob.SYSTEM, "job abandon"))
			return;

		string uid = LF_Utils.GetPlayerUid(pUserEntity);
		if (uid.IsEmpty())
		{
			LF_Debug.Error("Jobs", "abandon action refused: player has no account id");
			return;
		}

		LF_EJobType finished = LF_CivilianJob.GetActiveJob(uid);
		if (!LF_CivilianJob.AbandonRun(uid))
			return;

		LF_Utils.NotifyEntity(pUserEntity, "#EL-Job_Abandoned", LF_CivilianJob.NameKey(finished), 4.0);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#EL-Job_Abandon";
		return true;
	}
}
