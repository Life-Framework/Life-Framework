//------------------------------------------------------------------------------------------------
//! The "What to do next" activities (player menu tab, maintainer 2026-11-22). An activity is a way to
//! earn money (a mining run, a parcel run, police duty) with a short guided starter: two to seven steps,
//! each a quest of the quest system, chained so the next starts on its own when the last is done.
//!
//! The core file (Configs/Activities/LF_Activities.conf) ships no rows: which activities a server
//! offers, what they pay and where their places are is a pack's decision. A pack overlays rows by
//! activity id (LF_ConfigOverlay: add, replace, patch, remove); a patch gives the activity whole again.
//! No coordinate lives here: a step's target is a kind and an id the world resolves (LF_ActivityTargets).

//------------------------------------------------------------------------------------------------
//! How a step is known to be done. NONE leaves it to the step's own quest objectives (a PROCESS, a
//! SIGNAL); every other check is read on the server every few seconds (LF_ActivityGuide), and a step
//! whose check already holds when it starts is passed at once (the job is held, the pickaxe carried).
enum LF_EActivityCheck
{
	NONE,		//!< The step's quest objectives
	JOB,		//!< The player holds the job m_sCheckParam (LF_PlayerJobComponent)
	ITEM,		//!< The player carries m_iCheckCount of the prefab m_sCheckParam
	VEHICLE,	//!< The player has an owned vehicle out; passed when they own none to take out
	ARRIVE,		//!< The player stands within m_fArriveRadius of the step's target
	DUTY,		//!< The player is on duty as m_sCheckParam (POLICE or MEDIC)
	XP,			//!< The player earned m_iCheckCount XP in the job m_sCheckParam since the step started
	LICENCE,	//!< The player holds the licence m_sCheckParam (LF_ELicenseType name)
	CREW,		//!< The player is a member of a crew (organization): founded or joined
	PERK,		//!< The player has picked a trade perk (the labour office counter)
	FLAG		//!< The player's progression holds the flag m_sCheckParam (e.g. truck_licence, set when the truck check is passed)
}

//------------------------------------------------------------------------------------------------
//! One step of an activity's starter. Its quest id is LF_ActivityTable.QuestIdOf(activity, step).
[BaseContainerProps()]
class LF_ActivityStep
{
	[Attribute("", UIWidgets.EditBox, "Stable step id inside its activity (part of the quest id, a persistence key)")]
	string m_sStepId;

	[Attribute("", UIWidgets.EditBox, "Localization key of the step's title")]
	string m_sTitleKey;

	[Attribute("", UIWidgets.EditBox, "Localization key of the one-sentence quest note shown when the step starts")]
	string m_sNoteKey;

	[Attribute("0", UIWidgets.ComboBox, "How the step is known to be done", "", ParamEnumArray.FromEnum(LF_EActivityCheck))]
	LF_EActivityCheck m_eCheck;

	[Attribute("", UIWidgets.EditBox, "JOB and XP: the job id; ITEM: the prefab ({GUID}path); DUTY: POLICE or MEDIC; LICENCE: the licence (LF_ELicenseType name); FLAG: the progression flag")]
	string m_sCheckParam;

	[Attribute("1", UIWidgets.EditBox, "ITEM: how many to carry; XP: how much XP to earn")]
	int m_iCheckCount;

	[Attribute("", UIWidgets.EditBox, "Where the waypoint goes: 'garage', 'job' (a workplace of the job named by the id), 'giver', or an LF_Places kind. Empty: no waypoint")]
	string m_sTargetKind;

	[Attribute("", UIWidgets.EditBox, "The target's id; empty means the nearest of the kind")]
	string m_sTargetId;

	[Attribute("", UIWidgets.EditBox, "Name on the waypoint and the map mark; empty uses the step title")]
	string m_sTargetLabelKey;

	[Attribute("30", UIWidgets.EditBox, "ARRIVE: how close counts as there, metres")]
	float m_fArriveRadius;

