// red-proof: observed 2026-11-17 (#1962) with `tools\cli test --no-build --red LF_Test_EveronOldMan`: RedProof() makes the act load the core clerk instead of the old man, so his lines are missing (`- bye has a sound event`, `- bye has a length` fail), RED PROOF OK. Green without the hook (pack leg, worlds=LF_Everon1989_Test).
// tier: WORLD
//! #1962 in game: under the Everon 1989 header (the pack leg of `tools\cli test`), the old man on the bench
//! (Gaspard Lenoir) has a voiced dialogue about the zone and the move and one pointer for a newcomer. Every
//! line resolves to a caption key in the string table and a sound event with a length and a mouth envelope,
//! and every reply reads and leads somewhere playable. The pack owns it; the core ships no old man.
class LF_Test_EveronOldMan : LF_Test
{
	static const ResourceName OLD_MAN = "{99F9786003FB5B5E}Configs/Dialogue/LF_Dialogue_OldMan.conf";
	static const ResourceName CORE_CLERK = "{65C163E3C4868E76}Configs/Dialogue/LF_Dialogue_Clerk.conf";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/old-man";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// The act: the old man's dialogue as the pack ships it.
		LF_DialogueConfig config;
		if (!RedProof())
			config = LF_DialogueConfig.Load(OLD_MAN);
		else
			config = LF_DialogueConfig.Load(CORE_CLERK);

		ctx.NotNull(config, "the old man's dialogue loads");
		if (!config)
			return;

		ctx.EqualStr("old_man", config.m_sId, "the dialogue is the old man's");
		ctx.True(config.m_sSoundProject != "", "the old man has an audio project");
		ctx.True(WidgetManager.Translate(config.m_sSpeakerKey) == "Gaspard Lenoir", "the speaker is Gaspard Lenoir");

		array<string> problems = {};
		LF_DialogueFlow.Problems(config, problems);
		ctx.Equal(0, problems.Count(), "the old man's dialogue is playable");

		// The talk: the zone, the move, where to start, goodbye.
		array<string> lineIds = {"greet", "zone", "move", "start", "bye"};
		foreach (string lineId : lineIds)
		{
			LF_DialogueLine line = LF_DialogueFlow.FindLine(config, lineId);
			ctx.NotNull(line, "the old man has the line " + lineId);
			if (!line)
				continue;

			string caption = WidgetManager.Translate(line.m_sTextKey);
			ctx.True(caption != line.m_sTextKey && !caption.IsEmpty(), lineId + " has its caption in the string table");
			ctx.False(line.IsTextOnly(), lineId + " is voiced");
			ctx.True(!line.m_sEvent.IsEmpty(), lineId + " has a sound event");
			ctx.True(line.m_fSeconds > 1.5, lineId + " has a length");
			ctx.True(line.m_sMouth.Length() >= Math.Floor(line.m_fSeconds / LF_DialogueFlow.MOUTH_STEP) - 1, lineId + " has a mouth envelope as long as its recording");
			ctx.False(caption.IndexOf("bus") != -1 || caption.IndexOf("paper") != -1, lineId + " never mentions the bus or the paper");
		}

		LF_DialogueLine zone = LF_DialogueFlow.FindLine(config, "zone");
		LF_DialogueLine start = LF_DialogueFlow.FindLine(config, "start");
		if (zone)
			ctx.True(WidgetManager.Translate(zone.m_sTextKey).IndexOf("zone") != -1, "the zone line remembers the zone");

		if (start)
			ctx.True(WidgetManager.Translate(start.m_sTextKey).IndexOf("town board") != -1, "the pointer sends the newcomer to the town board");

		// Every reply has a caption and the greeting offers the whole talk.
		LF_DialogueNode greet = LF_DialogueFlow.FindNode(config, config.m_sStart);
		ctx.NotNull(greet, "the start node exists");
		if (!greet)
			return;

		ctx.True(greet.m_aOptions && greet.m_aOptions.Count() == 4, "the greeting offers four replies");
		if (!greet.m_aOptions)
			return;

		foreach (LF_DialogueOption option : greet.m_aOptions)
		{
			string reply = WidgetManager.Translate(option.m_sTextKey);
			ctx.True(reply != option.m_sTextKey && !reply.IsEmpty(), "reply " + option.m_sTextKey + " has its text in the string table");
		}
	}
}
