// red-proof: observed 2026-11-19 (#3464) with `tools\cli test --no-build --only LF_Test_EveronAnniversaryMemorial` before the data files existed: `[LFTEST] FAIL everon/anniversary-memorial` (the event had one phase, no windows, scenes or award row). Green after the files (pack leg, worlds=LF_Everon1989_Test). RedProof() skips the event and award lookups.
//! #3464 under the Everon 1989 header (the pack leg of `tools\cli test`): beats 2 and 4 of the anniversary arc.
//! At noon the residents walk from the church to the memorial (event phase `memorial`), then the town gathers
//! at the cafe (phase `gather`). Both are data patched onto the `anniversary` event beat 1 (#3463) added, from
//! their own file in `Configs/Events/LF_Events_Everon1989/`; the day's standing award is its own file in
//! `Configs/Reputation/LF_Reputation_Everon1989/`. The event is once per game year (a game day is 7200 s).
//! The test world holds no island, so no marker is looked up.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronAnniversaryMemorial : LF_Test
{
	static const ResourceName MEMORIAL_HOURS_CONF = "{7C3583A28EC897D0}Configs/Events/LF_Condition_AnniversaryMemorial.conf";
	static const ResourceName MEMORIAL_SCENE_CONF = "{0F6E04B0DFACEE84}Configs/Events/LF_Scene_AnniversaryMemorial.conf";
	static const ResourceName GATHER_HOURS_CONF = "{EF11EE5E405393FD}Configs/Events/LF_Condition_AnniversaryGather.conf";
	static const ResourceName GATHER_SCENE_CONF = "{5B6448D007777542}Configs/Events/LF_Scene_AnniversaryGather.conf";
	static const ResourceName RITE_CONF = "{0198079B059A452B}Configs/Events/LF_Rite_AnniversaryMemorial.conf";
	static const string EVENT_ID = "anniversary";
	static const string AWARD_ACTION = "anniversary_memorial";
	static const float GAME_DAY_SECONDS = 7200;
	static const int GAME_YEAR_DAYS = 365;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/anniversary-memorial";
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
	//! One walking scene: loads, the planner accepts it, the route is its placed markers in order, every walker
	//! prefab resolves and the residents do not all dress alike.
	protected void CheckScene(LF_TestContext ctx, ResourceName path, string sceneId, string label)
	{
		LF_EventScene scene = LF_EventScene.Cast(Load(path));
		ctx.NotNull(scene, label + " scene loads");
		if (!scene)
			return;

		ctx.EqualStr(sceneId, scene.m_sSceneId, label + " scene id");
		ctx.EqualStr("", LF_EventScenePlanner.Validate(scene), label + " scene is accepted by the planner");
		ctx.True(scene.m_aWaypointIds.Count() >= 2, label + " route has at least two waypoints");
		for (int i = 0; i < scene.m_aWaypointIds.Count(); i++)
			ctx.EqualStr(LF_Places.OrderedId(sceneId, i), scene.m_aWaypointIds[i], string.Format("%1 waypoint %2 is the scene's placed marker %2, in order", label, i));

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
				ctx.True(walkerResource && walkerResource.IsValid(), string.Format("%1 walker prefab %2 resolves", label, key));
			}
		}

		ctx.True(seen.Count() >= 2, label + " residents do not all wear the same outfit");
		LF_EventScenePlan plan = LF_EventScenePlanner.Plan(scene, 99);
		ctx.True(plan.m_bGo, label + " planner makes a plan with room in the AI budget");
		ctx.Equal(scene.m_iNpcCount, plan.m_aNpcs.Count(), label + " plan keeps every resident");
		LF_EventScenePlanner.ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	//! One daytime window: an hour range that holds at `inside` and not at the hours of the other events.
	protected void CheckWindow(LF_TestContext ctx, ResourceName path, float inside, string label)
	{
		LF_EventPrecondition hours = LF_EventPrecondition.Cast(Load(path));
		ctx.NotNull(hours, label + " window loads");
		if (!hours)
			return;

		ctx.True(hours.m_eKind == LF_EEventConditionKind.HOUR_RANGE, label + " window is an hour range");
		ctx.True(LF_EventPrecondition.HourInRange(inside, hours.m_fFromHour, hours.m_fToHour), string.Format("%1 window holds at %2:00", label, inside));
		ctx.False(LF_EventPrecondition.HourInRange(6, hours.m_fFromHour, hours.m_fToHour), label + " window is closed at 06:00 (the arrival and the shipment)");
		ctx.False(LF_EventPrecondition.HourInRange(20, hours.m_fFromHour, hours.m_fToHour), label + " window is closed at 20:00 (the square evening)");
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// The event, through the core overlay loader: beat 1's prepare first, then the memorial and the gathering.
		LF_EventDef anniversary;
		if (!RedProof())
			anniversary = LF_EventOverlay.Find(EVENT_ID);

		ctx.NotNull(anniversary, "the anniversary event resolves through the overlay");
		LF_EventsConfig events = LF_EventOverlay.Get();
		if (anniversary)
		{
			ctx.True(anniversary.m_bEnabled, "the anniversary event is enabled");
			ctx.Equal(GAME_DAY_SECONDS * GAME_YEAR_DAYS, anniversary.m_fCooldownSeconds, "the cooldown is a game year");
			ctx.Equal(3, anniversary.m_aPhases.Count(), "the event has three phases");
			ctx.EqualStr("prepare", anniversary.m_aPhases[0].m_sName, "beat 1's prepare phase is still first");
			int memorialAt = anniversary.FindPhase("memorial");
			int gatherAt = anniversary.FindPhase("gather");
			ctx.Equal(1, memorialAt, "the memorial is the second phase");
			ctx.Equal(2, gatherAt, "the gathering is the third phase");

			if (memorialAt != -1)
			{
				LF_EventPhase memorial = anniversary.m_aPhases[memorialAt];
				ctx.True(memorial.m_aModules && memorial.m_aModules.Find("anniversary_memorial_hours") != -1, "the memorial phase waits on its noon window");
				ctx.True(memorial.m_aModules && memorial.m_aModules.Find("anniversary_memorial_scene") != -1, "the memorial phase runs the walk to the memorial");
				ctx.True(memorial.m_aModules && memorial.m_aModules.Find("anniversary_memorial_rite") != -1, "the memorial phase runs the rite (bells, radios, the priest's line, the standing)");
			}

			if (gatherAt != -1)
			{
				LF_EventPhase gather = anniversary.m_aPhases[gatherAt];
				ctx.True(gather.m_aModules && gather.m_aModules.Find("anniversary_gather_hours") != -1, "the gather phase waits on its afternoon window");
				ctx.True(gather.m_aModules && gather.m_aModules.Find("anniversary_gather_scene") != -1, "the gather phase runs the walk to the cafe");
			}
		}

		// Windows and scenes: noon at the memorial, the afternoon at the cafe.
		CheckWindow(ctx, MEMORIAL_HOURS_CONF, 12, "the memorial");
		CheckWindow(ctx, GATHER_HOURS_CONF, 13, "the gathering");
		CheckScene(ctx, MEMORIAL_SCENE_CONF, "anniversary_memorial", "the memorial");
		CheckScene(ctx, GATHER_SCENE_CONF, "anniversary_gather", "the gathering");

		// Every module the new phases name registers through the loader.
		array<string> modules = {"anniversary_memorial_hours", "anniversary_memorial_scene", "anniversary_memorial_rite", "anniversary_gather_hours", "anniversary_gather_scene"};
		foreach (string moduleId : modules)
		{
			ctx.True(events && LF_EventOverlay.FindModule(events, moduleId) != -1, "the overlay lists module " + moduleId);
		}

		if (events)
		{
			LF_EventModuleLoader.RegisterFrom(events);
			ctx.True(LF_EventShell.IsRegistered("anniversary_memorial_hours"), "the memorial window registers");
			ctx.True(LF_EventShell.IsRegistered("anniversary_gather_hours"), "the gathering window registers");
			ctx.True(LF_EventSceneOperation.Cast(LF_EventShell.FindOperation("anniversary_memorial_scene")) != null, "the memorial walk registers as a scene operation");
			ctx.True(LF_EventSceneOperation.Cast(LF_EventShell.FindOperation("anniversary_gather_scene")) != null, "the gathering walk registers as a scene operation");
			ctx.True(LF_EventMemorialOperation.Cast(LF_EventShell.FindOperation("anniversary_memorial_rite")) != null, "the memorial rite registers as a memorial operation");
		}

		// The day's standing award: a little town standing, with a cooldown so a player earns it once a day.
		LF_ReputationConfig.Reload();
		LF_ReputationAwardDef award;
		if (!RedProof())
		{
			LF_ReputationConfig standing = LF_ReputationConfig.Get();
			if (standing)
				award = standing.FindAward(AWARD_ACTION);
		}

		ctx.NotNull(award, "the memorial's standing award row loads");
		if (award)
		{
			ctx.EqualStr("TOWN", award.m_sFaction, "the memorial raises town standing");
			ctx.True(award.m_iPoints > 0 && award.m_iPoints <= 25, "it is a small gain");
			ctx.True(award.m_fCooldownSeconds > 0, "the award has a cooldown, so it is earned once");
		}

		CheckRite(ctx);
		LF_ReputationConfig.Reload();
	}

	//------------------------------------------------------------------------------------------------
	//! The rite's data (#3806): it loads, is usable, and names the memorial's scene, the day's award row,
	//! radios whose sound slots the pack fills, and texts the pack's string table holds.
	protected void CheckRite(LF_TestContext ctx)
	{
		LF_EventMemorial rite = LF_EventMemorial.Cast(Load(RITE_CONF));
		ctx.NotNull(rite, "the rite config loads");
		if (!rite)
			return;

		ctx.EqualStr("", rite.Validate(), "the rite is usable");
		ctx.EqualStr("anniversary_memorial", rite.m_sSceneId, "the rite is held at the memorial scene's target");
		ctx.EqualStr(AWARD_ACTION, rite.m_sAwardAction, "the rite gives the day's standing award");
		ctx.True(rite.m_aRadioSlots && rite.m_aRadioSlots.Count() >= 3, "the rite quiets the town's radios");
		ctx.True(rite.m_iBellStrokes > 0, "the church bells ring");
		ctx.True(rite.m_sSpeakerName.StartsWith("#") && rite.m_sSpeakerLine.StartsWith("#"), "the priest's name and line are localisation keys");
		ctx.True(rite.m_sToastTitle.StartsWith("#") && rite.m_sToastText.StartsWith("#"), "the toast is localisation keys");

		// The priest's recorded speech (#3827): the rite names his dialogue conf and its voiced line, which
		// has an event, a length and the audio project that holds it.
		ctx.False(rite.m_sVoiceDialogue.IsEmpty() || rite.m_sVoiceLine.IsEmpty(), "the rite names the priest's recorded line");
		LF_DialogueConfig priest = LF_DialogueConfig.Load(rite.m_sVoiceDialogue);
		ctx.NotNull(priest, "the priest's dialogue conf loads");
		if (!priest)
			return;

		LF_DialogueLine speech = LF_DialogueFlow.FindLine(priest, rite.m_sVoiceLine);
		ctx.NotNull(speech, "the priest's dialogue holds the rite's voiced line");
		ctx.False(priest.m_sSoundProject.IsEmpty(), "the priest's dialogue names its audio project");
		if (!speech)
			return;

		ctx.False(speech.IsTextOnly(), "the priest's line is recorded");
		ctx.EqualStr("SOUND_DLG_ANNIVERSARY_PRIEST_MEMORIAL", speech.m_sEvent, "the clip is the priest's memorial speech");
		ctx.True(speech.m_fSeconds > 1, "the clip has a length the rite can wait for");
	}
}
