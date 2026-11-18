//------------------------------------------------------------------------------------------------
//! The bus subscriber that moves standing (#2697, docs/architecture/reputation.md section 3). No
//! feature calls LF_Reputation to report work: each publishes its event and this one listens, as
//! LF_CrimeRecordSubscriber does. Every change is a config award row (LF_ReputationAwardDef), so the
//! points, the faction and the cooldown are data; the code only picks the row for the event.
//!
//! GAIN NEEDS WORK, LOSS NEEDS PROOF. A gain comes from a finished run, a statement, a recovery, a
//! sale. A loss comes only from an arrest (the conviction), an identification, a flagged logout.
//! A wanted level changing or a cleared accusation never costs standing (no subscription to those).
//! The factions are independent: one event can feed two (an arrest costs town and police and gives
//! the underworld a little).
//!
//! Server only; the event bus is server only. An event with no player id (a town NPC witness) is
//! logged and skipped.
class LF_ReputationSubscriber : LF_EventSubscriber
{
	//! Award row ids (Configs/Reputation/LF_Reputation.conf). A job also tries "job_completed:<JOB>".
	static const string ACTION_JOB_COMPLETED = "job_completed";
	static const string ACTION_ARREST_TOWN = "arrest_town";
	static const string ACTION_ARREST_POLICE = "arrest_police";
	static const string ACTION_ARREST_UNDERWORLD = "arrest_underworld";
	static const string ACTION_SUSPECT_IDENTIFIED = "suspect_identified";
	static const string ACTION_WITNESS_STATEMENT = "witness_statement";
	static const string ACTION_WITNESS_UNDERWORLD = "witness_statement_underworld";
	static const string ACTION_VEHICLE_RECOVERED = "vehicle_recovered";
	static const string ACTION_FLAGGED_LOGOUT = "flagged_logout";
	static const string ACTION_FENCE_SALE = "fence_sale";
	static const string ACTION_VEHICLE_CHOPPED = "vehicle_chopped";
	static const string ACTION_CHAIN_STEP = "chain_step";
	static const string ACTION_HEIST_LOOT = "heist_loot";

	//! The chain station event kind that counts: the product was taken out (a load or a ready is not work done).
	static const string CHAIN_KIND_COLLECT = "collect";

