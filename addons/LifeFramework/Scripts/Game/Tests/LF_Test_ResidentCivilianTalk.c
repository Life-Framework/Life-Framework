// red-proof: RedProof() skips the spawn, so `the default resident spawns` fails; before #2998 the default resident prefab (Npc_Civilian_01) had no dialogue component or talk action.
//! #2998 (residents you can talk to): the neutral civilian a resident spawns as when their config names no prefab
//! carries the core `resident` dialogue and a talk action, so a core-only server's residents can be spoken to.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_ResidentCivilianTalk : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "residents/civilian-talk";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		IEntity npc;
		if (!RedProof())
			npc = LF_Utils.SpawnEntityPrefab(LF_ResidentSpawner.DEFAULT_PREFAB, "0 -50 0");

		ctx.True(npc != null, "the default resident spawns");
		if (!npc)
			return;

		ctx.NotNull(npc.FindComponent(LF_DialogueComponent), "the default resident has a dialogue component");
		LF_DialogueConfig config = LF_DialogueComponent.ConfigOf(npc);
		ctx.NotNull(config, "the default resident has a playable dialogue");
		if (config)
			ctx.EqualStr("resident", config.m_sId, "the default resident talks with the core resident dialogue");

		bool talk;
		BaseActionsManagerComponent manager = BaseActionsManagerComponent.Cast(npc.FindComponent(BaseActionsManagerComponent));
		if (manager)
		{
			array<BaseUserAction> actions = {};
			manager.GetActionsList(actions);
			foreach (BaseUserAction action : actions)
			{
				if (LF_DialogueTalkAction.Cast(action))
					talk = true;
			}
		}

		ctx.True(talk, "the default resident has a talk action");
		SCR_EntityHelper.DeleteEntityAndChildren(npc);
	}
}
