// red-proof: observed 2026-11-17 (#1961) with `tools\cli test --no-build --red LF_Test_EveronCafeOwner`: RedProof() makes the act load a dialogue the pack lacks, so no conversation loads (RED-OK everon/cafe-owner, 3 assertions, `the café owner's dialogue loads`). Green without the hook (pack leg, worlds=LF_Everon1989_Test).
// tier: WORLD
//! #1961 in game: under the Everon 1989 header (the pack leg of `tools\cli test`), the café owner Odile Marchand
//! has a voiced conversation about the zone and the move, and three useful pointers (work, the clerk, the handbook).
//! Every line has a caption key that resolves, a sound event and a length with a mouth envelope; every reply
//! has its text; the conversation is playable and ends. The dialogue is pack content, voiced with ElevenLabs
//! (voice Mariva Viva Muse, tools/assets/dialogue/cafe_owner).
class LF_Test_EveronCafeOwner : LF_Test
{
	static const ResourceName CAFE_OWNER = "{8A8B0025C1E0037F}Configs/Dialogue/LF_Dialogue_CafeOwner.conf";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/cafe-owner";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_MissionHeader header = LF_MissionHeader.GetCurrent();
		ctx.NotNull(header, "the pack leg runs under an LF mission header");

		// The act: the café owner's dialogue as the pack ships it. Red proof: the same act on a resource the pack lacks.
		ResourceName path = CAFE_OWNER;
		if (RedProof())
			path = "{0000000000000000}Configs/Dialogue/LF_Dialogue_CafeOwnerMissing.conf";

		LF_DialogueConfig config = LF_DialogueConfig.Load(path);
		ctx.NotNull(config, "the café owner's dialogue loads");
		if (!config)
			return;

		ctx.EqualStr("cafe_owner", config.m_sId, "the dialogue is the café owner's");
		ctx.True(!config.m_sSoundProject.IsEmpty(), "the dialogue names its audio project");
		ctx.EqualStr("Odile Marchand", WidgetManager.Translate(config.m_sSpeakerKey), "the speaker is Odile Marchand");

		array<string> problems = {};
		LF_DialogueFlow.Problems(config, problems);
		ctx.Equal(0, problems.Count(), "the café owner's dialogue is playable");

		// Each line: a caption in the string table, a sound event, a length and a mouth envelope. Seven lines: the
		// greeting, two memories (the zone, the move), three pointers (work, the clerk, the handbook) and the goodbye.
		array<string> lineIds = {"greet", "zone", "move", "work", "clerk", "handbook", "bye"};
		ctx.Equal(lineIds.Count(), config.m_aLines.Count(), "the café owner has exactly the seven lines");
		foreach (string lineId : lineIds)
		{
			LF_DialogueLine line = LF_DialogueFlow.FindLine(config, lineId);
			ctx.NotNull(line, "the café owner has the line " + lineId);
			if (!line)
				continue;

			string caption = WidgetManager.Translate(line.m_sTextKey);
			ctx.True(caption != line.m_sTextKey && !caption.IsEmpty(), lineId + " has its caption in the string table");
			ctx.False(line.IsTextOnly(), lineId + " is voiced");
			ctx.True(!line.m_sEvent.IsEmpty(), lineId + " has a sound event");
			ctx.True(line.m_fSeconds > 1.0, lineId + " has a length");
			ctx.True(line.m_sMouth.Length() >= Math.Floor(line.m_fSeconds / LF_DialogueFlow.MOUTH_STEP) - 1, lineId + " has a mouth envelope as long as its recording");
		}

		// What she remembers, and what she points to.
		LF_DialogueLine zone = LF_DialogueFlow.FindLine(config, "zone");
		LF_DialogueLine work = LF_DialogueFlow.FindLine(config, "work");
		LF_DialogueLine clerk = LF_DialogueFlow.FindLine(config, "clerk");
		LF_DialogueLine handbook = LF_DialogueFlow.FindLine(config, "handbook");
		if (zone)
			ctx.True(WidgetManager.Translate(zone.m_sTextKey).IndexOf("zone") != -1, "she remembers the zone");

		if (work)
			ctx.True(WidgetManager.Translate(work.m_sTextKey).IndexOf("post office") != -1, "she points to the post office for work");

		if (clerk)
			ctx.True(WidgetManager.Translate(clerk.m_sTextKey).IndexOf("clerk") != -1, "she points to the clerk");

		if (handbook)
			ctx.True(WidgetManager.Translate(handbook.m_sTextKey).IndexOf("handbook") != -1, "she points to the handbook");

		// Each reply the player can pick has its text, and every node leads somewhere that exists.
		foreach (LF_DialogueNode node : config.m_aNodes)
		{
			if (!node.m_aOptions)
				continue;

			foreach (LF_DialogueOption option : node.m_aOptions)
			{
				string reply = WidgetManager.Translate(option.m_sTextKey);
				ctx.True(reply != option.m_sTextKey && !reply.IsEmpty(), "reply " + option.m_sTextKey + " has its text");
				ctx.NotNull(LF_DialogueFlow.FindNode(config, option.m_sNext), "reply " + option.m_sTextKey + " leads to node " + option.m_sNext);
			}
		}

		// The talk is a menu she can be asked about in any order and that always ends on the goodbye.
		LF_DialogueNode start = LF_DialogueFlow.FindNode(config, config.m_sStart);
		ctx.NotNull(start, "the conversation has a start");
		LF_DialogueNode bye = LF_DialogueFlow.FindNode(config, "bye");
		ctx.NotNull(bye, "the conversation has a goodbye");
		if (bye)
			ctx.True(!bye.m_aOptions || bye.m_aOptions.IsEmpty(), "the goodbye ends the talk");
	}
}
