// red-proof: observed 2026-11-19 (#3463) with `tools\cli test --no-build --only LF_Test_EveronAnniversaryPrep` before the string rows existed: failed `the chairs errand has its title in the string table` (and the description, and both flowers rows). `tools\cli test --no-build --red LF_Test_EveronAnniversaryPrep` printed RED PROOF OK (RedProof() skips the overlay lookups, so the event and both errands read as missing). Green after the strings (pack leg, worlds=LF_Everon1989_Test).
//! #3463 under the Everon 1989 header (the pack leg of `tools\cli test`): beat 1 of the anniversary arc (the
//! day before). Residents put chairs and flags out on the square (an event scene of AI civilians walking from the
//! town hall to the square) and the mayor's errand asks players for flowers and chairs. Both are pack data: the
//! event `anniversary` (phase `prepare`) comes from its own file in `Configs/Events/LF_Events_Everon1989/`, the
//! quests from its own file in `Configs/Quests/LF_QuestGiverTable_Everon1989/`. The mayor is a new NPC (a later
//! card), so the errands hang off the bible's giver id `mayor`. The test world holds no island, so no marker is
//! looked up.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronAnniversaryPrep : LF_Test
{
	static const ResourceName SCENE_CONF = "{91F005BDCEE0F056}Configs/Events/LF_Scene_AnniversaryPrep.conf";
	static const ResourceName HOURS_CONF = "{DE111BA7485F73E8}Configs/Events/LF_Condition_AnniversaryPrep.conf";
	static const ResourceName CHAIR_PREFAB = "{D77E0D49C10C35BB}Prefabs/Placeables/Furniture/Furniture_Chair_Item.et";
	static const ResourceName PLANT_PREFAB = "{1E56C8013F719C43}Prefabs/Placeables/Furniture/Furniture_Plant_Item.et";
	static const string EVENT_ID = "anniversary";
	static const string HOURS_MODULE = "anniversary_prepare_hours";
	static const string SCENE_MODULE = "anniversary_prepare_scene";
	static const string GIVER_ID = "mayor";
	static const string QUEST_CHAIRS = "anniversary_chairs";
	static const string QUEST_FLOWERS = "anniversary_flowers";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/anniversary-prep";
	}

	//------------------------------------------------------------------------------------------------
	protected static Managed Load(ResourceName path)
	{
		Resource resource = BaseContainerTools.LoadContainer(path);
		if (!resource || !resource.IsValid() || !resource.GetResource())
			return null;

		return BaseContainerTools.CreateInstanceFromContainer(resource.GetResource().ToBaseContainer());
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_QuestDefinition FindQuest(array<ref LF_QuestDefinition> quests, string questId)
	{
		if (!quests)
			return null;

		foreach (LF_QuestDefinition quest : quests)
		{
			if (quest && quest.m_sQuestId == questId)
				return quest;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! One collect errand: a real caption, a COLLECT_REPORT objective for a prefab that loads, and the amount asked.
	protected void CheckErrand(LF_TestContext ctx, LF_QuestDefinition quest, ResourceName item, int amount, string label)
	{
		ctx.NotNull(quest, label + " is offered by the mayor");
		if (!quest)
			return;

		string title = WidgetManager.Translate(quest.m_sTitleKey);
		string desc = WidgetManager.Translate(quest.m_sDescriptionKey);
		ctx.True(!quest.m_sTitleKey.IsEmpty() && title != quest.m_sTitleKey, label + " has its title in the string table");
		ctx.True(!quest.m_sDescriptionKey.IsEmpty() && desc != quest.m_sDescriptionKey, label + " has its description in the string table");
		ctx.True(quest.m_aObjectives && quest.m_aObjectives.Count() == 1, label + " has one objective");
		if (!quest.m_aObjectives || quest.m_aObjectives.Count() != 1)
			return;

		LF_QuestObjective objective = quest.m_aObjectives[0];
		ctx.True(objective.m_eType == LF_EQuestEventType.COLLECT_REPORT, label + " is a carry-it-in errand");
		ctx.Equal(amount, objective.m_iTarget, label + " asks for the right amount");
		ctx.EqualStr(item, objective.m_sFilter, label + " names its item by GUID and path");
		Resource itemResource = Resource.Load(objective.m_sFilter);
		ctx.True(itemResource && itemResource.IsValid(), label + " item prefab resolves");
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// The event, through the core overlay loader.
		LF_EventDef prep;
		if (!RedProof())
			prep = LF_EventOverlay.Find(EVENT_ID);

		ctx.NotNull(prep, "the anniversary event resolves through the overlay");
		if (prep)
		{
			ctx.True(prep.m_bEnabled, "the anniversary event is enabled");
			ctx.True(prep.m_aPhases && prep.m_aPhases.Count() >= 1, "the event has a phase");
			if (prep.m_aPhases && prep.m_aPhases.Count() >= 1)
			{
				ctx.EqualStr("prepare", prep.m_aPhases[0].m_sName, "the first phase is prepare");
				ctx.True(prep.m_aPhases[0].m_aModules && prep.m_aPhases[0].m_aModules.Find(HOURS_MODULE) != -1, "the prepare phase waits on the daytime window");
				ctx.True(prep.m_aPhases[0].m_aModules && prep.m_aPhases[0].m_aModules.Find(SCENE_MODULE) != -1, "the prepare phase runs the square scene");
			}
		}

		// Its daytime window: the dusk and dawn windows belong to other events.
		LF_EventPrecondition hours = LF_EventPrecondition.Cast(Load(HOURS_CONF));
		ctx.NotNull(hours, "the prepare window loads");
		if (hours)
		{
			ctx.True(hours.m_eKind == LF_EEventConditionKind.HOUR_RANGE, "the prepare window is an hour range");
			ctx.True(LF_EventPrecondition.HourInRange(11, hours.m_fFromHour, hours.m_fToHour), "11:00 is inside the window");
			ctx.False(LF_EventPrecondition.HourInRange(6, hours.m_fFromHour, hours.m_fToHour), "06:00 (the arrival and the shipment) is outside the window");
			ctx.False(LF_EventPrecondition.HourInRange(20, hours.m_fFromHour, hours.m_fToHour), "20:00 (the square evening) is outside the window");
		}

		// Its scene: residents walking out to the square.
		LF_EventScene scene = LF_EventScene.Cast(Load(SCENE_CONF));
		ctx.NotNull(scene, "the preparations scene loads");
		if (scene)
		{
			ctx.EqualStr("anniversary_prepare", scene.m_sSceneId, "the scene id is anniversary_prepare");
			ctx.EqualStr("", LF_EventScenePlanner.Validate(scene), "the planner accepts the scene");
			ctx.True(scene.m_aWaypointIds.Count() >= 2, "the route has waypoints from the town hall to the square");
			for (int i = 0; i < scene.m_aWaypointIds.Count(); i++)
				ctx.EqualStr(LF_Places.OrderedId(scene.m_sSceneId, i), scene.m_aWaypointIds[i], string.Format("waypoint %1 is the scene's placed marker %1, in order", i));

			array<string> seen = {};
			if (scene.m_aPrefabs)
			{
				foreach (ResourceName walker : scene.m_aPrefabs)
				{
					string key = walker;
					if (seen.Find(key) != -1)
						continue;

					seen.Insert(key);
					Resource walkerResource = Resource.Load(walker);
					ctx.True(walkerResource && walkerResource.IsValid(), string.Format("the walker prefab %1 resolves", key));
				}
			}

			ctx.True(seen.Count() >= 2, "the residents do not all wear the same outfit");

			LF_EventScenePlan plan = LF_EventScenePlanner.Plan(scene, 99);
			ctx.True(plan.m_bGo, "the planner makes a plan with room in the AI budget");
			ctx.Equal(scene.m_iNpcCount, plan.m_aNpcs.Count(), "the plan keeps every resident");
			LF_EventScenePlanner.ResetStatics();
		}

		// Its module refs register as the window and the scene the phase names.
		LF_EventsConfig events = LF_EventOverlay.Get();
		ctx.True(events && LF_EventOverlay.FindModule(events, HOURS_MODULE) != -1, "the overlay lists the window module");
		ctx.True(events && LF_EventOverlay.FindModule(events, SCENE_MODULE) != -1, "the overlay lists the scene module");
		if (events)
		{
			LF_EventModuleLoader.RegisterFrom(events);
			ctx.True(LF_EventShell.IsRegistered(HOURS_MODULE), "the window registers");
			ctx.True(LF_EventSceneOperation.Cast(LF_EventShell.FindOperation(SCENE_MODULE)) != null, "the square scene registers as a scene operation");
		}

		// The mayor's errands: flowers from the market and chairs from the hall.
		LF_QuestGiverTable.ResetStatics();
		array<ref LF_QuestDefinition> quests;
		if (!RedProof())
			quests = LF_QuestGiverTable.QuestsFor(GIVER_ID, null);

		CheckErrand(ctx, FindQuest(quests, QUEST_CHAIRS), CHAIR_PREFAB, 4, "the chairs errand");
		CheckErrand(ctx, FindQuest(quests, QUEST_FLOWERS), PLANT_PREFAB, 3, "the flowers errand");
		LF_QuestGiverTable.ResetStatics();
	}
}
