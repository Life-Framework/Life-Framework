// red-proof: Observed 2026-11-08 with `tools\cli test --no-build --only EL_Test_NpcClothesReplicate`
// before Npc_Base.et carried an RplComponent: `[ELTEST] FAIL npc/clothes-replicate`,
// `- the NPC replicates, so its worn clothes reach clients: {850859F356F5058E}Prefabs/Characters/Npcs/Npc_Default_01.et`
// (the worn-count assertions passed: the server dresses the NPC; the client never saw it).
// Green after the fix.
// tier: WORLD
//! Station NPCs (Npc_Base.et) wear clothes that BaseLoadoutManagerComponent spawns on the server.
//! Those clothes carry an RplComponent, so a client only sees them when the NPC is replicated too
//! (Runtime override + Hierarchy, as vanilla Character_Base): without it the dedicated client
//! renders the NPC in underwear (#150). The dedicated-client screenshots in the PR are the proof
//! of the client side; this guards the server-side half cheaply.
class EL_Test_NpcClothesReplicate : EL_Test
{
	override string GetName()
	{
		return "npc/clothes-replicate";
	}

	override void Run(EL_TestContext ctx)
	{
		array<ResourceName> prefabs = {
			"{850859F356F5058E}Prefabs/Characters/Npcs/Npc_Default_01.et",
			"{E8989175335080B8}Prefabs/Characters/Quests/QuestGiver_Start.et",
			"{DE27E685D6FE93D7}Prefabs/Characters/Quests/QuestGiver_Clerk.et"
		};

		foreach (ResourceName prefab : prefabs)
		{
			IEntity npc = EL_Utils.SpawnEntityPrefab(prefab, "0 -50 0");
			ctx.True(npc != null, "the NPC spawns: " + prefab);
			if (!npc)
				continue;

			ctx.True(npc.FindComponent(RplComponent) != null, "the NPC replicates, so its worn clothes reach clients: " + prefab);

			int worn = 0;
			int wornReplicated = 0;
			IEntity child = npc.GetChildren();
			while (child)
			{
				if (child.FindComponent(BaseLoadoutClothComponent))
				{
					worn++;
					if (child.FindComponent(RplComponent))
						wornReplicated++;
				}
				child = child.GetSibling();
			}
			ctx.True(worn >= 3, "the NPC wears its loadout (jacket, pants, boots): " + prefab + " wears " + worn);
			ctx.Equal(worn, wornReplicated, "every worn item is a replicated item: " + prefab);

			SCR_EntityHelper.DeleteEntityAndChildren(npc);
		}
	}
}
