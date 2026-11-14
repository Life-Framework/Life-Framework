// red-proof: Observed 2026-11-08 with `tools\cli test --no-build --only LF_Test_NpcInteractionHeight`
// before the prefab fix: `[LFTEST] FAIL npc/interaction-height`,
// `- the interaction sits at chest height: {DE27E685D6FE93D7}Prefabs/Characters/Quests/QuestGiver_Clerk.et context Quest at 0.5 m, radius 2`
// (the same for QuestGiver_Start), and PoliceDutyNPC.et had no actions manager: its second
// CharacterIdentityComponent failed the prefab ("cannot be combined with component
// CharacterIdentityComponent"). Green after the fix.
// tier: WORLD
//! A talk-to NPC's action context sits on its chest, where the player looks, not at the hips
//! (playtest 2026-11-08: "between their legs"). The prefabs had Offset 0 0.5 0 with Radius 2.0;
//! they now pin the context to the Spine5 bone as vanilla Character_Base does for "Chest".
class LF_Test_NpcInteractionHeight : LF_Test
{
	override string GetName()
	{
		return "npc/interaction-height";
	}

	override void Run(LF_TestContext ctx)
	{
		array<ResourceName> prefabs = {
			"{DE27E685D6FE93D7}Prefabs/Characters/Quests/QuestGiver_Clerk.et",
			"{E8989175335080B8}Prefabs/Characters/Quests/QuestGiver_Start.et",
			"{05BE7F756ADCA703}Prefabs/Characters/Npcs/PoliceDutyNPC.et"
		};

		foreach (ResourceName prefab : prefabs)
		{
			IEntity npc = LF_Utils.SpawnEntityPrefab(prefab, "0 -50 0");
			ctx.True(npc != null, "the NPC spawns: " + prefab);
			if (!npc)
				continue;

			BaseActionsManagerComponent manager = BaseActionsManagerComponent.Cast(npc.FindComponent(BaseActionsManagerComponent));
			ctx.True(manager != null, "the NPC has an actions manager: " + prefab);
			if (manager)
			{
				array<UserActionContext> contexts = {};
				manager.GetContextList(contexts);
				int withActions = 0;
				foreach (UserActionContext context : contexts)
				{
					if (context.GetActionsCount() == 0)
						continue;
					withActions++;
					vector mat[4];
					context.GetTransformationModel(mat);
					float height = mat[3][1];
					string label = prefab + " context " + context.GetContextName() + " at " + height + " m, radius " + context.GetRadius();
					ctx.True(height > 1.1 && height < 1.7, "the interaction sits at chest height: " + label);
					// RoleTalk is the role conversation's comfortable reach (about 2.25 m); only
					// an NPC with a role offers it. Every other context stays on the body.
					float maxRadius = 1.0;
					if (context.GetContextName() == "RoleTalk")
						maxRadius = 2.5;
					ctx.True(context.GetRadius() <= maxRadius, "the interaction radius stays on the body: " + label);
				}
				ctx.True(withActions > 0, "the NPC offers an interaction: " + prefab);
			}

			SCR_EntityHelper.DeleteEntityAndChildren(npc);
		}
	}
}
