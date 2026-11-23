// LF_HospitalPresence.c - who must be on duty before the pharmacy cabinet can be robbed, and the
// big-job lock around it (#4446, 0.3.0 Crime D2, rung 4 of the robbery ladder).
//
// Rule. The cabinet opens only with at least one medic OR at least two police officers on duty
// (pack config), and only when no other big job runs and the pharmacy's own cooldown is over. A
// refusal is said in words: the cabinet action's disabled reason on the server, and a toast from
// the server's perform check for a dedicated client (the medic count lives on the server only).
//
// Lock. A breach that arms the cabinet takes LF_BigJobLock as "pharmacy". The job ends (the lock is
// released and the 2 h cooldown starts) a hold after the arm, which covers the fuse and the looting.
// A restart before that saves it as the full cooldown (LF_BigJobLock's "bigjob" section).
//
// Scope. The gate sits on the player path (LF_HospitalCabinetAction, modded below), so
// LF_Hospital.CanBreach / BreachCabinet stay the raw breach the scripted tests and events use.

//------------------------------------------------------------------------------------------------
//! The pharmacy rung's tuning. The field defaults are the core values; a pack lists a whole
//! LF_HospitalPresenceConfig in its mission header overlays to change them (the last one wins).
[BaseContainerProps(configRoot: true)]
class LF_HospitalPresenceConfig
{
	[Attribute("1", UIWidgets.EditBox, "On-duty medics that alone allow the pharmacy robbery", params: "0 32")]
	int m_iMinMedics;

	[Attribute("2", UIWidgets.EditBox, "On-duty police officers that alone allow the pharmacy robbery", params: "0 32")]
	int m_iMinPolice;

	[Attribute("7200", UIWidgets.EditBox, "Seconds the pharmacy job cools down after it ends", params: "60 86400")]
	int m_iCooldownSeconds;

	[Attribute("900", UIWidgets.EditBox, "Seconds after arming the cabinet that the job holds the big-job lock (fuse plus looting)", params: "30 2700")]
	int m_iHoldSeconds;

	protected static ref LF_HospitalPresenceConfig s_Config;

	//------------------------------------------------------------------------------------------------
	static LF_HospitalPresenceConfig Defaults()
	{
		LF_HospitalPresenceConfig config = new LF_HospitalPresenceConfig();
		// The same values as the Attribute defaults above (a script-built instance does not read them).
		config.m_iMinMedics = 1;
		config.m_iMinPolice = 2;
		config.m_iCooldownSeconds = LF_BigJobLock.COOLDOWN_PHARMACY;
		config.m_iHoldSeconds = 900;
		return config;
	}

	//------------------------------------------------------------------------------------------------
	//! The live tuning: the core defaults, replaced by the last pack overlay of this class.
	static LF_HospitalPresenceConfig Get()
	{
		if (s_Config)
			return s_Config;

		s_Config = Defaults();
		array<ref Resource> resources = {};
		array<ResourceName> names = {};
		LF_ConfigOverlay.LoadPackOverlays(ResourceName.Empty, LF_HospitalPresenceConfig, "hospitalPresence", resources, names);
		foreach (int i, Resource resource : resources)
		{
			LF_HospitalPresenceConfig overlay = LF_HospitalPresenceConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(resource.GetResource().ToBaseContainer()));
			if (!overlay)
			{
				LF_Debug.Error(LF_HospitalPresence.FEATURE, "pharmacy presence overlay has an unexpected root class: " + names[i]);
				continue;
			}

			s_Config = overlay;
			LF_Debug.Info(LF_HospitalPresence.FEATURE, "pharmacy presence tuning from " + names[i]);
		}

		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests swap the tuning; null drops back to the loaded one.
	static void UseForTest(LF_HospitalPresenceConfig config)
	{
		s_Config = config;
	}
}

//------------------------------------------------------------------------------------------------
class LF_HospitalPresence
{
	static const string FEATURE = "Hospital";
	//! The big-job lock key (the same every boot; the "bigjob" save section stores it).
	static const string JOB_KEY = "pharmacy";
	static const string KEY_TOO_FEW = "#LF-HospitalPresence_TooFew";

	//! Tests only: fixed duty counts instead of the live server count; negative uses the server.
	protected static int s_iMedicsOverride = -1;
	protected static int s_iPoliceOverride = -1;

