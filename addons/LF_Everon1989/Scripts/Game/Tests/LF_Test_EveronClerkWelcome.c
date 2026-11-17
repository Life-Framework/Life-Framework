// red-proof: observed 2026-11-17 (#1959) with `tools\cli test --no-build --red LF_Test_EveronClerkWelcome`: RedProof() makes the act load the core clerk, so no welcome node exists; got `- the welcome is the papers, the room, then what the town expects (expected 'welcome_papers;welcome_room;welcome_town;', got '')`, RED PROOF OK. Green without the hook (pack leg, worlds=LF_Everon1989_Test).
// tier: WORLD
//! #1959 in game: under the Everon 1989 header (the pack leg of `tools\cli test`), a character who is new
//! in town and talks to the clerk hears a voiced welcome before the register: the papers (Form R-89 and the
//! identity card), the hostel for the first night, and what the town expects. Each welcome line has a caption
//! key that resolves, a sound event and a length with a mouth envelope. A character that is already registered
//! is not offered the welcome. The core clerk ships none of it (it is text only and neutral).
class LF_Test_EveronClerkWelcome : LF_Test
{
	static const ResourceName OVERLAY = "{73865DD94EA3B5CA}Configs/Dialogue/LF_Dialogue_ClerkEveron.conf";
	static const ResourceName CORE_CLERK = "{65C163E3C4868E76}Configs/Dialogue/LF_Dialogue_Clerk.conf";
	static const string CLERK_ID = "clerk";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/clerk-welcome";
	}

	//------------------------------------------------------------------------------------------------
	//! The option of a node whose reply key ends with `suffix` (SayNew, SayNext ...), or null.
	protected LF_DialogueOption ReplyOf(LF_DialogueNode node, string suffix)
	{
		if (!node || !node.m_aOptions)
			return null;

		foreach (LF_DialogueOption option : node.m_aOptions)
		{
			if (option.m_sTextKey.EndsWith(suffix))
				return option;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_MissionHeader header = LF_MissionHeader.GetCurrent();
		ctx.NotNull(header, "the pack leg runs under an LF mission header");
		if (header)
			ctx.True(header.m_aLF_ConfigOverlays && header.m_aLF_ConfigOverlays.Find(OVERLAY) != -1, "the header lists the Everon clerk dialogue overlay");

		// The core clerk as shipped says no welcome: neutral, text only, no hostel.
		LF_DialogueConfig core = LF_DialogueConfig.Load(CORE_CLERK);
		ctx.NotNull(core, "the core clerk dialogue loads");
		if (core)
		{
			ctx.True(LF_DialogueFlow.FindLine(core, "welcome_papers") == null, "the core clerk has no welcome (the pack decides)");
			ctx.True(LF_DialogueFlow.FindNode(core, "welcome_papers") == null, "the core clerk has no welcome node");
		}

		// The act: the clerk as the running header lays its overlay over the core file.
		LF_DialogueConfig config;
		if (!RedProof())
			config = LF_DialogueConfig.Layered(LF_DialogueRegistry.Resolve(CLERK_ID));
		else
			config = LF_DialogueConfig.Load(CORE_CLERK);

		ctx.NotNull(config, "the Everon clerk dialogue loads");
		if (!config)
			return;

		array<string> problems = {};
		LF_DialogueFlow.Problems(config, problems);
		ctx.Equal(0, problems.Count(), "the clerk's dialogue with the welcome is playable");

		// The welcome: three lines, each with a caption, a sound event and a length.
		array<string> lineIds = {"welcome_papers", "welcome_room", "welcome_town"};
		array<string> mustMention = {"R-89", "hostel", "town"};
		foreach (int i, string lineId : lineIds)
		{
			LF_DialogueLine line = LF_DialogueFlow.FindLine(config, lineId);
			ctx.NotNull(line, "the clerk has the line " + lineId);
			if (!line)
				continue;

			string caption = WidgetManager.Translate(line.m_sTextKey);
			ctx.True(caption != line.m_sTextKey && !caption.IsEmpty(), lineId + " has its caption in the string table");
			ctx.True(caption.IndexOf(mustMention[i]) != -1, lineId + " says " + mustMention[i]);
			ctx.False(caption.IndexOf("homestay") != -1, lineId + " says hostel, never homestay (#1958)");
			ctx.False(line.IsTextOnly(), lineId + " is voiced");
			ctx.True(!line.m_sEvent.IsEmpty(), lineId + " has a sound event");
			ctx.True(line.m_fSeconds > 2.0, lineId + " has a length");
			ctx.True(line.m_sMouth.Length() >= Math.Floor(line.m_fSeconds / LF_DialogueFlow.MOUTH_STEP) - 1, lineId + " has a mouth envelope as long as its recording");
		}

		// The path: the newcomer's reply leads through the three lines, then to the menu with the register.
		LF_DialogueNode greet = LF_DialogueFlow.FindNode(config, config.m_sStart);
		LF_DialogueOption asNew = ReplyOf(greet, "SayNew");
		LF_DialogueOption asKnown = ReplyOf(greet, "SayNext");
		LF_DialogueOption asLost = ReplyOf(greet, "SayLost");
		ctx.NotNull(asNew, "the greeting has the 'I am new' reply");
		ctx.NotNull(asKnown, "the greeting still has 'What should I do now?'");
		ctx.NotNull(asLost, "the greeting still has the lost-card reply");
		if (!asNew || !asKnown || !asLost)
			return;

		ctx.EqualStr("QuestNotTurnedIn", asNew.m_sCondition, "only a character not yet registered is offered the welcome");
		ctx.EqualStr("welcome_papers", asNew.m_sNext, "the newcomer goes to the welcome");
		ctx.True(asKnown.m_sNext != "welcome_papers" && asLost.m_sNext != "welcome_papers", "a registered character and a lost card skip the welcome");

		string chain = "";
		string nodeId = asNew.m_sNext;
		LF_DialogueOption last;
		int guard = 0;
		while (nodeId != "menu" && guard < 6)
		{
			LF_DialogueNode node = LF_DialogueFlow.FindNode(config, nodeId);
			ctx.NotNull(node, "welcome node " + nodeId + " exists");
			if (!node)
				return;

			chain += node.m_sLine + ";";
			if (!node.m_aOptions || node.m_aOptions.Count() != 1)
			{
				ctx.Fail("welcome node " + nodeId + " offers one way on");
				return;
			}

			last = node.m_aOptions[0];
			nodeId = last.m_sNext;
			guard++;
		}

		ctx.EqualStr("welcome_papers;welcome_room;welcome_town;", chain, "the welcome is the papers, the room, then what the town expects");
		if (!last)
		{
			ctx.Fail("the newcomer's reply leads past no welcome node");
			return;
		}

		ctx.EqualStr("QuestConverse", last.m_sAction, "after the welcome the clerk registers the newcomer (the same talk as before)");
		ctx.EqualStr("arrival_register", last.m_sParam, "the register is the arrival register");

		// Who is offered it: a new character is, a registered one is not.
		IEntity fresh = LF_ExploitGuardsWorldFixture.SpawnCharacter(ctx, "0 0 0", false);
		IEntity known = LF_ExploitGuardsWorldFixture.SpawnCharacter(ctx, "2 0 0", false);
		if (fresh && known)
		{
			LF_QuestTrackerComponent knownTracker = LF_Component<LF_QuestTrackerComponent>.Find(known);
			ctx.NotNull(knownTracker, "the registered character has a quest tracker");
			if (knownTracker)
			{
				array<string> done = {"arrival_register"};
				knownTracker.RestoreTurnedIn(done);
				ctx.True(LF_DialogueHook.Offers(asNew, fresh, null), "a new character is offered 'I am new in Everon'");
				ctx.False(LF_DialogueHook.Offers(asNew, known, null), "a registered character is not offered the welcome");
				ctx.True(LF_DialogueHook.Offers(asKnown, known, null), "a registered character is offered the next errand");
				ctx.False(LF_DialogueHook.Offers(asKnown, fresh, null), "a new character is not offered the next errand yet");
			}
		}

		if (fresh)
			SCR_EntityHelper.DeleteEntityAndChildren(fresh);

		if (known)
			SCR_EntityHelper.DeleteEntityAndChildren(known);
	}
}
