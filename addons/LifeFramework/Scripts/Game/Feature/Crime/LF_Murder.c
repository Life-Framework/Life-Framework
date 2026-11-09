//------------------------------------------------------------------------------------------------
//! Murder (M1-27, docs/decisions.md "Killing anyone makes you wanted for murder").
//! A player who kills another character, a player or an NPC, is wanted at once. The crime goes
//! through the same report path as a robbery: LF_WantedReports records the killer's clothes,
//! vehicle, and place, never the name, and every on-duty officer gets an alert.
//!
//! What does not count: a death with no player behind it (an NPC, a fall, a fire), a self-kill,
//! and, by default (LF_Police.conf m_bOfficerKillOfWantedExempt), an on-duty officer killing a
//! wanted suspect. Repeat offenders are an admin matter: no ban, no kick, no karma.
//!
//! Entry: LF_GameModeRoleplay.OnControllableDestroyedEx on the server, which fires once per
//! character death, players and NPCs alike.
//------------------------------------------------------------------------------------------------
class LF_Murder
{
	static const string FEATURE = "Murder";

	static const string SKIP_NO_KILLER = "no player killer";
	static const string SKIP_SELF = "self-kill";
	static const string SKIP_NOT_CHARACTER = "victim is not a character";
	static const string SKIP_OFFICER_ON_WANTED = "on-duty officer killed a wanted suspect";

	protected static int s_iRecorded;

