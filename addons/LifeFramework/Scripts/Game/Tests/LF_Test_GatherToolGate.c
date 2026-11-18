// red-proof: 2026-11-17, `--only LF_Test_GatherToolGate` before the change: LF_GatherTools, LF_GatherToolRow, m_aTools and LF_OnGathered did not exist, so the test could not compile. `--red LF_Test_GatherToolGate` skips the better-tool row and the publish, so the speed and event checks fail.
//! gather/tool-gate (#2706): a gather node that names a tool refuses without it, in words and with no
//! number; the right tool passes; a better tool carries a speed factor that shortens the hold; a
//! finished gather publishes LF_OnGathered with the job family.
class LF_TestGatherToolRecorder : LF_EventSubscriber
{
	ref array<ref LF_OnGathered> m_aSeen = {};

	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnGathered gathered = LF_OnGathered.Cast(busEvent);
		if (gathered)
			m_aSeen.Insert(gathered);
		return true;
	}
}

[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_GatherToolGate : LF_Test
{
	protected const string SHOVEL = "{0619993AE6CDBF85}Prefabs/Items/Equipment/Accessories/ETool_ALICE/ETool_ALICE.et";
	protected const string SPADE = "Prefabs/Tools/Spade_Steel.et";
	protected const string PICKAXE = "Prefabs/Tools/Pickaxe.et";

	override string GetName()
	{
		return "gather/tool-gate";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_ResourceNodeDef sand = new LF_ResourceNodeDef();
		sand.m_sNodeId = "sand";
		sand.m_ToolPrefab = SHOVEL;
		sand.m_sJobFamily = LF_GatherTools.FAMILY_DIGGING;
		sand.m_aTools = {};
		if (!RedProof())
		{
			LF_GatherToolRow better = new LF_GatherToolRow();
			better.m_ToolPrefab = SPADE;
			better.m_fSpeed = 1.5;
			sand.m_aTools.Insert(better);
		}

		// No tool: refused, with a localization key that is words only.
		ctx.True(LF_GatherTools.NeedsTool(sand), "a node that names a tool needs one");
		ctx.False(LF_GatherTools.Accepts(sand, ResourceName.Empty), "empty hands do not pass");
		ctx.False(LF_GatherTools.Accepts(sand, PICKAXE), "a pickaxe does not dig sand");
		string key = LF_GatherTools.RefusalKey(sand, false);
		ctx.True(key.StartsWith("#"), "the refusal is a # localization key");
		ctx.False(HasDigit(key), "the refusal key carries no digit");
		ctx.EqualStr("#LF-Gather_NeedShovel", key, "digging asks for a shovel");

		LF_ResourceNodeDef rock = new LF_ResourceNodeDef();
		rock.m_ToolPrefab = PICKAXE;
		ctx.EqualStr("#LF-Gather_NeedPickaxe", LF_GatherTools.RefusalKey(rock, false), "a rock asks for a pickaxe");
		ctx.False(HasDigit(LF_GatherTools.RefusalKey(rock, false)), "the pickaxe key carries no digit");

		// The right tool passes at the base speed; a free node needs nothing.
		ctx.True(LF_GatherTools.Accepts(sand, SHOVEL), "the shovel digs");
		ctx.EqualFloat(1.0, LF_GatherTools.Speed(sand, SHOVEL), 0.001, "the base tool works at speed 1");
		LF_ResourceNodeDef apple = new LF_ResourceNodeDef();
		ctx.False(LF_GatherTools.NeedsTool(apple), "an apple needs no tool");
		ctx.True(LF_GatherTools.Accepts(apple, ResourceName.Empty), "bare hands pick apples");

		// A better tool: its factor shortens the hold.
		ctx.True(LF_GatherTools.Accepts(sand, SPADE), "the better tool is accepted");
		float speed = LF_GatherTools.Speed(sand, SPADE);
		ctx.EqualFloat(1.5, speed, 0.001, "the better tool carries its speed factor");
		float baseTime = LF_GatherTools.Duration(3.0, 1.0);
		float fastTime = LF_GatherTools.Duration(3.0, speed);
		ctx.True(fastTime < baseTime, "a faster tool shortens the action time");
		ctx.EqualFloat(2.0, fastTime, 0.001, "speed 1.5 turns a 3 s hold into 2 s");
		ctx.True(LF_GatherTools.Duration(3.0, 0) < 100, "a zero speed never divides by zero");

		// A finished gather publishes the event with the job family.
		LF_EventBus.ResetStatics();
		LF_TestGatherToolRecorder recorder = new LF_TestGatherToolRecorder();
		LF_EventBus.Subscribe(LF_OnGathered, recorder);
		if (!RedProof())
			LF_EventBus.Publish(LF_OnGathered.Create("uid-1", sand.m_sNodeId, LF_GatherTools.JobFamily(sand), "Prefabs/Items/Resources/Sandbag.et", 1, speed));
		ctx.Equal(1, recorder.m_aSeen.Count(), "one gather publishes one LF_OnGathered");
		if (recorder.m_aSeen.Count() > 0)
		{
			ctx.EqualStr(LF_GatherTools.FAMILY_DIGGING, recorder.m_aSeen[0].m_sJobFamily, "the event carries the job family");
			ctx.EqualStr("sand", recorder.m_aSeen[0].m_sNodeId, "the event names the node");
			ctx.EqualFloat(1.5, recorder.m_aSeen[0].m_fToolSpeed, 0.001, "the event carries the tool speed");
		}
		LF_EventBus.ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	protected bool HasDigit(string text)
	{
		for (int i = 0; i < text.Length(); i++)
		{
			int c = text.ToAscii(i);
			if (c >= 48 && c <= 57)
				return true;
		}
		return false;
	}
};