	protected static ref LF_ReputationSubscriber s_Subscriber;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_Subscriber = null;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Every event type standing listens to.
	static array<typename> EventTypes()
	{
		array<typename> types = {};
		types.Insert(LF_OnJobCompleted);
		types.Insert(LF_OnArrested);
		types.Insert(LF_OnSuspectIdentified);
		types.Insert(LF_OnWitnessStatement);
		types.Insert(LF_OnVehicleRecovered);
		types.Insert(LF_OnFlaggedLogout);
		types.Insert(LF_OnFenceSale);
		types.Insert(LF_OnVehicleChopped);
		types.Insert(LF_OnChainStep);
		types.Insert(LF_OnHeistLoot);
		return types;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: subscribes to every standing event once. Called when the game mode starts.
	//! \return True when the subscriber is on the bus for every event type.
	static bool EnsureSubscribed()
	{
		if (!Replication.IsServer())
			return false;

		if (!s_Subscriber)
			s_Subscriber = new LF_ReputationSubscriber();

		bool all = true;
		foreach (typename eventType : EventTypes())
		{
			if (LF_EventBus.IsSubscribed(eventType, s_Subscriber))
				continue;

			if (!LF_EventBus.Subscribe(eventType, s_Subscriber))
				all = false;
		}

		return all;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: unsubscribes from every event type (a bus reset drops the list on its own).
	static void Unsubscribe()
	{
		if (!s_Subscriber)
			return;

		foreach (typename eventType : EventTypes())
		{
			LF_EventBus.Unsubscribe(eventType, s_Subscriber);
		}
	}

	//------------------------------------------------------------------------------------------------
	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnJobCompleted job = LF_OnJobCompleted.Cast(busEvent);
		if (job)
			return OnJobCompleted(job);

		LF_OnArrested arrest = LF_OnArrested.Cast(busEvent);
		if (arrest)
			return OnArrested(arrest);

		LF_OnSuspectIdentified identified = LF_OnSuspectIdentified.Cast(busEvent);
		if (identified)
		{
			Award(identified.m_sPlayerUid, ACTION_SUSPECT_IDENTIFIED);
			return true;
		}

		LF_OnWitnessStatement statement = LF_OnWitnessStatement.Cast(busEvent);
		if (statement)
			return OnWitnessStatement(statement);

		LF_OnVehicleRecovered recovered = LF_OnVehicleRecovered.Cast(busEvent);
		if (recovered)
		{
			Award(recovered.m_sOfficerUid, ACTION_VEHICLE_RECOVERED);
			return true;
		}

		LF_OnFlaggedLogout logout = LF_OnFlaggedLogout.Cast(busEvent);
		if (logout)
		{
			Award(logout.m_sPlayerUid, ACTION_FLAGGED_LOGOUT);
			return true;
		}

		LF_OnFenceSale sale = LF_OnFenceSale.Cast(busEvent);
		if (sale)
		{
			Award(sale.m_sBuyerUid, ACTION_FENCE_SALE);
			return true;
		}

		LF_OnVehicleChopped chopped = LF_OnVehicleChopped.Cast(busEvent);
		if (chopped)
		{
			Award(chopped.m_sChopperUid, ACTION_VEHICLE_CHOPPED);
			return true;
		}

		LF_OnChainStep step = LF_OnChainStep.Cast(busEvent);
		if (step)
		{
			if (step.m_sKind == CHAIN_KIND_COLLECT)
				Award(step.m_sWorkerUid, ACTION_CHAIN_STEP);

			return true;
		}

		LF_OnHeistLoot loot = LF_OnHeistLoot.Cast(busEvent);
		if (loot)
		{
			Award(loot.m_sPlayerUid, ACTION_HEIST_LOOT);
			return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! A paid civilian run raises town standing. Duty (police, medic) and no job are not a civilian run.
	//! A row "job_completed:<JOB>" (e.g. job_completed:TAXI) overrides the general row for that job.
	protected bool OnJobCompleted(notnull LF_OnJobCompleted job)
	{
		if (job.m_iJob == LF_EJobType.UNEMPLOYED || job.m_iJob == LF_EJobType.POLICE || job.m_iJob == LF_EJobType.MEDIC)
		{
			LF_Debug.Info(LF_Reputation.FEATURE, string.Format("standing skipped: job %1 is duty or no job, not a civilian run", job.m_iJob));
			return true;
		}

		string action = ACTION_JOB_COMPLETED;
		string specific = ACTION_JOB_COMPLETED + ":" + typename.EnumToString(LF_EJobType, job.m_iJob);
		LF_ReputationConfig config = LF_ReputationConfig.Get();
		if (config && config.FindAward(specific))
			action = specific;

		Award(job.m_sPlayerUid, action);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! An arrest is the conviction: it costs town and police and gives the underworld a little.
	protected bool OnArrested(notnull LF_OnArrested arrest)
	{
		Award(arrest.m_sPlayerUid, ACTION_ARREST_TOWN);
		Award(arrest.m_sPlayerUid, ACTION_ARREST_POLICE);
		Award(arrest.m_sPlayerUid, ACTION_ARREST_UNDERWORLD);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A player witness gains police standing. A statement that describes a suspect (clothing, vehicle or
	//! plate) also costs the witness a little underworld standing (outline R4). A town NPC has no uid.
	protected bool OnWitnessStatement(notnull LF_OnWitnessStatement statement)
	{
		if (statement.m_bNpc || statement.m_sWitnessUid.IsEmpty())
		{
			LF_Debug.Info(LF_Reputation.FEATURE, string.Format("standing skipped: statement on report %1 has no player witness", statement.m_iReport));
			return true;
		}

		Award(statement.m_sWitnessUid, ACTION_WITNESS_STATEMENT);
		if (!statement.m_sClothing.IsEmpty() || !statement.m_sVehicle.IsEmpty() || !statement.m_sPlateFragment.IsEmpty())
			Award(statement.m_sWitnessUid, ACTION_WITNESS_UNDERWORLD);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Awards one config action; an empty uid (an NPC, an unowned vehicle) is skipped with a log.
	protected static int Award(string playerUid, string action)
	{
		if (playerUid.IsEmpty())
		{
			LF_Debug.Info(LF_Reputation.FEATURE, string.Format("standing skipped: action '%1' has no player id", action));
			return 0;
		}

		return LF_Reputation.Award(playerUid, action);
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_ReputationSubscriber's statics (LF_StaticReset finds it by reflection).
class LF_ReputationSubscriber_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_ReputationSubscriber";
	}

	override void Apply()
	{
		LF_ReputationSubscriber.ResetStatics();
	}
}
