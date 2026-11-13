// red-proof: Observed 2026-11-13 with `tools/cli test --no-build --only LF_Test_DialogueFlow`: made
// LF_DialogueFlow.IsOffered return true; got `[LFTEST] FAIL dialogue/flow`,
// `- a reply that was not offered cannot be picked`. Restored; green.
// tier: LOGIC
//! The rules of an NPC conversation (Feature/Dialogue/LF_DialogueConfig.c, LF_DialogueFlow): a
//! reply the server did not offer cannot be picked, what a feature hands back is spoken by the
//! line voiced for it, broken data is named, and the mouth and camera maths hold their bounds.
class LF_Test_DialogueFlow : LF_Test
{
	override string GetName()
	{
		return "dialogue/flow";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_DialogueOption Option(string text, string next, string condition = "")
	{
		LF_DialogueOption option = new LF_DialogueOption();
		option.m_sTextKey = text;
		option.m_sNext = next;
		option.m_sCondition = condition;
		return option;
	}

	//------------------------------------------------------------------------------------------------
	protected LF_DialogueLine Line(string id, string forKey = "")
	{
		LF_DialogueLine line = new LF_DialogueLine();
		line.m_sId = id;
		line.m_sTextKey = "#T_" + id;
		line.m_sForKey = forKey;
		line.m_sEvent = "SOUND_" + id;
		line.m_fSeconds = 2.5;
		line.m_sMouth = "0909";
		return line;
	}

	//------------------------------------------------------------------------------------------------
	//! greet -(0: conditional)-> menu, -(1)-> menu, -(2)-> bye; bye has a line and no reply.
	protected LF_DialogueConfig Config()
	{
		LF_DialogueConfig config = new LF_DialogueConfig();
		config.m_sId = "test";
		config.m_sStart = "greet";
		config.m_aNodes = {};
		config.m_aLines = {};
		config.m_aLines.Insert(Line("greet"));
		config.m_aLines.Insert(Line("register", "#Quest_TurnIn"));
		config.m_aLines.Insert(Line("bye"));

		LF_DialogueNode greet = new LF_DialogueNode();
		greet.m_sId = "greet";
		greet.m_sLine = "greet";
		greet.m_aOptions = {};
		greet.m_aOptions.Insert(Option("#R_new", "menu", "SomeCondition"));
		greet.m_aOptions.Insert(Option("#R_work", "menu"));
		greet.m_aOptions.Insert(Option("#R_bye", "bye"));
		config.m_aNodes.Insert(greet);

		LF_DialogueNode menu = new LF_DialogueNode();
		menu.m_sId = "menu";
		menu.m_aOptions = {};
		menu.m_aOptions.Insert(Option("#R_bye", "bye"));
		config.m_aNodes.Insert(menu);

		LF_DialogueNode bye = new LF_DialogueNode();
		bye.m_sId = "bye";
		bye.m_sLine = "bye";
		config.m_aNodes.Insert(bye);
		return config;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_DialogueConfig config = Config();
		array<string> problems = {};
		LF_DialogueFlow.Problems(config, problems);
		ctx.Equal(0, problems.Count(), "a complete dialogue has no problems");

		// Replies: only what the server offered can be picked.
		LF_DialogueNode greet = LF_DialogueFlow.FindNode(config, "greet");
		array<bool> allowed = {false, true, true};
		int mask = LF_DialogueFlow.OptionMask(greet, allowed);
		ctx.Equal(6, mask, "a reply whose condition fails is not in the mask");
		ctx.True(LF_DialogueFlow.Pick(greet, mask, 0) == null, "a reply that was not offered cannot be picked");
		LF_DialogueOption picked = LF_DialogueFlow.Pick(greet, mask, 1);
		ctx.True(picked != null && picked.m_sNext == "menu", "an offered reply is picked and leads on");
		ctx.True(LF_DialogueFlow.Pick(greet, mask, 3) == null, "an index past the node's replies is refused");
		ctx.True(LF_DialogueFlow.Pick(greet, mask, -1) == null, "a negative index is refused");
		ctx.True(LF_DialogueFlow.Pick(greet, 255, 7) == null, "an index past the screen's rows is refused whatever the mask");
		array<bool> few = {true};
		ctx.Equal(1, LF_DialogueFlow.OptionMask(greet, few), "a reply with no verdict is hidden");
		ctx.Equal(0, LF_DialogueFlow.OptionMask(LF_DialogueFlow.FindNode(config, "bye"), allowed), "a node with no replies offers none");

		// What the NPC says: the action's keys first, each by the line voiced for it, then the node's line.
		ctx.EqualStr("register", LF_DialogueFlow.SpokenFor(config, "#Quest_TurnIn"), "a key a feature hands back is spoken by the line voiced for it");
		ctx.EqualStr("#Quest_Other", LF_DialogueFlow.SpokenFor(config, "#Quest_Other"), "a key with no voiced line stays a caption");
		array<string> keys = {"#Quest_TurnIn", "", "#Quest_Other"};
		ctx.EqualStr("register;#Quest_Other;bye", LF_DialogueFlow.SpokenList(config, LF_DialogueFlow.FindNode(config, "bye"), keys), "the action's answer comes before the node's own line");
		ctx.EqualStr("", LF_DialogueFlow.SpokenList(config, LF_DialogueFlow.FindNode(config, "menu"), null), "a hub node says nothing");

		// Broken data is named, not played.
		greet.m_aOptions[2].m_sNext = "nowhere";
		problems.Clear();
		LF_DialogueFlow.Problems(config, problems);
		ctx.True(problems.Count() == 1 && problems[0].Contains("nowhere"), "a reply that leads to a missing node is a problem");
		greet.m_aOptions[2].m_sNext = "bye";
		greet.m_aOptions[1].m_sCondition = "A";
		greet.m_aOptions[2].m_sCondition = "B";
		problems.Clear();
		LF_DialogueFlow.Problems(config, problems);
		ctx.True(problems.Count() == 1 && problems[0].Contains("always offered"), "a node whose every reply is conditional is a problem: the player could be left with none");
		config.m_sStart = "";
		problems.Clear();
		LF_DialogueFlow.Problems(config, problems);
		ctx.Equal(2, problems.Count(), "a missing start node is a problem too");

		// Mouth: the envelope is one digit per 50 ms, interpolated, shut outside the line.
		ctx.EqualFloat(0, LF_DialogueFlow.MouthAt("09", 0), 0.001, "the mouth starts where the envelope starts");
		ctx.EqualFloat(1, LF_DialogueFlow.MouthAt("09", 0.05), 0.001, "a 9 is fully open");
		ctx.EqualFloat(0.5, LF_DialogueFlow.MouthAt("09", 0.025), 0.001, "between two steps the mouth is between them");
		ctx.EqualFloat(0, LF_DialogueFlow.MouthAt("09", 0.2), 0.001, "past the envelope the mouth is shut");
		ctx.EqualFloat(0, LF_DialogueFlow.MouthAt("", 1), 0.001, "no envelope keeps the mouth shut");
		ctx.EqualFloat(0, LF_DialogueFlow.MouthAt("99", -1), 0.001, "before the line the mouth is shut");

		// Captions: a recording holds for its length, a bare caption for reading time.
		ctx.EqualFloat(2.5, LF_DialogueFlow.HoldSeconds(LF_DialogueFlow.FindLine(config, "greet"), 10), 0.001, "a voiced line holds for its recording");
		ctx.EqualFloat(6, LF_DialogueFlow.HoldSeconds(null, 100), 0.001, "a bare caption holds for reading time");
		ctx.EqualFloat(LF_DialogueFlow.SILENT_SECONDS_MIN, LF_DialogueFlow.HoldSeconds(null, 3), 0.001, "a short bare caption still holds long enough to see");

		// Camera: both shots look at the NPC; the face shot is close, the shoulder shot is behind the player.
		vector npcHead = "10 1.7 10";
		vector playerHead = "10 1.7 12";
		vector position;
		vector lookAt;
		float fov;
		vector facingPlayer = "0 0 1";
		vector facingAway = "0 0 -1";
		LF_DialogueCamera.Frame(npcHead, facingPlayer, playerHead, true, position, lookAt, fov);
		ctx.True(vector.Distance(position, npcHead) < 1.3 && vector.Distance(position, npcHead) > 0.8, "the face shot stands about a metre from the NPC's head");
		ctx.True(vector.Distance(lookAt, npcHead) < 0.2, "the face shot looks at the NPC's head");
		ctx.True(vector.Distance(position, npcHead) < vector.Distance(playerHead, npcHead), "in the face shot the player is behind the camera");
		ctx.True(vector.Dot(position - npcHead, facingPlayer) > 0.8, "the face shot stands in front of the NPC's face");
		LF_DialogueCamera.Frame(npcHead, facingAway, playerHead, true, position, lookAt, fov);
		ctx.True(vector.Dot(position - npcHead, facingAway) > 0.8, "a player behind the NPC still gets its face, not the back of its head");
		vector besidePlayer = "13 1.7 10";
		LF_DialogueCamera.Frame(npcHead, facingPlayer, besidePlayer, true, position, lookAt, fov);
		ctx.True(position[0] > npcHead[0], "the face shot leans to the side the player stands on");
		LF_DialogueCamera.Frame(npcHead, facingPlayer, playerHead, false, position, lookAt, fov);
		ctx.True(vector.Distance(position, npcHead) > vector.Distance(playerHead, npcHead), "the shoulder shot stands behind the player");
		ctx.True(vector.Distance(position, playerHead) < 1.1, "the shoulder shot stays at the player's shoulder");
		ctx.True(vector.Distance(position, playerHead) > LF_DialogueCamera.HEAD_KEEP_OUT, "the shoulder shot stands outside the player's head");
		vector side = LF_DialogueCamera.ShoulderSide(npcHead, playerHead);
		vector pushed = LF_DialogueCamera.KeepOutOfHead(playerHead + "0 0.05 0.1", playerHead, side, LF_DialogueCamera.HEAD_KEEP_OUT);
		ctx.EqualFloat(LF_DialogueCamera.HEAD_KEEP_OUT, vector.Distance(pushed, playerHead), 0.01, "a camera inside the head is pushed out to the keep-out sphere");
		ctx.True(vector.Dot(pushed - playerHead, side) >= -0.001, "a camera pushed out of the head leaves on the shoulder side");
		ctx.True(vector.Distance(LF_DialogueCamera.KeepOutOfHead(playerHead + "0 3 0", playerHead, side, LF_DialogueCamera.HEAD_KEEP_OUT), playerHead + "0 3 0") < 0.001, "a camera already clear of the head is left alone");
		LF_DialogueCamera.Frame(npcHead, vector.Zero, npcHead, true, position, lookAt, fov);
		ctx.True(vector.Distance(position, npcHead) > 0.8, "two heads on one spot still give a camera that stands off");
		ctx.EqualFloat(0, LF_DialogueCamera.Ease(0), 0.001, "the ease starts at rest");
		ctx.EqualFloat(1, LF_DialogueCamera.Ease(1), 0.001, "the ease ends on the shot");
		ctx.EqualFloat(1, LF_DialogueCamera.Ease(4), 0.001, "the ease does not overshoot");
		ctx.True(LF_DialogueCamera.Ease(0.1) < 0.1 && LF_DialogueCamera.Ease(0.9) > 0.9, "the ease starts and stops slowly");
	}
}
