// red-proof: observed with `tools\cli test --no-build --red LF_Test_CounterNpcTalkReach` (RedProof() skips the spawns, so every `<npc> spawns` fails); on main before this change the shopkeeper, fuel clerk and town clerk talk spheres were 0.6 m on the spine, so `the talk sphere reaches across a counter` failed with `radius 0.6`.
//! A clerk behind a counter can be talked to from the customer side (live play 2026-11-22: "you
//! couldn't really talk to the clerk across the counter"). The core counter staff carry a talk sphere
//! of about 2.4 m on the chest, the prompt shows from 2.1 m (across a counter) on the same level and
//! not from another storey or beyond the 3 m show range.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_CounterNpcTalkReach : LF_Test
{
	static const float MIN_REACH = 2.0;
	static const float MAX_REACH = 2.5;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "npc/counter-talk-reach";
	}

	//------------------------------------------------------------------------------------------------
	//! The largest radius among the NPC's Talk and Quest contexts that hold an action, 0 when none.
	static float TalkRadius(IEntity npc)
	{
		BaseActionsManagerComponent manager = BaseActionsManagerComponent.Cast(npc.FindComponent(BaseActionsManagerComponent));
		if (!manager)
			return 0;

		array<UserActionContext> contexts = {};
		manager.GetContextList(contexts);
		float best;
		foreach (UserActionContext context : contexts)
		{
			string name = context.GetContextName();
			if ((name == "Talk" || name == "Quest") && context.GetActionsCount() > 0)
				best = Math.Max(best, context.GetRadius());
		}

		return best;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<ResourceName> prefabs = {
			"{AB73D0036F42E73E}Prefabs/Characters/Npcs/LF_Shopkeeper.et",
			"{9FF2685A41D86574}Prefabs/Characters/Npcs/LF_FuelClerk.et",
			"{DE27E685D6FE93D7}Prefabs/Characters/Quests/QuestGiver_Clerk.et"
		};

		IEntity customer;
		if (!RedProof())
			customer = LF_Utils.SpawnEntityPrefab(prefabs[0], "2.1 -50 0");
		ctx.True(customer != null, "the customer stand-in spawns");
		if (!customer)
			return;

		foreach (ResourceName prefab : prefabs)
		{
			IEntity npc;
			if (!RedProof())
				npc = LF_Utils.SpawnEntityPrefab(prefab, "0 -50 0");
			ctx.True(npc != null, "the counter NPC spawns: " + prefab);
			if (!npc)
				continue;

			float radius = TalkRadius(npc);
			ctx.True(radius >= MIN_REACH && radius <= MAX_REACH, "the talk sphere reaches across a counter: " + prefab + " radius " + radius);

			// The prompt of the dialogue talk action (the quest action shares the same range rules).
			BaseActionsManagerComponent manager = BaseActionsManagerComponent.Cast(npc.FindComponent(BaseActionsManagerComponent));
			if (manager)
			{
				array<BaseUserAction> actions = {};
				manager.GetActionsList(actions);
				foreach (BaseUserAction action : actions)
				{
					LF_DialogueTalkAction talk = LF_DialogueTalkAction.Cast(action);
					if (!talk)
						continue;

					ctx.True(talk.CanBeShownScript(customer), "the talk prompt shows from 2.1 m across a counter: " + prefab);
					customer.SetOrigin("2.1 -47 0");
					ctx.False(talk.CanBeShownScript(customer), "the talk prompt does not show from another storey: " + prefab);
					customer.SetOrigin("3.6 -50 0");
					ctx.False(talk.CanBeShownScript(customer), "the talk prompt does not show beyond 3 m: " + prefab);
					customer.SetOrigin("2.1 -50 0");
				}
			}

			SCR_EntityHelper.DeleteEntityAndChildren(npc);
		}

		SCR_EntityHelper.DeleteEntityAndChildren(customer);
	}
}
