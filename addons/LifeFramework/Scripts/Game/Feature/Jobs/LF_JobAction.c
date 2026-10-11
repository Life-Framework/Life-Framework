//------------------------------------------------------------------------------------------------
//! Workplace actions. Offer checks do not pay and do not grant XP.
//------------------------------------------------------------------------------------------------
class LF_JobSignUpAction : ScriptedUserAction
{
	[Attribute("5", UIWidgets.ComboBox, "Job", "", ParamEnumArray.FromEnum(EL_EJobType))]
	protected EL_EJobType m_eJob;

	//------------------------------------------------------------------------------------------------
	EL_EJobType GetJob()
	{
		return m_eJob;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return LF_CivilianJob.IsDeliveryJob(m_eJob);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		string uid = EL_Utils.GetPlayerUid(user);
		if (uid.IsEmpty())
		{
			SetCannotPerformReason("#EL-Job_NotSignedUp");
			return false;
		}

		if (!LF_CivilianJob.CanSignUp(uid, m_eJob))
		{
			SetCannotPerformReason("#EL-Job_LevelRequired");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		string uid = EL_Utils.GetPlayerUid(pUserEntity);
		if (uid.IsEmpty())
		{
			EL_Debug.Error("Jobs", "signup action refused: player has no account id");
			return;
		}

		if (!LF_CivilianJob.SignUp(uid, m_eJob))
			return;

		EL_PlayerJobComponent jobs = EL_Component<EL_PlayerJobComponent>.Find(pUserEntity);
		if (!jobs)
		{
			EL_Debug.Error("Jobs", "signup action: player has no EL_PlayerJobComponent");
			return;
		}

		if (!jobs.SetJob(m_eJob))
			EL_Debug.Error("Jobs", "signup action: job component refused the workplace");

		EL_Utils.Notify(WidgetManager.Translate(LF_CivilianJob.NameKey(m_eJob)), WidgetManager.Translate(LF_CivilianJob.NameKey(m_eJob)), 3.0);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = SignUpKey(m_eJob);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected string SignUpKey(EL_EJobType job)
	{
		if (job == EL_EJobType.FUEL_DELIVERY)
			return "#EL-Job_SignUp_Fuel";
		if (job == EL_EJobType.TAXI)
			return "#EL-Job_SignUp_Taxi";

		return "#EL-Job_SignUp_Courier";
	}
}

//------------------------------------------------------------------------------------------------
class LF_JobStartRunAction : ScriptedUserAction
{
	[Attribute("5", UIWidgets.ComboBox, "Job", "", ParamEnumArray.FromEnum(EL_EJobType))]
	protected EL_EJobType m_eJob;

	//------------------------------------------------------------------------------------------------
	EL_EJobType GetJob()
	{
		return m_eJob;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return LF_CivilianJob.IsDeliveryJob(m_eJob);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		string uid = EL_Utils.GetPlayerUid(user);
		if (uid.IsEmpty() || LF_CivilianJob.GetJob(uid) != m_eJob)
		{
			SetCannotPerformReason("#EL-Job_NotSignedUp");
			return false;
		}

		if (LF_CivilianJob.HasActiveRun(uid))
		{
			SetCannotPerformReason("#EL-Job_NoActiveRun");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		string uid = EL_Utils.GetPlayerUid(pUserEntity);
		if (uid.IsEmpty())
		{
			EL_Debug.Error("Jobs", "start action refused: player has no account id");
			return;
		}

		if (!LF_CivilianJob.StartRun(uid, m_eJob))
			return;

		if (!LF_CivilianJob.GiveCargo(pUserEntity, m_eJob))
			LF_CivilianJob.AbandonRun(uid);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		if (m_eJob == EL_EJobType.FUEL_DELIVERY)
			outName = "#EL-Job_Pickup_Fuel";
		else if (m_eJob == EL_EJobType.TAXI)
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
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		string uid = EL_Utils.GetPlayerUid(user);
		if (uid.IsEmpty() || !LF_CivilianJob.HasActiveRun(uid))
		{
			SetCannotPerformReason("#EL-Job_NoActiveRun");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		string uid = EL_Utils.GetPlayerUid(pUserEntity);
		if (uid.IsEmpty())
		{
			EL_Debug.Error("Jobs", "dropoff action refused: player has no account id");
			return;
		}

		EL_EJobType finished = LF_CivilianJob.GetActiveJob(uid);
		if (!LF_CivilianJob.TakeCargo(pUserEntity, finished))
			return;

		int paid = LF_CivilianJob.CompleteRun(uid);
		if (paid <= 0)
			return;

		LF_CivilianJob.HandCash(pUserEntity, paid);

		EL_PlayerJobComponent jobs = EL_Component<EL_PlayerJobComponent>.Find(pUserEntity);
		if (jobs)
			jobs.PullCareer();
		else
			EL_Debug.Error("Jobs", "dropoff action: player has no EL_PlayerJobComponent");

		EL_Utils.Notify(WidgetManager.Translate("#EL-Job_Paid", paid), WidgetManager.Translate(LF_CivilianJob.NameKey(finished)), 4.0);
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
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		string uid = EL_Utils.GetPlayerUid(user);
		if (uid.IsEmpty() || !LF_CivilianJob.HasActiveRun(uid))
		{
			SetCannotPerformReason("#EL-Job_NoActiveRun");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		string uid = EL_Utils.GetPlayerUid(pUserEntity);
		if (uid.IsEmpty())
		{
			EL_Debug.Error("Jobs", "abandon action refused: player has no account id");
			return;
		}

		EL_EJobType finished = LF_CivilianJob.GetActiveJob(uid);
		if (!LF_CivilianJob.AbandonRun(uid))
			return;

		EL_Utils.Notify(WidgetManager.Translate("#EL-Job_Abandoned"), WidgetManager.Translate(LF_CivilianJob.NameKey(finished)), 4.0);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#EL-Job_Abandon";
		return true;
	}
}
