// red-proof: 2026-11-19 (#3451) `toolscli test --no-build --red LF_Test_EveronGaspard` RED-OK (RedProof() lands no fish, so `the catches are counted (expected 3, got 0)` and `after the fish the cove is offered` fail); the plain run is green (pack leg, worlds=LF_Everon1989_Test).
//! #3451 in game: under the Everon 1989 header (the pack leg of `tools\cli test`), Gaspard (the old man) has a reply
//! about a cove that shows only once the character has landed three fish and, when picked, marks the cove on their
//! map (`LF_MapKnown`); the cove is a fishing table whose species bite at the dusk hours. The core ships no cove: the
//! pack's `LF_Coves_Everon1989.conf` is a fishing overlay (`LF_FishingLayers`) and `LF_Dialogue_Gaspard.conf` a patch
//! of the old man's dialogue.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronGaspard : LF_Test
{
	static const ResourceName OLD_MAN = "{99F9786003FB5B5E}Configs/Dialogue/LF_Dialogue_OldMan.conf";
	static const string TEST_UID = "lf-test-gaspard-angler";
	static const string COVE_PLACE = "fishermans-bay";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/gaspard";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// The cove: a pack table whose species the dusk hours decide.
		LF_FishingConfig.Reload();
		LF_FishingConfig fishing = LF_FishingConfig.Get();
		ctx.NotNull(fishing, "the fishing config resolves under the pack header");
		if (fishing)
			CheckCove(ctx, fishing);

		// The talk: the reply is gated by the hook and runs it.
		LF_DialogueConfig config = LF_DialogueConfig.Layered(OLD_MAN);
		ctx.NotNull(config, "the old man's dialogue loads with the cove patch");
		if (!config)
		{
			LF_FishingConfig.Reload();
			return;
		}

		array<string> problems = {};
		LF_DialogueFlow.Problems(config, problems);
		ctx.Equal(0, problems.Count(), "the patched dialogue is playable");

		LF_DialogueLine coveLine = LF_DialogueFlow.FindLine(config, "cove");
		ctx.NotNull(coveLine, "Gaspard has the cove line");
		if (coveLine)
		{
			string caption = WidgetManager.Translate(coveLine.m_sTextKey);
			ctx.True(caption != coveLine.m_sTextKey && !caption.IsEmpty(), "the cove line has its caption in the string table");
			ctx.True(caption.IndexOf("dusk") != -1, "the tip says when to go: dusk");
		}

		LF_DialogueOption reply;
		LF_DialogueNode regular = LF_DialogueFlow.FindNode(config, "greet_regular");
		ctx.NotNull(regular, "the regular greeting exists");
		if (regular && regular.m_aOptions)
		{
			foreach (LF_DialogueOption option : regular.m_aOptions)
			{
				if (option.m_sNext == "cove")
					reply = option;
			}
		}

		ctx.NotNull(reply, "a regular is offered the cove reply");
		if (reply)
		{
			CheckReply(ctx, reply);
			ctx.True(WidgetManager.Translate(reply.m_sTextKey) != reply.m_sTextKey, "the cove reply has its text in the string table");
		}

		LF_FishingConfig.Reload();
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckCove(LF_TestContext ctx, LF_FishingConfig fishing)
	{
		LF_FishTableDef cove = fishing.FindTable("cove");
		ctx.NotNull(cove, "the pack adds a cove table");
		if (!cove || !cove.m_aSpecies)
			return;

		int dusk;
		foreach (LF_FishWeightDef entry : cove.m_aSpecies)
		{
			LF_FishSpeciesDef species = fishing.FindSpecies(entry.m_sSpecies);
			ctx.NotNull(species, "the cove species " + entry.m_sSpecies + " exists");
			if (!species || species.m_aHours.IsEmpty())
				continue;

			// A species with hours bites at dusk only: 17 to 21.
			dusk++;
			ctx.True(species.BitesAt(19), entry.m_sSpecies + " bites at 19:00");
			ctx.False(species.BitesAt(12), entry.m_sSpecies + " does not bite at noon");
		}

		ctx.True(dusk > 0, "at least one cove species lists dusk hours");
		LF_FishCatch noon = LF_FishingRules.PickCatch(fishing, "cove", 5, "OCEAN", 12, 3, null, 0.0, 0.0);
		LF_FishCatch evening = LF_FishingRules.PickCatch(fishing, "cove", 5, "OCEAN", 19, 3, null, 0.0, 0.0);
		ctx.True(noon != null && noon.m_Species && noon.m_Species.m_aHours.IsEmpty(), "at noon the cove gives only the any-hour fish");
		ctx.True(evening != null && evening.m_Species && !evening.m_Species.m_aHours.IsEmpty(), "at dusk the cove gives its dusk fish");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckReply(LF_TestContext ctx, LF_DialogueOption reply)
	{
		ctx.EqualStr("CoveTip", reply.m_sCondition, "the reply is gated by the cove hook");
		ctx.EqualStr("CoveTip", reply.m_sAction, "the reply runs the cove hook");

		string placeId;
		int fish;
		ctx.True(LF_FishingLanded.ParseTip(reply.m_sParam, placeId, fish), "the hook param names a place and a catch count");
		ctx.EqualStr(COVE_PLACE, placeId, "the hook names the cove place");
		ctx.True(fish >= 1, "the gate is a catch count");

		LF_DialogueHook hook = LF_DialogueHook.Find(reply.m_sCondition);
		ctx.NotNull(hook, "the hook class exists");
		IEntity angler = LF_TestCharacterFixture.Spawn(ctx);
		if (!hook || !angler)
		{
			LF_TestCharacterFixture.Delete(angler);
			return;
		}

		array<ref LF_JobXpRow> savedXp = LF_JobLevels.ExportAll();
		array<ref LF_MapKnownRow> savedKnown = LF_MapKnown.ExportAll();
		LF_Utils.SetUidForTests(angler, TEST_UID);
		LF_JobLevels.ApplyAll(null);
		LF_MapKnown.ApplyAll(null);

		ctx.False(hook.Allows(angler, null, reply.m_sParam),"a character with no fish is not offered the cove");
		ctx.Equal(0, LF_FishingLanded.Count(TEST_UID), "a new character has landed nothing");

		// The act: land the fish the gate asks for. RedProof() lands none.
		if (!RedProof())
			LF_JobLevels.Award(TEST_UID, LF_FishingSession.XP_ACTION, fish);

		ctx.Equal(fish, LF_FishingLanded.Count(TEST_UID), "the catches are counted");
		ctx.True(hook.Allows(angler, null, reply.m_sParam), "after the fish the cove is offered");

		// The hook hears the cove place for the character (the server half; no player controls the fixture body).
		ctx.True(LF_MapKnown.KnownOf(TEST_UID, placeId) == LF_EMapPoiKnown.NONE, "before the tip the cove is unknown");
		LF_MapKnown.HearCharacter(TEST_UID, placeId);
		ctx.True(LF_MapKnown.KnownOf(TEST_UID, placeId) == LF_EMapPoiKnown.HEARD, "after the tip the cove is heard of");

		LF_Utils.SetUidForTests(null, "");
		LF_JobLevels.ApplyAll(savedXp);
		LF_MapKnown.ApplyAll(savedKnown);
		LF_TestCharacterFixture.Delete(angler);
	}
}
