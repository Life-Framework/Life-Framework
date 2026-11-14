// red-proof: observed 2026-11-14 with `tools\cli test --no-build --only LF_Test_DialoguePackOverlay`: making LF_DialogueIndexConfig.PathOf match the wrong id failed `- the clerk id names the clerk file` and `- add: the pack's new dialogue resolves by id`; dropping the IsTextOnly exemption in LF_DialogueFlow.Problems failed `- a text-only line needs no recording (expected 0, got 1)`. Restored; green.
// tier: LOGIC
//! #957: dialogue by id and the text-only line. The registry maps an id to a dialogue file and a pack
//! adds, replaces or removes an id with the override verbs. A line with no event and no length is text
//! only and the dialogue stays playable; a half-made recording does not. A pack patch that leaves the
//! dialogue broken is dropped whole and the core one plays.
class LF_Test_DialoguePackOverlay : LF_Test
{
	override string GetName()
	{
		return "dialogue/pack-overlay";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_DialogueIndexEntry Entry(string id, ResourceName conf)
	{
		LF_DialogueIndexEntry entry = new LF_DialogueIndexEntry();
		entry.m_sId = id;
		entry.m_Conf = conf;
		return entry;
	}

	//------------------------------------------------------------------------------------------------
	protected LF_DialogueIndexConfig MakeIndex()
	{
		LF_DialogueIndexConfig index = new LF_DialogueIndexConfig();
		index.m_aEntries = {};
		index.m_aEntries.Insert(Entry("clerk", "{65C163E3C4868E76}Configs/Dialogue/LF_Dialogue_Clerk.conf"));
		index.m_aEntries.Insert(Entry("fuel_clerk", "{6E980D0B378BB71E}Configs/Dialogue/LF_Dialogue_FuelClerk.conf"));
		return index;
	}

	//------------------------------------------------------------------------------------------------
	protected LF_DialogueOption Reply(string key, string next)
	{
		LF_DialogueOption option = new LF_DialogueOption();
		option.m_sTextKey = key;
		option.m_sNext = next;
		return option;
	}

	//------------------------------------------------------------------------------------------------
	protected LF_DialogueLine MakeLine(string id, string sound, float seconds)
	{
		LF_DialogueLine line = new LF_DialogueLine();
		line.m_sId = id;
		line.m_sTextKey = "#LF-Dialogue_Clerk_Greet";
		line.m_sEvent = sound;
		line.m_fSeconds = seconds;
		return line;
	}

	//------------------------------------------------------------------------------------------------
	//! greet (voiced, one reply to bye) and bye (says bye, no replies).
	protected LF_DialogueConfig MakeCore()
	{
		LF_DialogueConfig config = new LF_DialogueConfig();
		config.m_sId = "test-npc";
		config.m_sStart = "greet";
		config.m_aLines = {};
		config.m_aLines.Insert(MakeLine("greet", "SOUND_TEST_greet", 1.5));
		config.m_aLines.Insert(MakeLine("bye", "SOUND_TEST_bye", 1.5));
		config.m_aNodes = {};
		LF_DialogueNode greet = new LF_DialogueNode();
		greet.m_sId = "greet";
		greet.m_sLine = "greet";
		greet.m_aOptions = {};
		greet.m_aOptions.Insert(Reply("#LF-Dialogue_Clerk_SayBye", "bye"));
		config.m_aNodes.Insert(greet);
		LF_DialogueNode bye = new LF_DialogueNode();
		bye.m_sId = "bye";
		bye.m_sLine = "bye";
		config.m_aNodes.Insert(bye);
		return config;
	}

	//------------------------------------------------------------------------------------------------
	protected bool Op(LF_ConfigList list, string id, string verb)
	{
		return LF_ConfigOverlay.Apply(list, id, verb, "test-pack.conf", null, null, null);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// By id: the registry names the file; an unknown id names nothing.
		LF_DialogueIndexConfig index = MakeIndex();
		ctx.EqualStr("{65C163E3C4868E76}Configs/Dialogue/LF_Dialogue_Clerk.conf", index.PathOf("clerk"), "the clerk id names the clerk file");
		ctx.True(index.PathOf("baker").IsEmpty(), "an id no addon registers names nothing");
		ctx.True(index.PathOf("").IsEmpty(), "an empty id names nothing");

		// A pack replaces the file an id points at, and adds an id.
		LF_DialogueIndexList list = new LF_DialogueIndexList(index);
		list.Set(list.IndexOf("clerk"), Entry("clerk", "{38D29E0E9DC24207}Configs/Tests/LF_Test_DialogueClerkPatch.conf"));
		ctx.EqualStr("{38D29E0E9DC24207}Configs/Tests/LF_Test_DialogueClerkPatch.conf", index.PathOf("clerk"), "replace: the pack's file answers for the clerk id");
		ctx.Equal(2, index.m_aEntries.Count(), "replace keeps the entry count");
		list.Set(list.Count(), Entry("baker", "{B34964F9C0AB3BB7}Configs/Tests/LF_Test_DialogueClerkBadPatch.conf"));
		ctx.EqualStr("{B34964F9C0AB3BB7}Configs/Tests/LF_Test_DialogueClerkBadPatch.conf", index.PathOf("baker"), "add: the pack's new dialogue resolves by id");

		// remove and the refusals.
		index = MakeIndex();
		list = new LF_DialogueIndexList(index);
		ctx.True(Op(list, "fuel_clerk", "remove"), "remove: an id is taken out");
		ctx.True(index.PathOf("fuel_clerk").IsEmpty(), "remove: the id no longer resolves");
		ctx.Equal(1, index.m_aEntries.Count(), "remove: one entry left");
		ctx.True(!Op(list, "ghost", "remove"), "remove of an unknown id is refused");
		ctx.True(!Op(list, "ghost", "patch"), "patch of an unknown id is refused");
		ctx.True(!Op(list, "clerk", "add"), "add of an id the index has is refused");
		ctx.Equal(1, index.m_aEntries.Count(), "refused entries change nothing");

		// Text only: no event and no length is playable, and shows for reading time.
		LF_DialogueConfig core = MakeCore();
		array<string> problems = {};
		LF_DialogueFlow.Problems(core, problems);
		ctx.Equal(0, problems.Count(), "the test dialogue is playable");

		LF_DialogueConfig patched = MakeCore();
		patched.m_aLines.Insert(MakeLine("extra", "", 0));
		LF_DialogueNode extra = new LF_DialogueNode();
		extra.m_sId = "extra";
		extra.m_sLine = "extra";
		patched.m_aNodes.Insert(extra);
		patched.m_aNodes[0].m_aOptions.Insert(Reply("#LF-Dialogue_Clerk_SayWork", "extra"));
		ctx.True(patched.m_aLines[2].IsTextOnly(), "a line with no event and no length is text only");
		problems.Clear();
		LF_DialogueFlow.Problems(patched, problems);
		ctx.Equal(0, problems.Count(), "a text-only line needs no recording");
		ctx.True(LF_DialogueFlow.Choose(core, patched) == patched, "a patch that adds a text-only line plays");
		ctx.True(LF_DialogueFlow.HoldSeconds(patched.m_aLines[2], 40) >= LF_DialogueFlow.SILENT_SECONDS_MIN, "a text-only line holds for reading time");
		ctx.True(!patched.m_aLines[0].IsTextOnly(), "a voiced line is not text only");

		// A half-made recording is still a problem; the broken patch is dropped whole.
		LF_DialogueConfig half = MakeCore();
		half.m_aLines.Insert(MakeLine("half", "SOUND_TEST_half", 0));
		problems.Clear();
		LF_DialogueFlow.Problems(half, problems);
		ctx.True(problems.Count() > 0, "an event with no length is a half-made recording");
		ctx.True(LF_DialogueFlow.Choose(core, half) == core, "a patch with a half-made recording is dropped; the core plays");
	}
}