	//------------------------------------------------------------------------------------------------
	//! Pure rule: why this death is not a murder, or "" when it is.
	//! \param killerPlayerId Player id behind the kill, 0 or less when no player did it.
	//! \param victimPlayerId Player id of the victim, 0 or less for an NPC.
	//! \param victimIsCharacter The destroyed controllable is a character (not a vehicle or a turret).
	//! \param killerIsVictim The killer entity is the victim's own body.
	//! \param killerOnDutyOfficer The killer is clocked-in police.
	//! \param victimWanted The victim's wanted level (0 for an NPC).
	//! \param officerExempt Config: an on-duty officer killing a wanted suspect is not murder.
	static string SkipReason(int killerPlayerId, int victimPlayerId, bool victimIsCharacter, bool killerIsVictim, bool killerOnDutyOfficer, int victimWanted, bool officerExempt)
	{
		if (!victimIsCharacter)
			return SKIP_NOT_CHARACTER;

		if (killerPlayerId <= 0)
			return SKIP_NO_KILLER;

		if (killerIsVictim || (victimPlayerId > 0 && victimPlayerId == killerPlayerId))
			return SKIP_SELF;

		if (officerExempt && killerOnDutyOfficer && victimWanted > 0)
			return SKIP_OFFICER_ON_WANTED;

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a controllable died. Decides whether it was murder and records it.
	//! \return True when a murder was recorded.
	static bool OnCharacterKilled(IEntity victim, IEntity killerEntity, int victimPlayerId, int killerPlayerId)
	{
		if (!Replication.IsServer())
			return false;

		bool isCharacter;
		if (ChimeraCharacter.Cast(victim))
			isCharacter = true;

		bool killerIsVictim;
		if (victim && killerEntity == victim)
			killerIsVictim = true;

		bool killerOnDuty;
		string killerUid;
		if (killerPlayerId > 0)
		{
			killerUid = LF_Utils.GetPlayerUID(killerPlayerId);
			LF_PlayerAccount killerAccount = FindAccount(killerUid);
			if (killerAccount)
				killerOnDuty = killerAccount.GetFaction() == LF_Faction.POLICE && killerAccount.IsOnDuty();
		}

		int victimWanted;
		string victimLabel = "npc";
		if (victimPlayerId > 0)
		{
			string victimUid = LF_Utils.GetPlayerUID(victimPlayerId);
			victimLabel = victimUid;
			LF_PlayerAccount victimAccount = FindAccount(victimUid);
			if (victimAccount)
				victimWanted = victimAccount.GetWantedLevel();
		}

		string skip = SkipReason(killerPlayerId, victimPlayerId, isCharacter, killerIsVictim, killerOnDuty, victimWanted, OfficerExempt());
		if (victimPlayerId > 0)
		{
			string killerPlayerUid;
			if (killerPlayerId > 0 && !killerIsVictim)
				killerPlayerUid = killerUid;
			LF_OnPlayerKilled killed = LF_OnPlayerKilled.Create(victimLabel, killerPlayerUid);
			if (victim)
				killed.m_vPosition = victim.GetOrigin();
			killed.m_sWeapon = LF_IncidentLog.WeaponOf(killerEntity);
			killed.m_bMurder = skip.IsEmpty();
			LF_EventBus.Publish(killed);
		}

		if (!skip.IsEmpty())
		{
			// Vehicles and props die all the time; only character deaths are worth a line.
			if (isCharacter)
				LF_Debug.Info(FEATURE, string.Format("death of %1 is not murder: %2", victimLabel, skip));
			return false;
		}

		return Record(killerUid, killerEntity, victimLabel);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: raises the killer's wanted by the configured amount, files the crime report with
	//! their description, and alerts on-duty police. The seam the LOGIC test drives.
	//! \param killerUid Persistent uid of the killer.
	//! \param killer The killer's body, described for the report. May be null (empty description).
	//! \param victimLabel For the log only.
	static bool Record(string killerUid, IEntity killer, string victimLabel)
	{
		if (killerUid.IsEmpty())
		{
			LF_Debug.Error(FEATURE, string.Format("murder of %1 not recorded: the killer has no uid", victimLabel));
			return false;
		}

		LF_PlayerAccount account = LF_PlayerAccountManager.GetOrCreate(killerUid);
		if (!account)
		{
			LF_Debug.Error(FEATURE, string.Format("murder of %1 not recorded: no account for %2", victimLabel, killerUid));
			return false;
		}

		int amount = WantedAmount();
		account.IncreaseWantedLevel(amount);
		LF_PlayerAccountManager accountManager = LF_PlayerAccountManager.GetInstance();
		if (accountManager)
			accountManager.SaveAndReleaseAccount(account);

		// A street has no alarm bell: the report and the radio call only (no site, LF_Dispatch).
		LF_Dispatch.Report(killerUid, killer, LF_WantedReports.OFFENCE_MURDER, vector.Zero);
		if (killer)
			LF_Utils.NotifyEntity(killer, "#LF-Murder_Wanted", "#LF-Murder_Title", 6.0);

		s_iRecorded++;
		LF_Debug.Info(FEATURE, string.Format("murder by %1 of %2: wanted +%3 -> %4", killerUid, victimLabel, amount, account.GetWantedLevel()));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Murders recorded since the server started. Tests read it.
	static int GetRecordedCount()
	{
		return s_iRecorded;
	}

	//------------------------------------------------------------------------------------------------
	static int WantedAmount()
	{
		LF_PoliceConfig config = LF_PoliceConfig.Get();
		if (!config)
			return LF_PoliceConfig.DEFAULT_MURDER_WANTED;

		if (config.m_iMurderWanted < 1)
		{
			LF_Debug.Error(FEATURE, string.Format("murder wanted amount %1 is below 1; using %2", config.m_iMurderWanted, LF_PoliceConfig.DEFAULT_MURDER_WANTED));
			return LF_PoliceConfig.DEFAULT_MURDER_WANTED;
		}

		return config.m_iMurderWanted;
	}

	//------------------------------------------------------------------------------------------------
	static bool OfficerExempt()
	{
		LF_PoliceConfig config = LF_PoliceConfig.Get();
		if (!config)
			return LF_PoliceConfig.DEFAULT_OFFICER_KILL_OF_WANTED_EXEMPT;

		return config.m_bOfficerKillOfWantedExempt;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_PlayerAccount FindAccount(string uid)
	{
		if (uid.IsEmpty())
			return null;

		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		if (!accounts)
			return null;

		return accounts.GetAccount(uid);
	}
}