	[Attribute("", UIWidgets.Object, "NONE: the quest objectives of the step")]
	ref array<ref LF_QuestObjective> m_aObjectives;

	[Attribute("0", UIWidgets.EditBox, "Money paid when the step is done")]
	int m_iRewardMoney;

	[Attribute("0", UIWidgets.EditBox, "XP paid when the step is done, in the activity's job")]
	float m_fRewardXp;
}

//------------------------------------------------------------------------------------------------
//! One activity: what the tab prints and who may start it.
[BaseContainerProps()]
class LF_ActivityDef : LF_ConfigRecord
{
	[Attribute("", UIWidgets.EditBox, "Stable activity id (part of every step's quest id)")]
	string m_sId;

	[Attribute("", UIWidgets.EditBox, "Localization key of the name")]
	string m_sNameKey;

	[Attribute("", UIWidgets.EditBox, "Localization key of the one-line pitch")]
	string m_sPitchKey;

	[Attribute("0", UIWidgets.EditBox, "Rough pay an hour once running (tools\\cli economy), 0 for unknown")]
	int m_iPayPerHour;

	[Attribute("", UIWidgets.EditBox, "Where the activity happens, for the distance: same kinds as a step target")]
	string m_sPlaceKind;

	[Attribute("", UIWidgets.EditBox, "The place's id; empty means the nearest of the kind")]
	string m_sPlaceId;

	[Attribute("", UIWidgets.EditBox, "Job whose level m_iMinLevel reads and whose XP the steps pay (empty: CIVILIAN)")]
	string m_sJob;

	[Attribute("0", UIWidgets.EditBox, "Minimum level in m_sJob to see the activity (0 = none)")]
	int m_iMinLevel;

	[Attribute("", UIWidgets.EditBox, "Licence (LF_ELicenseType name) the player must hold to see it; empty = none")]
	string m_sLicence;

	[Attribute("", UIWidgets.EditBox, "Faction the player must be in (POLICE, MEDIC); empty = any")]
	string m_sFaction;

	[Attribute("", UIWidgets.EditBox, "Trade gate: a trade (job id) whose level m_iMinTradeLevel reads, or * for the best of any trade but CIVILIAN. Empty = no trade gate")]
	string m_sTrade;

	[Attribute("0", UIWidgets.EditBox, "Minimum level in m_sTrade to see the activity (0 = no trade gate)")]
	int m_iMinTradeLevel;

	[Attribute("", UIWidgets.EditBox, "Vehicle gate: owning a vehicle of this capacity class (LF_Capacity: car, pickup, van, truck) or a bigger one. With a trade gate either one opens the activity. Empty = none")]
	string m_sOwnsClass;

	[Attribute("0", UIWidgets.CheckBox, "Only after a criminal record: the player has completed at least one crime (LF_CrimeRecord)")]
	bool m_bNeedsRecord;

	[Attribute("", UIWidgets.Object, "The starter steps, in order")]
	ref array<ref LF_ActivityStep> m_aSteps;

	//------------------------------------------------------------------------------------------------
	static LF_ActivityDef Make(string id)
	{
		LF_ActivityDef def = new LF_ActivityDef();
		def.m_sId = id;
		def.m_aSteps = {};
		return def;
	}

