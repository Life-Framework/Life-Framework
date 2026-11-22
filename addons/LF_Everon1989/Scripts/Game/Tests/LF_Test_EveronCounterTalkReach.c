// red-proof: observed with `tools\cli test --no-build --red LF_Test_EveronCounterTalkReach` (RedProof() skips the spawns, so every `<npc> spawns` fails); before this change the teller, store clerk, cafe owner and night-shift clerk talk spheres were 0.6 m, so `the talk sphere reaches across a counter` failed.
//! The pack's counter staff (bank teller, store clerk, cafe owner, night-shift clerk) have a talk
//! sphere that reaches a customer across their counter (about 2.4 m). The core twin is
//! LF_Test_CounterNpcTalkReach.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronCounterTalkReach : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/counter-talk-reach";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<ResourceName> prefabs = {
			"{7C27C5AD4C1C5D2D}Configs/Residents/Npcs/LF_Resident_Teller.et",
			"{559ECE12ABB3673F}Configs/Residents/Npcs/LF_Resident_StoreClerk.et",
			"{83D04234954B8914}Configs/Npcs/LF_Npc_Townsfolk_CafeOwner.et"
		};

		foreach (ResourceName prefab : prefabs)
		{
			IEntity npc;
			if (!RedProof())
				npc = LF_Utils.SpawnEntityPrefab(prefab, "0 -50 0");
			ctx.True(npc != null, "the counter NPC spawns: " + prefab);
			if (!npc)
				continue;

			float radius = LF_Test_CounterNpcTalkReach.TalkRadius(npc);
			ctx.True(radius >= LF_Test_CounterNpcTalkReach.MIN_REACH && radius <= LF_Test_CounterNpcTalkReach.MAX_REACH, "the talk sphere reaches across a counter: " + prefab + " radius " + radius);
			SCR_EntityHelper.DeleteEntityAndChildren(npc);
		}
	}
}
