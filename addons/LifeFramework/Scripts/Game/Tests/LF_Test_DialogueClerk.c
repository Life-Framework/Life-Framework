// red-proof: observed 2026-11-14 (#323 clerk voice split): gave the core LF_Dialogue_Clerk.conf an m_sSoundProject with `tools/cli test --no-build --only LF_Test_DialogueClerk`; got `- the core clerk names no audio project: it ships no voice`. Restored; green.
// red-proof: Observed 2026-11-13 with `tools/cli test --no-build --only LF_Test_DialogueClerk`: switched off
// the story-line capture in LF_QuestTrackerComponent.Notify; got `[LFTEST] FAIL dialogue/clerk`,
// `- the clerk speaks the hand-in and the next errand (expected 'register;board', got '')`. Restored; green.
// tier: WORLD
//! The town clerk's conversation (Configs/Dialogue/LF_Dialogue_Clerk.conf on QuestGiver_Clerk.et):
//! the core ships it neutral and text only (no recordings, no audio project; a pack voices it, see
//! LF_Test_ClerkEveronVoice), the generated config loads and is playable, every hook it names exists,
//! and its "talk" reply runs the same server talk as before (LF_QuestTrackerComponent.RequestConverse)
//! with the clerk's story lines spoken instead of shown as slips. It needs no voice and no pack.
class LF_Test_DialogueClerk : LF_Test
{
	static const ResourceName CLERK = "{DE27E685D6FE93D7}Prefabs/Characters/Quests/QuestGiver_Clerk.et";
	static const ResourceName CLERK_CONF = "{65C163E3C4868E76}Configs/Dialogue/LF_Dialogue_Clerk.conf";

	override string GetName()
	{
		return "dialogue/clerk";
	}

	override void Run(LF_TestContext ctx)
	{
		// The core file as it ships: neutral, text only, no voice.
		LF_DialogueConfig shipped = LF_DialogueConfig.Load(CLERK_CONF);
		ctx.True(shipped != null, "the core clerk dialogue loads");
		if (shipped)
		{
			ctx.True(shipped.m_sSoundProject.IsEmpty(), "the core clerk names no audio project: it ships no voice");
			foreach (LF_DialogueLine line : shipped.m_aLines)
			{
				ctx.True(line.IsTextOnly(), "core line " + line.m_sId + " is text only (no event, no length)");
				ctx.True(WidgetManager.Translate(line.m_sTextKey).IndexOf("Everon") == -1, "core line " + line.m_sId + " names no place");
			}

			array<string> problems = {};
			LF_DialogueFlow.Problems(shipped, problems);
			ctx.Equal(0, problems.Count(), "the text-only core clerk is playable");
		}

		// The test's own clerk, a step from where the town's clerk is registered as the giver
		// (the quest talk checks the player stands at the registered one, as LF_Test_ArrivalChain does).
		IEntity clerk = LF_Utils.SpawnEntityPrefab(CLERK, "135 0 242.5");
		ctx.True(clerk != null, "the clerk prefab spawns");
		LF_DialogueConfig config = LF_DialogueComponent.ConfigOf(clerk);
		ctx.True(config != null, "the clerk has a playable dialogue");
		if (!clerk || !config)
		{
			if (clerk)
				SCR_EntityHelper.DeleteEntityAndChildren(clerk);

			return;
		}

		ctx.EqualStr("clerk", config.m_sId, "it is the clerk's dialogue");
		foreach (LF_DialogueLine line : config.m_aLines)
		{
			ctx.True(WidgetManager.Translate(line.m_sTextKey) != line.m_sTextKey, "line " + line.m_sId + " has its caption in the string table");
			// A pack that voices the clerk gives each line a recording as long as its mouth envelope; text only has neither.
			if (!line.IsTextOnly())
				ctx.True(line.m_fSeconds > 0.5 && line.m_sMouth.Length() >= Math.Floor(line.m_fSeconds / LF_DialogueFlow.MOUTH_STEP) - 1, "line " + line.m_sId + " has a mouth envelope as long as its recording");
		}

		foreach (LF_DialogueNode node : config.m_aNodes)
		{
			if (!node.m_aOptions)
				continue;

			foreach (LF_DialogueOption option : node.m_aOptions)
			{
				ctx.True(WidgetManager.Translate(option.m_sTextKey) != option.m_sTextKey, "reply " + option.m_sTextKey + " is in the string table");
				if (!option.m_sCondition.IsEmpty())
					ctx.True(LF_DialogueHook.Find(option.m_sCondition) != null, "condition hook exists: " + option.m_sCondition);

				if (!option.m_sAction.IsEmpty())
					ctx.True(LF_DialogueHook.Find(option.m_sAction) != null, "action hook exists: " + option.m_sAction);
			}
		}

		// The mouth pose names bones the clerk's skeleton has, or the mouth would never move.
		array<ref LF_DialogueMouthBone> rig = LF_DialogueVoice.Rig();
		ctx.True(rig.Count() >= 3, "the mouth pose moves the lips");
		Animation skeleton = clerk.GetAnimation();
		foreach (LF_DialogueMouthBone bone : rig)
			ctx.True(skeleton && skeleton.GetBoneIndex(bone.m_sBone) != -1, "the clerk's skeleton has the mouth bone " + bone.m_sBone);

		// Every story line the clerk can hand back in the arrival chain has a line of its own (voiced where a pack voices it).
		array<string> story = {"#LF-Quest_ArrivalRegister_TurnIn", "#LF-Quest_ArrivalBoard_Desc", "#LF-Quest_ArrivalWork_Desc", "#LF-Quest_ArrivalBank_Desc", "#LF-Quest_ArrivalDone_TurnIn", "#LF-Quest_Clerk_Idle"};
		foreach (string key : story)
			ctx.True(LF_DialogueFlow.FindLine(config, LF_DialogueFlow.SpokenFor(config, key)) != null, "the clerk speaks " + key);

		// The talk reply: the same server talk, its story lines collected for the scene.
		IEntity newcomer = LF_ExploitGuardsWorldFixture.SpawnCharacter(ctx, "134 0 242.5", true);
		if (!newcomer)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(clerk);
			return;
		}