	//------------------------------------------------------------------------------------------------
	static void SetCountsForTests(int medics, int police)
	{
		s_iMedicsOverride = medics;
		s_iPoliceOverride = police;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_iMedicsOverride = -1;
		s_iPoliceOverride = -1;
		LF_HospitalPresenceConfig.UseForTest(null);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: on-duty medics among the connected players.
	static int CountMedicsOnDuty()
	{
		if (s_iMedicsOverride >= 0)
			return s_iMedicsOverride;

		if (!GetGame())
			return 0;

		PlayerManager playerManager = GetGame().GetPlayerManager();
		LF_PlayerAccountManager accountManager = LF_PlayerAccountManager.GetInstance();
		if (!playerManager || !accountManager)
			return 0;

		int count = 0;
		array<int> playerIds = {};
		playerManager.GetPlayers(playerIds);
		foreach (int playerId : playerIds)
		{
			if (LF_MedicDuty.IsOnDuty(accountManager.GetAccount(LF_Utils.GetAccountUID(playerId))))
				count++;
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: on-duty police officers among the connected players.
	static int CountPoliceOnDuty()
	{
		if (s_iPoliceOverride >= 0)
			return s_iPoliceOverride;

		return LF_AccountReplication.CountPoliceOnDuty();
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: one medic or two police (the config's numbers) are enough.
	static bool PresenceMet(int medics, int police, LF_HospitalPresenceConfig config)
	{
		if (!config)
			config = LF_HospitalPresenceConfig.Get();

		return medics >= config.m_iMinMedics || police >= config.m_iMinPolice;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: why the cabinet may not be robbed now, as a string-table key; empty when it may.
	//! \param why The reason for the server log.
	static string RefusalKey(int now, out string why)
	{
		why = string.Empty;
		LF_HospitalPresenceConfig config = LF_HospitalPresenceConfig.Get();
		int medics = CountMedicsOnDuty();
		int police = CountPoliceOnDuty();
		if (!PresenceMet(medics, police, config))
		{
			why = string.Format("too few on duty (%1 medics of %2, %3 police of %4)", medics, config.m_iMinMedics, police, config.m_iMinPolice);
			return KEY_TOO_FEW;
		}

		string running = LF_BigJobLock.RunningJob(now);
		if (!running.IsEmpty())
		{
			why = string.Format("big job %1 is running", running);
			return LF_BigJobLock.RefusalKey(LF_EBigJobLock.BUSY);
		}

		int left = LF_BigJobLock.CooldownLeft(JOB_KEY, now);
		if (left > 0)
		{
			why = string.Format("cooling down, %1 s left", left);
			return LF_BigJobLock.RefusalKey(LF_EBigJobLock.COOLDOWN);
		}

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! The refusal in words for the player (the cooldown with its minutes left).
	static string RefusalText(string key, int now)
	{
		if (key == LF_BigJobLock.RefusalKey(LF_EBigJobLock.COOLDOWN))
			return LF_BigJobLock.RefusalText(LF_EBigJobLock.COOLDOWN, JOB_KEY, now);

		return LF_Terms.Text(key);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the player's cabinet robbery. Checks presence and the lock before the charge is taken
	//! (a refusal costs nothing), breaches, then takes the big-job lock and schedules the job's end.
	static bool Breach(string targetId, string playerUid, IEntity user)
	{
		int now = LF_BigJobLock.Now();
		string why;
		string key = RefusalKey(now, why);
		if (!key.IsEmpty())
		{
			LF_Debug.Info(FEATURE, string.Format("pharmacy cabinet %1 refused for %2: %3", targetId, playerUid, why));
			if (user)
				LF_Utils.NotifyEntity(user, RefusalText(key, now), "", 4.0);

			return false;
		}

		if (!LF_Hospital.BreachCabinet(targetId, playerUid, user))
			return false;

		LF_HospitalPresenceConfig config = LF_HospitalPresenceConfig.Get();
		LF_EBigJobLock taken = LF_BigJobLock.TryBegin(JOB_KEY, playerUid, config.m_iCooldownSeconds, now);
		if (taken != LF_EBigJobLock.OK)
		{
			// RefusalKey said free a moment ago in the same call; this only fires on a broken key.
			LF_Debug.Error(FEATURE, string.Format("pharmacy cabinet %1 opened but the big-job lock refused: %2", targetId, typename.EnumToString(LF_EBigJobLock, taken)));
			return true;
		}

		LF_Debug.Info(FEATURE, string.Format("pharmacy cabinet %1 armed by %2: big-job lock taken, ends in %3 s", targetId, playerUid, config.m_iHoldSeconds));
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().CallLater(LF_HospitalPresence.EndJob, config.m_iHoldSeconds * 1000, false);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The pharmacy job is over: releases the lock and starts its cooldown. A no-op when another
	//! job holds the lock (the pharmacy's already ran out through LF_BigJobLock's stale expiry).
	static void EndJob()
	{
		int now = LF_BigJobLock.Now();
		if (LF_BigJobLock.Finish(JOB_KEY, now))
			LF_Debug.Info(FEATURE, string.Format("pharmacy job ended; %1 s cooldown starts", LF_BigJobLock.CooldownLeft(JOB_KEY, now)));
	}
}

//------------------------------------------------------------------------------------------------
//! The cabinet action asks the presence rule and the big-job lock; the refusal is its disabled reason.
modded class LF_HospitalCabinetAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (!super.CanBePerformedScript(user))
			return false;

		// The medic count and the lock live on the server; a dedicated client's perform is refused
		// there with the same words as a toast.
		if (!Replication.IsServer())
			return true;

		string why;
		int now = LF_BigJobLock.Now();
		string key = LF_HospitalPresence.RefusalKey(now, why);
		if (key.IsEmpty())
			return true;

		// The words, not the key: the cooldown line carries its minutes left.
		SetCannotPerformReason(LF_HospitalPresence.RefusalText(key, now));
		return false;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		LF_HospitalCabinetComponent cabinet = LF_Component<LF_HospitalCabinetComponent>.Find(pOwnerEntity);
		if (!cabinet)
		{
			LF_Debug.Error("Hospital", "cabinet action: owner has no LF_HospitalCabinetComponent");
			return;
		}

		string playerUid = LF_Utils.GetPlayerUid(pUserEntity);
		if (playerUid.IsEmpty())
		{
			LF_Debug.Error("Hospital", "cabinet action refused: player has no account id");
			return;
		}

		LF_HospitalPresence.Breach(cabinet.GetTargetId(), playerUid, pUserEntity);
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_HospitalPresence's statics (LF_StaticReset finds it by reflection).
class LF_HospitalPresence_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_HospitalPresence";
	}

	override void Apply()
	{
		LF_HospitalPresence.ResetStatics();
	}
}
