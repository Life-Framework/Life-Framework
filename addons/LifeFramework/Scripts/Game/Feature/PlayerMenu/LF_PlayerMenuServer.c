// LF_PlayerMenuServer.c: the server fills the personal file of ONE player, the one who asked
// (LF_PlayerMenuController answers on that player's own controller). Every value is read from the
// server's own state; the client sends nothing but the ask. Read only: nothing here changes a thing.

class LF_PlayerMenuServer
{
	static const string FEATURE = "PlayerMenu";

	//------------------------------------------------------------------------------------------------
	//! Server: the file of playerId. Missing parts (no account yet, no body) read as zero and log once.
	static LF_PlayerMenuData Build(int playerId)
	{
		LF_PlayerMenuData data = new LF_PlayerMenuData();
		string uid = LF_Utils.GetPlayerUID(playerId);
		string accountUid = LF_Utils.GetAccountUID(playerId);
		IEntity body;
		if (GetGame() && GetGame().GetPlayerManager())
			body = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);

		LF_PlayerAccount account;
		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		if (accounts)
			account = accounts.GetAccount(playerId);

		if (account)
		{
			LF_PlayerCharacter character = account.GetActiveCharacter();
			if (character)
				data.m_sName = character.GetFullName().Trim();

			data.m_eFaction = account.GetFaction();
			data.m_bOnDuty = account.IsOnDuty();
			data.m_iWanted = account.GetWantedLevel();
		}
		else
		{
			LF_Debug.Warn(FEATURE, "file for player " + playerId + " has no account yet: name and role blank");
		}

		if (data.m_sName.IsEmpty() && body)
			data.m_sName = LF_Utils.GetCharacterName(body);

		if (body)
			data.m_iCash = LF_MoneyUtils.GetCash(body);

		LF_ATMManager atm = LF_ATMManager.GetInstance();
		if (atm && !uid.IsEmpty())
		{
			LF_BankAccount bank = atm.GetAccount(uid);
			if (bank)
				data.m_iBank = bank.GetBalance();
		}

		data.m_iHouses = HousesOf(uid);
		if (!uid.IsEmpty())
		{
			data.m_iGarageUsed = LF_GarageSlots.UsedBy(uid);
			data.m_iGarageCap = LF_GarageSlots.CapOf(uid);
		}

		if (!accountUid.IsEmpty())
			data.m_iPlaytimeSeconds = LF_Playtime.Total(accountUid);

		AddLicences(data, uid, body);
		AddSkills(data, uid);
		AddQuests(data, body);
		return data;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Live houses whose owner key is uid.
	static int HousesOf(string uid)
	{
		if (uid.IsEmpty())
			return 0;

		int count = 0;
		foreach (string id, LF_HouseManagerComponent house : LF_HouseManagerComponent.Live())
		{
			if (house && house.GetOwnerKey() == uid)
				count++;
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! The name keys of the licences held: the body's live set, else the saved one. UNEMPLOYED is
	//! no licence a player reads.
	static void AddLicences(notnull LF_PlayerMenuData data, string uid, IEntity body)
	{
		LF_LicenseManagerComponent licences;
		if (body)
			licences = LF_LicenseManagerComponent.Cast(body.FindComponent(LF_LicenseManagerComponent));

		array<LF_ELicenseType> held = {};
		if (licences)
		{
			foreach (LF_ELicenseType type : LF_LicenseBits.Order())
			{
				if (licences.HasLicense(type))
					held.Insert(type);
			}
		}
		else if (!uid.IsEmpty())
		{
			LF_Progression.GetLicenses(uid, held);
		}

		foreach (LF_ELicenseType licence : held)
		{
			if (licence == LF_ELicenseType.UNEMPLOYED)
				continue;

			string key = LF_LicenceDesk.NameKey(licence);
			if (licences)
			{
				LF_LicenseConfig config = licences.GetLicenseConfig(licence);
				if (config && !config.m_sLicenseName.IsEmpty())
					key = config.m_sLicenseName;
			}

			data.m_aLicenceKeys.Insert(key);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! One row per job track of the jobs config, in config order.
	static void AddSkills(notnull LF_PlayerMenuData data, string uid)
	{
		LF_JobsConfig config = LF_JobsConfig.Get();
		if (!config || !config.m_aTracks)
		{
			LF_Debug.Warn(FEATURE, "no jobs config: the skills list is empty");
			return;
		}

		foreach (LF_JobTrackDef track : config.m_aTracks)
		{
			if (!track || track.m_sJob.IsEmpty())
				continue;

			LF_PlayerMenuSkill skill = SkillOf(track, LF_JobLevels.XpOf(uid, track.m_sJob));
			LF_PlayerMenuTrade.Fill(skill, uid);
			data.m_aSkills.Insert(skill);
		}
	}

	//------------------------------------------------------------------------------------------------
	static LF_PlayerMenuSkill SkillOf(notnull LF_JobTrackDef track, int xp)
	{
		LF_PlayerMenuSkill skill = new LF_PlayerMenuSkill();
		skill.m_sJob = track.m_sJob;
		skill.m_iXp = xp;
		int level;
		int floor;
		int next;
		LF_PlayerMenuXp.Span(track.m_aLevelThresholds, xp, level, floor, next);
		skill.m_iLevel = level;
		skill.m_iFloor = floor;
		skill.m_iNext = next;
		return skill;
	}

	//------------------------------------------------------------------------------------------------
	//! The quest rows from the body's tracker and the quest manager's definitions.
	static void AddQuests(notnull LF_PlayerMenuData data, IEntity body)
	{
		LF_QuestManager manager = LF_QuestManager.GetInstance();
		if (!manager || !manager.GetDefinitions())
			return;

		LF_QuestTrackerComponent trackerComponent;
		if (body)
			trackerComponent = LF_QuestTrackerComponent.Cast(body.FindComponent(LF_QuestTrackerComponent));

		LF_QuestTracker tracker;
		array<string> turnedIn = {};
		if (trackerComponent)
		{
			tracker = trackerComponent.GetTracker();
			turnedIn = trackerComponent.GetTurnedInQuestIds();
		}

		array<ref LF_PlayerMenuQuestInput> inputs = {};
		foreach (string questId, LF_QuestDefinition def : manager.GetDefinitions())
		{
			if (!def)
				continue;

			int status = -1;
			LF_QuestState state;
			if (tracker)
				state = tracker.GetState(questId);

			if (state)
				status = state.GetStatusValue();

			LF_PlayerMenuQuestInput input = LF_PlayerMenuQuestInput.Make(questId, def.m_sRequiredPreviousQuestId, !def.m_sTargetKind.IsEmpty(), status, turnedIn && turnedIn.Find(questId) != -1);
			input.m_sTitleKey = def.m_sTitleKey;
			input.m_sDescKey = def.m_sDescriptionKey;
			if (def.m_aObjectives)
			{
				foreach (int index, LF_QuestObjective objective : def.m_aObjectives)
				{
					if (!objective)
						continue;

					input.m_iTarget += objective.m_iTarget;
					if (state)
						input.m_iProgress += LF_Utils.MinInt(state.GetObjectiveProgress(index), objective.m_iTarget);
				}
			}

			inputs.Insert(input);
		}

		data.m_aQuests = LF_PlayerMenuQuests.Order(inputs);
	}
}
