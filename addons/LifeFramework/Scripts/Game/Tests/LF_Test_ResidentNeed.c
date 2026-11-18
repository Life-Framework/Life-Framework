// red-proof: 2026-11-18, `cli test --no-build --red LF_Test_ResidentNeed`: RedProof() skips the Attach call and the
//   crime that makes the resident refuse, so the resident offers nothing, the turn-in is not remembered and the
//   refused player is still handed the errand; the first failing check is `the resident offers their need quest`.
//! residents/need: a resident with `m_sNeedQuest` becomes a quest giver offering that quest (#2862); turning it
//! in through the quest tracker applies `helped` to that resident's memory of the player; a resident whose
//! memory of the player is `refuses` hands that player no errand; an unknown quest id leaves no offer.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_ResidentNeed : LF_Test
{
	protected static const string RESIDENT = "test_need_baker";
	protected static const string QUEST = "test_need_talk";
	protected static const string UID_HELPER = "test-need-helper";
	protected static const string UID_REFUSED = "test-need-refused";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "residents/need";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_ResidentNeedQuestsConfig MakeConfig()
	{
		LF_QuestObjective objective = new LF_QuestObjective();
		objective.m_eType = LF_EQuestEventType.TALK;
		objective.m_sFilter = "resident_" + RESIDENT;
		objective.m_iTarget = 1;

		LF_QuestDefinition quest = new LF_QuestDefinition();
		quest.m_sQuestId = QUEST;
		quest.m_aObjectives = {};
		quest.m_aObjectives.Insert(objective);

		LF_ResidentNeedQuestsConfig config = new LF_ResidentNeedQuestsConfig();
		config.m_aQuests = {};
		config.m_aQuests.Insert(quest);
		return config;
	}

	//------------------------------------------------------------------------------------------------
	protected LF_QuestGiverComponent SpawnResident(LF_TestContext ctx, vector at)
	{
		IEntity npc = LF_Utils.SpawnEntityPrefab(LF_ResidentNeeds.GIVER_PREFAB, at, "0 0 0", false);
		ctx.NotNull(npc, "the needy resident prefab spawns");
		if (!npc)
			return null;

		LF_QuestGiverComponent giver = LF_QuestGiverComponent.Cast(npc.FindComponent(LF_QuestGiverComponent));
		ctx.NotNull(giver, "the needy resident prefab carries a quest giver");
		return giver;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ResidentMemory.ResetStatics();
		LF_ResidentNeeds.ResetStatics();
		LF_ResidentNeeds.UseForTest(MakeConfig());

		LF_ResidentDef def = LF_ResidentDef.Make(RESIDENT, "", "");
		def.m_sNeedQuest = QUEST;

		LF_QuestGiverComponent giver = SpawnResident(ctx, "100 0 100");
		IEntity helper = LF_ExploitGuardsWorldFixture.SpawnCharacter(ctx, "104 0 100", true);
		IEntity refused = LF_ExploitGuardsWorldFixture.SpawnCharacter(ctx, "108 0 100", true);
		if (giver && helper && refused)
			RunNeed(ctx, giver, def, helper, refused);

		SCR_EntityHelper.DeleteEntityAndChildren(helper);
		SCR_EntityHelper.DeleteEntityAndChildren(refused);
		if (giver)
			SCR_EntityHelper.DeleteEntityAndChildren(giver.GetOwner());

		RunUnknown(ctx);

		LF_ResidentMemory.ResetStatics();
		LF_ResidentNeeds.ResetStatics();
		LF_Progression.Forget(UID_HELPER);
		LF_Progression.Forget(UID_REFUSED);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunNeed(LF_TestContext ctx, LF_QuestGiverComponent giver, LF_ResidentDef def, IEntity helper, IEntity refused)
	{
		if (!RedProof())
			ctx.True(LF_ResidentNeeds.Attach(giver.GetOwner(), def), "a resident with a need attaches it");

		ctx.Equal(1, giver.GetQuests().Count(), "the resident offers their need quest");
		if (giver.GetQuests().Count() == 1)
			ctx.EqualStr(QUEST, giver.GetQuests()[0].m_sQuestId, "the offer is the need quest");
		ctx.EqualStr("resident_" + RESIDENT, giver.GetGiverId(), "the giver is named for the resident");
		ctx.EqualStr("resident_" + RESIDENT, LF_QuestManager.GetInstance().GetGiverOfQuest(QUEST), "the quest is registered against the resident's giver");
		ctx.EqualStr(RESIDENT, LF_ResidentNeeds.ResidentOf(QUEST), "the quest maps back to the resident who asked");

		// Help is remembered.
		LF_QuestTrackerComponent tracker = LF_Component<LF_QuestTrackerComponent>.Find(helper);
		ctx.NotNull(tracker, "the helper carries the quest tracker");
		if (tracker)
		{
			tracker.SetUidForTests(UID_HELPER);
			ctx.True(tracker.AcceptQuest(QUEST), "a stranger is handed the errand");
			LF_QuestManager.GetInstance().ReportTalk(helper, "resident_" + RESIDENT);
			ctx.True(tracker.TurnInQuest(QUEST), "the errand is turned in");
			ctx.Equal(10, LF_ResidentMemory.Score(RESIDENT, UID_HELPER), "helping raises the resident's memory of the player by the helped delta");
		}

		// A resident who refuses the player hands them nothing.
		if (!RedProof())
		{
			LF_ResidentMemory.Apply(RESIDENT, UID_REFUSED, "crime_seen", 0);
			LF_ResidentMemory.Apply(RESIDENT, UID_REFUSED, "crime_seen", 1000);
			LF_ResidentMemory.Apply(RESIDENT, UID_REFUSED, "crime_seen", 2000);
		}

		ctx.EqualStr(LF_ResidentMemory.TIER_REFUSES, LF_ResidentMemory.Tier(RESIDENT, UID_REFUSED), "the resident refuses this player");
		LF_QuestTrackerComponent other = LF_Component<LF_QuestTrackerComponent>.Find(refused);
		ctx.NotNull(other, "the refused player carries the quest tracker");
		if (other)
		{
			other.SetUidForTests(UID_REFUSED);
			ctx.False(other.AcceptQuest(QUEST), "a resident who refuses the player hands them no errand");
			ctx.True(other.GetTracker().GetStatus(QUEST) != LF_EQuestStatus.ACTIVE, "the refused player has no active need quest");
		}
	}

	//------------------------------------------------------------------------------------------------
	//! A need quest id the config does not know: logged, and the resident spawns with no offer.
	protected void RunUnknown(LF_TestContext ctx)
	{
		LF_QuestGiverComponent giver = SpawnResident(ctx, "112 0 100");
		if (!giver)
			return;

		LF_ResidentDef def = LF_ResidentDef.Make("test_need_unknown", "", "");
		def.m_sNeedQuest = "no_such_need_quest";
		ctx.False(LF_ResidentNeeds.Attach(giver.GetOwner(), def), "an unknown need quest is not attached");
		ctx.Equal(0, giver.GetQuests().Count(), "the resident with an unknown need offers nothing");
		ctx.EqualStr("", LF_ResidentNeeds.ResidentOf("no_such_need_quest"), "no resident is recorded for the unknown quest");

		LF_ResidentDef plain = LF_ResidentDef.Make("test_need_plain", "", "");
		ctx.False(LF_ResidentNeeds.Attach(giver.GetOwner(), plain), "a resident with no need attaches nothing");
		SCR_EntityHelper.DeleteEntityAndChildren(giver.GetOwner());
	}
}
