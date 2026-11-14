// red-proof: Observed 2026-11-14 with `tools\cli test --no-build --only LF_Test_RolesWorld`: the police outfit first used the full
// Jacket_M88, which attaches a belt dummy the NPC storage lacks; `- ...LF_Npc_Police_1.et holds every worn item in its loadout storage
// (expected 5, got 4)`. Fixed with Jacket_M88_base; green.
// tier: WORLD
//! NPC roles in the world: every outfit prefab spawns dressed (jacket, pants and boots worn, held in the
//! NPC's loadout storage), carries its role, and offers its role conversation with its strings in the
//! table; the variants of a role wear different clothes; every placed town NPC has a role.
class LF_Test_RolesWorld : LF_Test
{
	protected ref array<IEntity> m_aFound = {};

	override string GetName()
	{
		return "npc/roles-world";
	}

	protected bool InsertFound(IEntity entity)
	{
		m_aFound.Insert(entity);
		return true;
	}

	protected bool FilterNpc(IEntity entity)
	{
		return entity && LF_NpcBaseEntity.Cast(entity) != null && entity.FindComponent(BaseLoadoutManagerComponent) != null;
	}

	//! The worn clothing of an NPC as one string, one prefab name per piece.
	protected string Worn(IEntity npc, out int count, out int stored)
	{
		string names;
		IEntity child = npc.GetChildren();
		while (child)
		{
			if (child.FindComponent(BaseLoadoutClothComponent))
			{
				count++;
				names += LF_Utils.GetPrefabName(child) + " ";
				InventoryItemComponent item = InventoryItemComponent.Cast(child.FindComponent(InventoryItemComponent));
				if (item && item.GetParentSlot() && EquipedLoadoutStorageComponent.Cast(item.GetParentSlot().GetStorage()))
					stored++;
			}

			child = child.GetSibling();
		}

		return names;
	}

	override void Run(LF_TestContext ctx)
	{
		LF_Roles.ResetStatics();
		foreach (LF_RoleDef role : LF_Roles.All())
		{
			array<string> signatures = {};
			foreach (string prefab : role.m_aOutfits)
			{
				IEntity npc = LF_Utils.SpawnEntityPrefab(prefab, "0 -50 0");
				ctx.True(npc != null, role.m_sId + " outfit spawns: " + prefab);
				if (!npc)
					continue;

				ctx.EqualStr(role.m_sId, LF_Roles.RoleOf(npc), prefab + " carries its role");
				int count;
				int stored;
				string worn = Worn(npc, count, stored);
				ctx.True(count >= 3, prefab + " is dressed (wears " + count + ": " + worn + ")");
				ctx.Equal(count, stored, prefab + " holds every worn item in its loadout storage (" + worn + ")");
				if (signatures.Find(worn) == -1)
					signatures.Insert(worn);

				if (role.m_bOwnDialogue)
				{
					ctx.True(LF_Component<LF_DialogueComponent>.Find(npc) != null, prefab + " keeps its authored dialogue");
				}
				else
				{
					LF_DialogueConfig config = LF_DialogueComponent.ConfigOf(npc);
					ctx.True(config != null, prefab + " offers its role conversation");
					if (config)
					{
						foreach (LF_DialogueNode node : config.m_aNodes)
						{
							if (!node.m_sLine.IsEmpty())
								ctx.True(WidgetManager.Translate(node.m_sLine) != node.m_sLine, node.m_sLine + " is in the string table");

							foreach (LF_DialogueOption option : node.m_aOptions)
								ctx.True(WidgetManager.Translate(option.m_sTextKey) != option.m_sTextKey, option.m_sTextKey + " is in the string table");
						}
					}
				}

				SCR_EntityHelper.DeleteEntityAndChildren(npc);
			}

			// Outfits of one base differ; the civilian twin of an outfit wears the same clothes.
			ctx.True(signatures.Count() >= role.m_aPrimary.Count(), role.m_sId + " outfits wear different clothes");
		}

		m_aFound = new array<IEntity>();
		GetGame().GetWorld().QueryEntitiesBySphere("200 0 250", 400, InsertFound, FilterNpc);
		array<IEntity> placed = {};
		placed.Copy(m_aFound);
		ctx.True(placed.Count() > 0, "the town places NPCs");
		foreach (IEntity placedNpc : placed)
		{
			// A quest giver with no job to dress for is the only NPC left without a role.
			if (LF_Component<LF_QuestGiverComponent>.Find(placedNpc) && LF_Roles.RoleOf(placedNpc).IsEmpty())
				continue;

			ctx.True(!LF_Roles.RoleOf(placedNpc).IsEmpty(), "placed NPC " + placedNpc.GetName() + " has a role");
		}

		LF_Roles.ResetStatics();
	}
}
