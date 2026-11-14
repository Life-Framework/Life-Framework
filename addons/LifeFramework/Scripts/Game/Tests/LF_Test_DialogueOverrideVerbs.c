// red-proof: observed 2026-11-14 with `tools\cli test --no-build --only LF_Test_DialogueOverrideVerbs`: making LF_DialogueNodeList.RemoveAt a no-op failed `- remove: two nodes left (expected 2, got 3)` and `- remove: the node is gone by id`; making LF_DialogueFlow.Choose always return the patched dialogue failed `- a patch that leaves a reply to a removed node is dropped; the core plays`. Restored; green.
// tier: LOGIC
//! #1109, #957: the dialogue nodes and lines on the override model (#962). `op: remove` takes a node
//! or a line out, `op: patch`, `remove` of an id no lower layer has and `add` of one it has are
//! refused. A patched dialogue that is broken (a reply to a removed node, a hook with no class) is
//! dropped whole and the core dialogue plays; a sound patch is the one that plays.
class LF_Test_DialogueOverrideVerbs : LF_Test
{
	override string GetName()
	{
		return "dialogue/override-verbs";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_DialogueOption Reply(string key, string next, string action = "")
	{
		LF_DialogueOption option = new LF_DialogueOption();
		option.m_sTextKey = key;
		option.m_sNext = next;
		option.m_sAction = action;
		return option;
	}

	//------------------------------------------------------------------------------------------------
	//! A reply list of one or two (a null second is left out).
	protected array<ref LF_DialogueOption> Two(LF_DialogueOption a, LF_DialogueOption b)
	{
		array<ref LF_DialogueOption> list = {};
		list.Insert(a);
		if (b)
			list.Insert(b);
		return list;
	}

	//------------------------------------------------------------------------------------------------
	protected LF_DialogueNode MakeNode(string id, string line, array<ref LF_DialogueOption> options)
	{
		LF_DialogueNode node = new LF_DialogueNode();
		node.m_sId = id;
		node.m_sLine = line;
		node.m_aOptions = options;
		return node;
	}

	//------------------------------------------------------------------------------------------------
	protected LF_DialogueLine MakeLine(string id)
	{
		LF_DialogueLine line = new LF_DialogueLine();
		line.m_sId = id;
		line.m_sTextKey = "#LF-Dialogue_Clerk_Greet";
		line.m_sEvent = "SOUND_TEST_" + id;
		line.m_fSeconds = 1.5;
		return line;
	}

	//------------------------------------------------------------------------------------------------
	//! greet (says greet, replies to menu and bye), menu (replies to bye), bye (says bye).
	protected LF_DialogueConfig MakeCore()
	{
		LF_DialogueConfig config = new LF_DialogueConfig();
		config.m_sId = "test-npc";
		config.m_sStart = "greet";
		config.m_aLines = {};
		config.m_aLines.Insert(MakeLine("greet"));
		config.m_aLines.Insert(MakeLine("bye"));
		config.m_aNodes = {};
		config.m_aNodes.Insert(MakeNode("greet", "greet", Two(Reply("#LF-Dialogue_Clerk_SayNext", "menu"), Reply("#LF-Dialogue_Clerk_SayBye", "bye"))));
		config.m_aNodes.Insert(MakeNode("menu", "", Two(Reply("#LF-Dialogue_Clerk_SayBye", "bye"), null)));
		config.m_aNodes.Insert(MakeNode("bye", "bye", null));
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
		LF_DialogueConfig core = MakeCore();
		array<string> problems = {};
		LF_DialogueFlow.Problems(core, problems);
		ctx.Equal(0, problems.Count(), "the test dialogue is playable");

		// remove: a node and a line leave their lists.
		LF_DialogueConfig patched = MakeCore();
		LF_DialogueNodeList nodes = new LF_DialogueNodeList(patched);
		LF_DialogueLineList lines = new LF_DialogueLineList(patched);
		ctx.True(Op(nodes, "bye", "remove"), "remove: a node is taken out");
		ctx.Equal(2, patched.m_aNodes.Count(), "remove: two nodes left");
		ctx.True(LF_DialogueFlow.FindNode(patched, "bye") == null, "remove: the node is gone by id");
		ctx.True(Op(lines, "bye", "remove"), "remove: a line is taken out");
		ctx.True(LF_DialogueFlow.FindLine(patched, "bye") == null, "remove: the line is gone by id");
		ctx.Equal(1, patched.m_aLines.Count(), "remove: one line left");

		// A removed node that replies still lead to makes the dialogue broken: the core plays.
		LF_DialogueConfig chosen = LF_DialogueFlow.Choose(core, patched);
		ctx.True(chosen == core, "a patch that leaves a reply to a removed node is dropped; the core plays");

		// patch of a missing id, remove of a missing id and add of an existing id are refused.
		patched = MakeCore();
		nodes = new LF_DialogueNodeList(patched);
		lines = new LF_DialogueLineList(patched);
		ctx.True(!Op(nodes, "ghost", "patch"), "patch of a missing node is refused");
		ctx.True(!Op(nodes, "ghost", "remove"), "remove of a missing node is refused");
		ctx.True(!Op(nodes, "menu", "add"), "add of an existing node is refused");
		ctx.True(!Op(lines, "ghost", "patch"), "patch of a missing line is refused");
		ctx.True(!Op(lines, "greet", "add"), "add of an existing line is refused");
		ctx.True(!Op(nodes, "menu", "delete"), "an unknown verb is refused");
		ctx.Equal(3, patched.m_aNodes.Count(), "refused entries change no node");
		ctx.Equal(2, patched.m_aLines.Count(), "refused entries change no line");
		ctx.True(LF_DialogueFlow.FindNode(patched, "ghost") == null, "a refused patch adds nothing");

		// A patch that keeps the dialogue playable is the one that plays.
		patched = MakeCore();
		nodes = new LF_DialogueNodeList(patched);
		LF_DialogueNode menu = LF_DialogueFlow.FindNode(patched, "menu");
		menu.m_aOptions = Two(Reply("#LF-Dialogue_Clerk_SayBye", "bye"), Reply("#LF-Dialogue_Clerk_SayWork", "greet"));
		ctx.True(LF_DialogueFlow.Choose(core, patched) == patched, "a playable patch is used");

		// A hook that no class implements drops the patch; a shipped hook is fine.
		patched = MakeCore();
		menu = LF_DialogueFlow.FindNode(patched, "menu");
		menu.m_aOptions = Two(Reply("#LF-Dialogue_Clerk_SayBye", "bye", "QuestConverse"), null);
		ctx.True(LF_DialogueFlow.Choose(core, patched) == patched, "a reply that names a shipped hook is kept");
		menu.m_aOptions = Two(Reply("#LF-Dialogue_Clerk_SayBye", "bye", "NoSuchHookAnywhere"), null);
		ctx.True(LF_DialogueFlow.Choose(core, patched) == core, "a reply that names a hook with no class drops the patch");
	}
}
