// red-proof: remove the giver from Quests.layer or change the talk route so the
// intro_talk state never reaches COMPLETED; the registration or lifecycle
// assertions go red. Run `tools\cli test --tier all`.

// tier: WORLD
class LF_Test_QuestWorld : EL_Test
{
	override string GetName()
	{
		return "quests/world-seam";
	}

	override void Run(EL_TestContext ctx)
	{
		LF_QuestManager manager = LF_QuestManager.GetInstance();
		ctx.NotNull(manager, "quest manager exists");

		// The giver in Quests.layer must have registered its definitions on init.
		ctx.NotNull(manager.GetDefinition("intro_talk"), "intro_talk registered by the giver");
		ctx.NotNull(manager.GetDefinition("intro_collect"), "intro_collect registered");
		ctx.NotNull(manager.GetDefinition("intro_gather"), "intro_gather registered");
		ctx.NotNull(manager.GetDefinition("intro_area"), "intro_area registered");
		ctx.NotNull(manager.GetDefinition("intro_kill"), "intro_kill registered");
		ctx.NotNull(manager.GetDefinition("intro_timed"), "intro_timed registered");

		Resource characterRes = Resource.Load("{9B5BB216CC7FF18E}Prefabs/Characters/Core/Character_Roleplay.et");
		ctx.True(characterRes != null && characterRes.IsValid(), "character prefab loads");

		EntitySpawnParams spawnParams();
		spawnParams.TransformMode = ETransformMode.WORLD;
		spawnParams.Transform[3] = "100 0 100";

		IEntity character = GetGame().SpawnEntityPrefab(characterRes, GetGame().GetWorld(), spawnParams);
		ctx.NotNull(character, "test character spawns");
		if (!character)
			return;

		LF_QuestTrackerComponent tracker = LF_QuestTrackerComponent.Cast(character.FindComponent(LF_QuestTrackerComponent));
		ctx.NotNull(tracker, "test character carries the quest tracker");
		if (!tracker)
			return;

		// Talk quest: accept, interact with the giver, turn in, reward items land.
		tracker.AcceptQuest("intro_talk");
		ctx.True(tracker.GetTracker().GetStatus("intro_talk") == LF_EQuestStatus.ACTIVE, "talk quest accepted");
		manager.ReportTalk(character, "Start");
		ctx.True(tracker.GetTracker().GetStatus("intro_talk") == LF_EQuestStatus.COMPLETED, "talk quest completes on giver interaction");
		tracker.TurnInQuest("intro_talk");
		ctx.True(tracker.GetTracker().GetStatus("intro_talk") == LF_EQuestStatus.TURNED_IN, "talk quest turned in");
		EL_PlayerLevelComponent levelComp = EL_Component<EL_PlayerLevelComponent>.Find(character);
		if (levelComp)
			ctx.True(levelComp.GetPlayerExperience() >= 10, "turn-in granted reward XP");

		// Gather quest: three gather reports complete it.
		tracker.AcceptQuest("intro_gather");
		manager.ReportGather(character, "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et");
		manager.ReportGather(character, "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et");
		manager.ReportGather(character, "{A231A5F8D479B5DC}Prefabs/Items/Drinks/WaterBottle.et");
		ctx.True(tracker.GetTracker().GetStatus("intro_gather") == LF_EQuestStatus.COMPLETED, "gather quest completes after 3 gathers");

		// Kill quest: the killer's quest counts the victim's faction.
		tracker.AcceptQuest("intro_kill");
		manager.ReportKill(character, character);
		manager.ReportKill(character, character);
		ctx.True(tracker.GetTracker().GetStatus("intro_kill") == LF_EQuestStatus.COMPLETED, "kill quest completes after 2 kills");

		// Area quest: routing an area report completes it (the frame-tick
		// position check is not controllable in a headless run).
		tracker.AcceptQuest("intro_area");
		manager.ReportArea(character, "Area_Start");
		ctx.True(tracker.GetTracker().GetStatus("intro_area") == LF_EQuestStatus.COMPLETED, "area quest completes on area entry");
	}
};
