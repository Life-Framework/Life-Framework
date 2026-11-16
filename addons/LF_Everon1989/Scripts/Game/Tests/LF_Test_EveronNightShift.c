// red-proof: observed 2026-11-17 (#1963) with `tools\cli test --no-build --red LF_Test_EveronNightShift`: RedProof() skips the registry lookup, so the id names nothing; `- the night-shift worker's dialogue resolves by id`, RED PROOF OK. Green without the hook (pack leg, worlds=LF_Everon1989_Test).
// tier: WORLD
//! #1963 (The relocated, #1015): under the Everon 1989 header (the pack leg of `tools\cli test`) the
//! night-shift worker's dialogue resolves by id through the pack's index overlay, is playable, and every
//! line he says has a caption key that resolves, a sound event, a length and a mouth envelope. He remembers
//! the Zone and the move, and points a newcomer at the town board and the clerk's hostel.
class LF_Test_EveronNightShift : LF_Test
{
	static const string NIGHT_SHIFT_ID = "night_shift";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/night-shift";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// The act: the id as the running header's index overlay names it. Without the overlay (red proof) it names nothing.
		string path;
		if (!RedProof())
			path = LF_DialogueRegistry.Resolve(NIGHT_SHIFT_ID);

		ctx.True(!path.IsEmpty(), "the night-shift worker's dialogue resolves by id");
		if (path.IsEmpty())
			return;

		LF_DialogueConfig config = LF_DialogueConfig.Layered(path);
		ctx.NotNull(config, "the night-shift worker's dialogue loads");
		if (!config)
			return;

		array<string> problems = {};
		LF_DialogueFlow.Problems(config, problems);
		ctx.Equal(0, problems.Count(), "the night-shift worker's dialogue is playable");
		ctx.True(!config.m_sSoundProject.IsEmpty(), "the dialogue has an audio project");

		string speaker = WidgetManager.Translate(config.m_sSpeakerKey);
		ctx.True(speaker != config.m_sSpeakerKey && speaker.IndexOf("Duval") != -1, "the speaker is named Duval");

		array<string> lineIds = {"greet", "about_zone", "about_move", "about_work", "bye"};
		array<string> mustMention = {"families", "Zone", "suitcase", "town board", "Good night"};
		foreach (int i, string lineId : lineIds)
		{
			LF_DialogueLine line = LF_DialogueFlow.FindLine(config, lineId);
			ctx.NotNull(line, "the worker has the line " + lineId);
			if (!line)
				continue;

			string caption = WidgetManager.Translate(line.m_sTextKey);
			ctx.True(caption != line.m_sTextKey && !caption.IsEmpty(), lineId + " has its caption in the string table");
			ctx.True(caption.IndexOf(mustMention[i]) != -1, lineId + " says " + mustMention[i]);
			ctx.False(line.IsTextOnly(), lineId + " is voiced");
			ctx.True(!line.m_sEvent.IsEmpty(), lineId + " has a sound event");
			ctx.True(line.m_fSeconds > 1.0, lineId + " has a length");
			ctx.True(line.m_sMouth.Length() >= Math.Floor(line.m_fSeconds / LF_DialogueFlow.MOUTH_STEP) - 1, lineId + " has a mouth envelope as long as its recording");
		}

		// The talk: the greeting offers the Zone, the move, work and goodbye; each answer leads back to the same menu.
		LF_DialogueNode greet = LF_DialogueFlow.FindNode(config, config.m_sStart);
		ctx.NotNull(greet, "the start node exists");
		if (!greet || !greet.m_aOptions)
			return;

		ctx.Equal(4, greet.m_aOptions.Count(), "the greeting offers four replies");
		foreach (LF_DialogueOption option : greet.m_aOptions)
		{
			LF_DialogueNode next = LF_DialogueFlow.FindNode(config, option.m_sNext);
			ctx.NotNull(next, "reply " + option.m_sTextKey + " leads to a node");
			string reply = WidgetManager.Translate(option.m_sTextKey);
			ctx.True(reply != option.m_sTextKey && !reply.IsEmpty(), "reply " + option.m_sTextKey + " has its text in the string table");
			if (next && next.m_sId != "bye")
				ctx.Equal(4, next.m_aOptions.Count(), "after " + next.m_sId + " the menu comes back");
		}
	}
}
