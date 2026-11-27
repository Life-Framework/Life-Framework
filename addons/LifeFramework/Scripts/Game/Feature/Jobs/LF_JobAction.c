//------------------------------------------------------------------------------------------------
//! Workplace actions. Offer checks do not pay and do not grant XP.
//------------------------------------------------------------------------------------------------
//! The standing gate of a job counter (#2701): sign-up reads the gate `job:<JOB>`, a run start the
//! gate `run:<JOB>` (LF_ReputationGateDef rows). Standing lives on the server, so the greyed offer
//! reads it only where the server runs; the server decides again on use.
class LF_JobStandingGate
{
	//------------------------------------------------------------------------------------------------
	static string GateId(string verb, LF_EJobType job)
	{
		return verb + ":" + typename.EnumToString(LF_EJobType, job);
	}

	//------------------------------------------------------------------------------------------------
	//! \return "" when the player's standing passes the gate, else the refusal `#` key (the faction's words),
	//! with the minimum tier's word key in tierWord.
	static string Refusal(string uid, string verb, LF_EJobType job, out string tierWord)
	{
		return LF_Reputation.Refusal(uid, GateId(verb, job), tierWord);
	}
}

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
	//! \return "" when the level is enough, else the job-level words (LF_JobLevelWords, never a number).
	protected string LevelRefusal(IEntity user, string uid)
	{
		int need = LF_CivilianJob.RequiredLevel(m_eJob);
		if (Replication.IsServer())
			return LF_JobLevelWords.Refusal(LF_JobLevels.JOB_DELIVERY, LF_CivilianJob.GetLevel(uid), need);

		int level = 1;
		LF_PlayerJobComponent jobs = LF_Component<LF_PlayerJobComponent>.Find(user);
		if (jobs)
			level = Math.Max(1, jobs.GetJobLevel());

		return LF_JobLevelWords.Refusal(LF_JobLevels.JOB_DELIVERY, level, need);
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

		string refusal = LevelRefusal(user, uid);
		if (!refusal.IsEmpty() || !LF_CivilianJob.IsDeliveryJob(m_eJob))
		{
			if (refusal.IsEmpty())
				refusal = "#LF-Job_MoreRunsFirst";

			SetCannotPerformReason(LF_Terms.Text(refusal));
			return false;
		}

		// Standing is server state: a dedicated client cannot read it, so the server decides on use.
		if (Replication.IsServer())
		{
			string tierWord;
			string standing = LF_JobStandingGate.Refusal(uid, "job", m_eJob, tierWord);
			if (!standing.IsEmpty())
			{
				SetCannotPerformReason(LF_Reputation.RefusalText(standing, tierWord));
				return false;
			}
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

		string tierWord;
		string standing = LF_JobStandingGate.Refusal(uid, "job", m_eJob, tierWord);
		if (!standing.IsEmpty())
		{
			LF_Debug.Info("Jobs", string.Format("signup refused: %1 lacks the standing for %2", uid, LF_JobStandingGate.GateId("job", m_eJob)));
			LF_Utils.NotifyEntity(pUserEntity, standing, LF_CivilianJob.NameKey(m_eJob), 6.0, tierWord);
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

	//! How far from the rank's pad the cab may stand, in metres.
	static const float TAXI_CAB_REACH_M = 15; // lf-fixed: a kerb's reach from the rank sign, not a gameplay knob

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
		string tierWord;
		if (Replication.IsServer())
		{
			string uid = LF_Utils.GetPlayerUid(user);
			if (uid.IsEmpty() || LF_CivilianJob.GetJob(uid) != m_eJob)
				refusal = "#EL-Job_NotSignedUp";
			else if (LF_CivilianJob.HasActiveRun(uid))
				refusal = "#EL-Job_NoActiveRun";
			else
				refusal = LF_JobStandingGate.Refusal(uid, "run", m_eJob, tierWord);
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

		SetCannotPerformReason(LF_Reputation.RefusalText(refusal, tierWord));
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

	//! The toast key for a start that did not begin a run; "" for STARTED. A refused start never
	//! returns in silence (#3356: "Pick up parcel" did nothing while a dead run was still open).
	static string StartRefusalKey(LF_ERunStart outcome)
	{
		if (outcome == LF_ERunStart.STARTED)
			return string.Empty;
		if (outcome == LF_ERunStart.NO_DEPOSIT)
			return LF_CivilianJob.KEY_NEED_DEPOSIT;
		if (outcome == LF_ERunStart.ALREADY_RUN)
			return LF_CivilianJob.KEY_ALREADY_RUN;
		if (outcome == LF_ERunStart.NO_CARGO)
			return LF_CivilianJob.KEY_NO_ROOM;

		return LF_CivilianJob.KEY_START_REFUSED;
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

		// Standing gate (#2701): the server decides again on use, whatever the client offered.
		string tierWord;
		string standing = LF_JobStandingGate.Refusal(uid, "run", m_eJob, tierWord);
		if (!standing.IsEmpty())
		{
			LF_Debug.Info("Jobs", string.Format("start run refused: %1 lacks the standing for %2", uid, LF_JobStandingGate.GateId("run", m_eJob)));
			LF_Utils.NotifyEntity(pUserEntity, standing, LF_CivilianJob.NameKey(m_eJob), 6.0, tierWord);
			return;
		}

		// The cargo deposit comes off the player's cash first; a player short of it gets no run and no cargo.
		int deposit;
		LF_ERunStart started = LF_CivilianJob.BeginRun(pUserEntity, uid, m_eJob, LF_JobGuards.Now(), deposit);
		if (started == LF_ERunStart.NO_DEPOSIT)
		{
			LF_Utils.NotifyEntity(pUserEntity, StartRefusalKey(started), LF_CivilianJob.NameKey(m_eJob), 6.0, deposit.ToString());
			return;
		}

		if (started != LF_ERunStart.STARTED)
		{
			LF_Utils.NotifyEntity(pUserEntity, StartRefusalKey(started), LF_CivilianJob.NameKey(m_eJob), 6.0);
			return;
		}

		// No free cars: the rank's locked cab opens for the driver who started the fare, nobody else.
		if (m_eJob == LF_EJobType.TAXI)
			LF_VehicleLockComponent.HandNearestJobVehicle(pOwnerEntity, TAXI_CAB_REACH_M, pUserEntity);

		// The nearest drop goes on the map and the waypoint (LF_JobMapMarks; a taxi run gets none).
		LF_JobMapMarks.OnRunStarted(uid, m_eJob, pUserEntity.GetOrigin());

		// A first-time courier does not know where the drop is; the toast says.
		string startedKey = LF_CivilianJob.RunStartedKey(m_eJob);
		if (!startedKey.IsEmpty())
			LF_Utils.NotifyEntity(pUserEntity, startedKey, LF_CivilianJob.NameKey(m_eJob), 8.0, deposit.ToString());
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

		// The rules (a placed drop, the distance, the time, a taxi's passenger) and the payout are one call.
		LF_EJobType finished = LF_CivilianJob.GetActiveJob(uid);
		int levelBefore = LF_CivilianJob.GetLevel(uid);
		string refusal;
		int paid = LF_CivilianJob.Dropoff(pUserEntity, uid, pOwnerEntity, LF_JobGuards.Now(), refusal);
		if (!refusal.IsEmpty())
			LF_Utils.NotifyEntity(pUserEntity, refusal, LF_CivilianJob.NameKey(finished), 5.0);

		if (paid <= 0)
			return;

		LF_PlayerJobComponent jobs = LF_Component<LF_PlayerJobComponent>.Find(pUserEntity);
		if (jobs)
			jobs.PullCareer();
		else
			LF_Debug.Error("Jobs", "dropoff action: player has no LF_PlayerJobComponent");

		// The toast names each part of the pay (#3148): the run, the night rate, the bonus.
		LF_JobPayParts parts = LF_CivilianJob.TakeLastParts(uid);
		if (!parts)
			parts = LF_JobPayParts.Make(paid, 0, 0);

		LF_Utils.NotifyEntity(pUserEntity, parts.ToastKey(), LF_CivilianJob.NameKey(finished), 4.0, parts.ToastArg());

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
		int forfeited = LF_CivilianJob.GetDeposit(uid);
		if (!LF_CivilianJob.AbandonRun(uid))
			return;

		if (forfeited > 0)
			LF_Utils.NotifyEntity(pUserEntity, "#LF-Job_AbandonedDeposit", LF_CivilianJob.NameKey(finished), 5.0, forfeited.ToString());
		else
			LF_Utils.NotifyEntity(pUserEntity, "#EL-Job_Abandoned", LF_CivilianJob.NameKey(finished), 4.0);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#EL-Job_Abandon";
		return true;
	}
}
