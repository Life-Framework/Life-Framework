// red-proof: RED-OK residents/resident-talk (fails with the hook on: no memory is applied, so every word reads neutral and the five greetings collapse to one)
//! Talking to a resident (#2908): the opening line follows the memory word for the player. The core
//! resident dialogue is loaded for real (WORLD); the rules around it are pure.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_ResidentTalk : LF_Test
{
	override string GetName()
	{
		return "residents/resident-talk";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_ResidentMemory.ResetStatics();
		LF_ResidentMemoryConfig config = new LF_ResidentMemoryConfig();
		config.m_iCooldownSeconds = 0;
		config.m_iGossipPercent = 0;
		config.m_iRefusesBelow = -60;
		config.m_iWaryBelow = -20;
		config.m_iFriendlyFrom = 20;
		config.m_iRegularFrom = 60;
		config.m_aReasons = {};
		config.m_aReasons.Insert(LF_ResidentMemoryReason.Make("helped", 10, false));
		config.m_aReasons.Insert(LF_ResidentMemoryReason.Make("crime_seen", -30, false));
		config.m_aKnows = {};
		LF_ResidentMemoryConfig.UseForTest(config);

		// Red-proof hook: with --red no memory is applied, so every character reads neutral.
		if (!RedProof())
		{
			int now = 1000;
			for (int i = 0; i < 3; i++)
			{
				now += 10;
				LF_ResidentMemory.Apply("t_res", "u_refuses", "crime_seen", now);
			}
			LF_ResidentMemory.Apply("t_res", "u_wary", "crime_seen", now);
			for (int j = 0; j < 2; j++)
			{
				now += 10;
				LF_ResidentMemory.Apply("t_res", "u_friendly", "helped", now);
			}
			for (int k = 0; k < 6; k++)
			{
				now += 10;
				LF_ResidentMemory.Apply("t_res", "u_regular", "helped", now);
			}
		}

		ResourceName path = LF_DialogueRegistry.Resolve("resident");
		ctx.True(!path.IsEmpty(), "the core index names a 'resident' dialogue");
		LF_DialogueConfig dialogue = LF_DialogueConfig.Layered(path);
		ctx.NotNull(dialogue, "the core resident dialogue loads");
		if (!dialogue)
			return;

		array<string> problems = {};
		LF_DialogueFlow.Problems(dialogue, problems);
		ctx.True(problems.IsEmpty(), "the core resident dialogue has no problems");

		array<string> uids = {"u_refuses", "u_wary", "u_none", "u_friendly", "u_regular"};
		array<string> words = {"refuses", "wary", "neutral", "friendly", "regular"};
		array<string> seen = {};
		for (int n = 0; n < uids.Count(); n++)
		{
			string word = LF_ResidentMemory.Tier("t_res", uids[n]);
			ctx.True(word == words[n], string.Format("%1 reads '%2'", uids[n], words[n]));
			string line = LF_ResidentTalk.LineFor(dialogue, "t_res", word);
			ctx.True(!line.IsEmpty(), string.Format("'%1' has an opening line", word));
			ctx.True(seen.Find(line) == -1, string.Format("'%1' opens on a line no other word does (%2)", word, line));
			seen.Insert(line);
			ctx.True(LF_DialogueFlow.FindLine(dialogue, line) != null, string.Format("line '%1' has its caption", line));
		}

		// The one who saw you steal turns away: no replies, the talk ends on the line.
		LF_DialogueNode refuses = LF_DialogueFlow.FindNode(dialogue, LF_ResidentTalk.StartNodeFor(dialogue, "t_res", "refuses"));
		ctx.True(refuses != null && (!refuses.m_aOptions || refuses.m_aOptions.IsEmpty()), "refuses offers no replies");
		LF_DialogueNode regular = LF_DialogueFlow.FindNode(dialogue, LF_ResidentTalk.StartNodeFor(dialogue, "t_res", "regular"));
		ctx.True(regular != null && regular.m_aOptions && !regular.m_aOptions.IsEmpty(), "a regular can answer");

		// A pack node for one resident id beats the core node; other residents keep the core one.
		LF_DialogueNode packed = new LF_DialogueNode();
		packed.m_sId = LF_ResidentTalk.ResidentNodeId("t_res", "regular");
		packed.m_sLine = "t_res_special";
		dialogue.m_aNodes.Insert(packed);
		ctx.True(LF_ResidentTalk.LineFor(dialogue, "t_res", "regular") == "t_res_special", "a pack node for the resident id wins");
		ctx.True(LF_ResidentTalk.LineFor(dialogue, "t_other", "regular") == "greet_regular", "another resident keeps the core line");
		ctx.True(LF_ResidentTalk.LineFor(dialogue, "t_res", "wary") == "greet_wary", "the pack node covers only its own word");

		// A dialogue with no tier nodes greets as it always did; a missing dialogue is empty.
		LF_DialogueConfig plain = new LF_DialogueConfig();
		plain.m_sStart = "greet";
		plain.m_aNodes = {};
		ctx.True(LF_ResidentTalk.StartNodeFor(plain, "t_res", "regular") == "greet", "no tier nodes: the dialogue's own start");
		ctx.True(LF_ResidentTalk.StartNodeFor(null, "t_res", "regular").IsEmpty(), "no dialogue: no node");

		LF_ResidentMemory.ResetStatics();
		LF_ResidentMemoryConfig.ResetStatics();
	}
}