		LF_QuestTrackerComponent tracker = LF_QuestTrackerComponent.Cast(newcomer.FindComponent(LF_QuestTrackerComponent));
		LF_DialogueNode greet = LF_DialogueFlow.FindNode(config, config.m_sStart);
		ctx.True(tracker != null && greet != null && greet.m_aOptions.Count() == 4, "the newcomer can be greeted with four replies");
		if (tracker && greet && greet.m_aOptions.Count() == 4)
		{
			LF_DialogueOption asNew = greet.m_aOptions[0];
			LF_DialogueOption asKnown = greet.m_aOptions[1];
			ctx.True(LF_DialogueHook.Offers(asNew, newcomer, clerk), "a newcomer is offered 'I am new in town'");
			ctx.False(LF_DialogueHook.Offers(asKnown, newcomer, clerk), "a newcomer is not offered 'What should I do now?'");
			array<string> spoken = {};
			LF_DialogueHook talk = LF_DialogueHook.Find(asNew.m_sAction);
			if (talk)
				talk.Run(newcomer, clerk, asNew.m_sParam, spoken);

			ctx.True(tracker.HasTurnedIn("arrival_register"), "the reply registers the player: the same talk as before");
			ctx.EqualStr("register;board", LF_DialogueFlow.SpokenList(config, null, spoken), "the clerk speaks the hand-in and the next errand");
			ctx.False(LF_DialogueHook.Offers(asNew, newcomer, clerk), "a registered player is no longer offered 'I am new in town'");
			ctx.True(LF_DialogueHook.Offers(asKnown, newcomer, clerk), "a registered player is offered 'What should I do now?'");
			spoken.Clear();
			if (talk)
				talk.Run(newcomer, clerk, "", spoken);

			ctx.EqualStr("board", LF_DialogueFlow.SpokenList(config, null, spoken), "mid-errand the clerk repeats the errand");
		}

		SCR_EntityHelper.DeleteEntityAndChildren(newcomer);
		SCR_EntityHelper.DeleteEntityAndChildren(clerk);
	}
}