	//------------------------------------------------------------------------------------------------
	//! The job the activity levels; CIVILIAN when none is named.
	string JobOrCivilian()
	{
		if (m_sJob.IsEmpty())
			return "CIVILIAN";

		return m_sJob;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a trade level or an owned vehicle opens the activity (the "work" gate).
	bool HasWorkGate()
	{
		return (!m_sTrade.IsEmpty() && m_iMinTradeLevel > 0) || !m_sOwnsClass.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	int StepCount()
	{
		if (!m_aSteps)
			return 0;

		return m_aSteps.Count();
	}
}

//------------------------------------------------------------------------------------------------
//! The table. Loaded from Configs/Activities/LF_Activities.conf; pack overlays layer over it.
[BaseContainerProps(configRoot: true)]
class LF_ActivityConfig
{
	static const ResourceName CONFIG_PATH = "{6A3C2E51B7D94F08}Configs/Activities/LF_Activities.conf";
	static const string OVERLAY_NAME = "activities";

	[Attribute("", UIWidgets.Object, "The activities, in the order the tab lists them")]
	ref array<ref LF_ActivityDef> m_aActivities;

	//------------------------------------------------------------------------------------------------
	LF_ActivityDef Find(string id)
	{
		if (!m_aActivities || id.IsEmpty())
			return null;

		foreach (LF_ActivityDef def : m_aActivities)
		{
			if (def && def.m_sId == id)
				return def;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Why an activity cannot stand (no id, no steps, a step with no id or a repeated one, a NONE step
	//! with no objective, a check with no parameter), or empty.
	static string Problem(notnull LF_ActivityDef def)
	{
		if (def.m_sId.IsEmpty())
			return "an activity has no id";

		if (def.StepCount() == 0)
			return string.Format("activity %1 has no steps", def.m_sId);

		array<string> seen = {};
		foreach (LF_ActivityStep step : def.m_aSteps)
		{
			if (!step || step.m_sStepId.IsEmpty())
				return string.Format("activity %1 has a step with no id", def.m_sId);

			if (seen.Find(step.m_sStepId) != -1)
				return string.Format("activity %1 repeats step %2", def.m_sId, step.m_sStepId);

			seen.Insert(step.m_sStepId);
			if (step.m_eCheck == LF_EActivityCheck.NONE && (!step.m_aObjectives || step.m_aObjectives.IsEmpty()))
				return string.Format("activity %1 step %2 has no check and no objective", def.m_sId, step.m_sStepId);

			bool needsParam = step.m_eCheck == LF_EActivityCheck.JOB || step.m_eCheck == LF_EActivityCheck.ITEM || step.m_eCheck == LF_EActivityCheck.DUTY || step.m_eCheck == LF_EActivityCheck.XP || step.m_eCheck == LF_EActivityCheck.LICENCE || step.m_eCheck == LF_EActivityCheck.FLAG;
			if (needsParam && step.m_sCheckParam.IsEmpty())
				return string.Format("activity %1 step %2 checks %3 of nothing", def.m_sId, step.m_sStepId, typename.EnumToString(LF_EActivityCheck, step.m_eCheck));

			if (step.m_eCheck == LF_EActivityCheck.ARRIVE && step.m_sTargetKind.IsEmpty())
				return string.Format("activity %1 step %2 asks to arrive with no target", def.m_sId, step.m_sStepId);
		}

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	void ApplyLayers(ResourceName coreConf = CONFIG_PATH)
	{
		array<ref Resource> resources = {};
		array<ResourceName> names = {};
		LF_ConfigOverlay.LoadPackOverlays(coreConf, LF_ActivityConfig, OVERLAY_NAME, resources, names);
		LF_ConfigClaims claims = new LF_ConfigClaims();
		foreach (int i, Resource resource : resources)
		{
			int changed = ApplyContainer(resource.GetResource().ToBaseContainer(), names[i], claims);
			LF_Debug.Info(LF_ActivityTable.FEATURE, string.Format("activities: pack overlay %1 changed %2 row(s)", names[i], changed));
		}
	}

	//------------------------------------------------------------------------------------------------
	int ApplyContainer(notnull BaseContainer container, string source = "pack", LF_ConfigClaims claims = null)
	{
		if (!m_aActivities)
			m_aActivities = new array<ref LF_ActivityDef>();

		return LF_ConfigOverlay.ApplyContainerList(new LF_ActivityList(this), container, "m_aActivities", source, claims);
	}
}

//------------------------------------------------------------------------------------------------
//! The activity list as the override model sees it. The id is the activity id.
class LF_ActivityList : LF_ConfigList
{
	protected LF_ActivityConfig m_Config;

	void LF_ActivityList(LF_ActivityConfig config)
	{
		m_Config = config;
	}

	override string GetName()
	{
		return "activities.activities";
	}

	override int Count()
	{
		return m_Config.m_aActivities.Count();
	}

	override LF_ConfigRecord Get(int index)
	{
		return m_Config.m_aActivities[index];
	}

	override string IdOf(notnull LF_ConfigRecord record)
	{
		return LF_ActivityDef.Cast(record).m_sId;
	}

	override string IdOfContainer(notnull BaseContainer container)
	{
		string id;
		container.Get("m_sId", id);
		return id;
	}

	override string IdHint()
	{
		return "an activity id";
	}

	override LF_ConfigRecord NewRecord(string id)
	{
		return LF_ActivityDef.Make(id);
	}

	override void Set(int index, notnull LF_ConfigRecord record)
	{
		LF_ActivityDef def = LF_ActivityDef.Cast(record);
		if (index < m_Config.m_aActivities.Count())
			m_Config.m_aActivities.Set(index, def);
		else
			m_Config.m_aActivities.Insert(def);
	}

	override void RemoveAt(int index)
	{
		m_Config.m_aActivities.RemoveOrdered(index);
	}

	//! A patch gives the activity whole again: every field the entry sets replaces the old one, and
	//! a step list replaces the old steps.
	override int PatchFromContainer(notnull LF_ConfigRecord record, notnull BaseContainer container)
	{
		LF_ActivityDef def = LF_ActivityDef.Cast(record);
		LF_ActivityDef given = LF_ActivityDef.Cast(BaseContainerTools.CreateInstanceFromContainer(container));
		if (!given)
			return 0;

		int changed = 0;
		array<string> strings = {"m_sNameKey", "m_sPitchKey", "m_sPlaceKind", "m_sPlaceId", "m_sJob", "m_sLicence", "m_sFaction", "m_sTrade", "m_sOwnsClass"};
		foreach (string field : strings)
		{
			if (!container.IsVariableSetDirectly(field))
				continue;

			string value;
			container.Get(field, value);
			SetString(def, field, value);
			changed++;
		}

		if (container.IsVariableSetDirectly("m_iPayPerHour"))
		{
			def.m_iPayPerHour = given.m_iPayPerHour;
			changed++;
		}

		if (container.IsVariableSetDirectly("m_iMinLevel"))
		{
			def.m_iMinLevel = given.m_iMinLevel;
			changed++;
		}

		if (container.IsVariableSetDirectly("m_iMinTradeLevel"))
		{
			def.m_iMinTradeLevel = given.m_iMinTradeLevel;
			changed++;
		}

		if (container.IsVariableSetDirectly("m_bNeedsRecord"))
		{
			def.m_bNeedsRecord = given.m_bNeedsRecord;
			changed++;
		}

		if (container.IsVariableSetDirectly("m_aSteps"))
		{
			def.m_aSteps = given.m_aSteps;
			changed++;
		}

		return changed;
	}

	//------------------------------------------------------------------------------------------------
	protected void SetString(notnull LF_ActivityDef def, string field, string value)
	{
		if (field == "m_sNameKey")
			def.m_sNameKey = value;
		else if (field == "m_sPitchKey")
			def.m_sPitchKey = value;
		else if (field == "m_sPlaceKind")
			def.m_sPlaceKind = value;
		else if (field == "m_sPlaceId")
			def.m_sPlaceId = value;
		else if (field == "m_sJob")
			def.m_sJob = value;
		else if (field == "m_sLicence")
			def.m_sLicence = value;
		else if (field == "m_sFaction")
			def.m_sFaction = value;
		else if (field == "m_sTrade")
			def.m_sTrade = value;
		else if (field == "m_sOwnsClass")
			def.m_sOwnsClass = value;
	}

	override string Incomplete(notnull LF_ConfigRecord record)
	{
		return LF_ActivityConfig.Problem(LF_ActivityDef.Cast(record));
	}
}

//------------------------------------------------------------------------------------------------
//! The loaded table: core file, then pack overlays. Each activity's steps become quest definitions,
//! registered with the quest manager when the world starts (RegisterQuests): step 1 is started only by
//! the player's "Start" (LF_ActivityGuide.Start), every later step needs the one before turned in and
//! starts on its own, and every step turns itself in where the player stands once done.
class LF_ActivityTable
{
	static const string FEATURE = "Activities";
	static const string QUEST_PREFIX = "activity_";
	//! The SIGNAL a checked step's quest waits on: "activity.step:<questId>".
	static const string SIGNAL_PREFIX = "activity.step:";

	protected static ref LF_ActivityConfig s_Config;
	protected static bool s_bLoadAttempted;
	//! Tests: a table built in script in place of the file.
	protected static ref LF_ActivityConfig s_TestConfig;

	//------------------------------------------------------------------------------------------------
	//! The loaded table, or null (logged once) when the file does not resolve.
	static LF_ActivityConfig Get()
	{
		if (s_TestConfig)
			return s_TestConfig;

		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(LF_ActivityConfig.CONFIG_PATH);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error(FEATURE, "activity table does not resolve: " + LF_ActivityConfig.CONFIG_PATH);
			return null;
		}

		s_Config = LF_ActivityConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!s_Config)
		{
			LF_Debug.Error(FEATURE, "activity table has an unexpected root class: " + LF_ActivityConfig.CONFIG_PATH);
			return null;
		}

		if (!s_Config.m_aActivities)
			s_Config.m_aActivities = new array<ref LF_ActivityDef>();

		s_Config.ApplyLayers();
		Prune(s_Config);
		LF_Debug.Info(FEATURE, string.Format("activity table loaded: %1 activities", s_Config.m_aActivities.Count()));
		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: use this table (null goes back to the file).
	static void SetForTests(LF_ActivityConfig config)
	{
		s_TestConfig = config;
		if (config && !config.m_aActivities)
			config.m_aActivities = new array<ref LF_ActivityDef>();
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the activities that cannot stand, logged.
	static int Prune(notnull LF_ActivityConfig config)
	{
		int dropped = 0;
		for (int i = config.m_aActivities.Count() - 1; i >= 0; i--)
		{
			LF_ActivityDef def = config.m_aActivities[i];
			string problem = "an empty row";
			if (def)
				problem = LF_ActivityConfig.Problem(def);

			if (problem.IsEmpty())
				continue;

			LF_Debug.Error(FEATURE, "activity refused: " + problem);
			config.m_aActivities.RemoveOrdered(i);
			dropped++;
		}

		return dropped;
	}

	//------------------------------------------------------------------------------------------------
	static LF_ActivityDef Find(string id)
	{
		LF_ActivityConfig config = Get();
		if (!config)
			return null;

		return config.Find(id);
	}

	//------------------------------------------------------------------------------------------------
	static string QuestIdOf(string activityId, string stepId)
	{
		return QUEST_PREFIX + activityId + "_" + stepId;
	}

	//------------------------------------------------------------------------------------------------
	static string SignalOf(string questId)
	{
		return SIGNAL_PREFIX + questId;
	}

	//------------------------------------------------------------------------------------------------
	//! The activity and step a quest id belongs to. \return false when it is no activity step.
	static bool StepOf(string questId, out LF_ActivityDef activity, out int stepIndex)
	{
		activity = null;
		stepIndex = -1;
		if (!questId.StartsWith(QUEST_PREFIX))
			return false;

		LF_ActivityConfig config = Get();
		if (!config)
			return false;

		foreach (LF_ActivityDef def : config.m_aActivities)
		{
			if (!def)
				continue;

			foreach (int i, LF_ActivityStep step : def.m_aSteps)
			{
				if (step && QuestIdOf(def.m_sId, step.m_sStepId) == questId)
				{
					activity = def;
					stepIndex = i;
					return true;
				}
			}
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! The quest of one step: chained to the step before, starting and finishing on its own (except the
	//! first, which only "Start" begins), one-off. A checked step waits on its own SIGNAL.
	static LF_QuestDefinition BuildQuest(notnull LF_ActivityDef activity, int index)
	{
		LF_ActivityStep step = activity.m_aSteps[index];
		LF_QuestDefinition quest = new LF_QuestDefinition();
		quest.m_sQuestId = QuestIdOf(activity.m_sId, step.m_sStepId);
		quest.m_sTitleKey = step.m_sTitleKey;
		quest.m_sDescriptionKey = step.m_sNoteKey;
		quest.m_sJob = activity.JobOrCivilian();
		quest.m_iRewardMoney = step.m_iRewardMoney;
		quest.m_fRewardXp = step.m_fRewardXp;
		quest.m_bAutoTurnIn = true;
		quest.m_aRewardItems = {};
		quest.m_aExclusiveQuestIds = {};
		quest.m_aTurnInAwards = {};
		if (index > 0)
		{
			quest.m_sRequiredPreviousQuestId = QuestIdOf(activity.m_sId, activity.m_aSteps[index - 1].m_sStepId);
			quest.m_bAutoAccept = true;
		}

		quest.m_aObjectives = {};
		if (step.m_eCheck == LF_EActivityCheck.NONE)
		{
			foreach (LF_QuestObjective objective : step.m_aObjectives)
			{
				if (objective)
					quest.m_aObjectives.Insert(objective);
			}
		}
		else
		{
			LF_QuestObjective signal = new LF_QuestObjective();
			signal.m_eType = LF_EQuestEventType.SIGNAL;
			signal.m_iTarget = 1;
			signal.m_eMode = LF_EQuestProgressMode.ADD;
			signal.m_sFilter = SignalOf(quest.m_sQuestId);
			quest.m_aObjectives.Insert(signal);
		}

		return quest;
	}

	//------------------------------------------------------------------------------------------------
	//! Server, at world start: every step of every activity is a quest the quest manager knows. A step
	//! whose quest id is already taken (by a giver's quest) is refused, logged. \return Quests registered.
	static int RegisterQuests()
	{
		LF_ActivityConfig config = Get();
		LF_QuestManager manager = LF_QuestManager.GetInstance();
		if (!config || !manager)
		{
			LF_Debug.Warn(FEATURE, "activity quests not registered: no table or no quest manager");
			return 0;
		}

		int registered = 0;
		foreach (LF_ActivityDef activity : config.m_aActivities)
		{
			for (int i = 0; i < activity.StepCount(); i++)
			{
				LF_QuestDefinition quest = BuildQuest(activity, i);
				LF_QuestDefinition existing = manager.GetDefinition(quest.m_sQuestId);
				if (existing && !existing.m_sQuestId.StartsWith(QUEST_PREFIX))
				{
					LF_Debug.Error(FEATURE, "activity step refused: quest id already used: " + quest.m_sQuestId);
					continue;
				}

				manager.RegisterDefinition(quest);
				registered++;
			}
		}

		LF_Debug.Info(FEATURE, string.Format("activity quests registered: %1", registered));
		return registered;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: registers the activity quests unless the quest manager already knows them (a world
	//! start, or the quest manager was reset since). \return Whether the quests are known now.
	static bool EnsureRegistered()
	{
		LF_ActivityConfig config = Get();
		LF_QuestManager manager = LF_QuestManager.GetInstance();
		if (!config || !manager)
			return false;

		bool missing = false;
		foreach (LF_ActivityDef activity : config.m_aActivities)
		{
			if (activity.StepCount() > 0 && !manager.GetDefinition(QuestIdOf(activity.m_sId, activity.m_aSteps[0].m_sStepId)))
			{
				missing = true;
				break;
			}
		}

		if (missing)
			RegisterQuests();

		return true;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_Config = null;
		s_bLoadAttempted = false;
		s_TestConfig = null;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_ActivityTable's statics (LF_StaticReset finds it by reflection).
class LF_ActivityTable_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_ActivityTable";
	}

	override void Apply()
	{
		LF_ActivityTable.ResetStatics();
	}
}
