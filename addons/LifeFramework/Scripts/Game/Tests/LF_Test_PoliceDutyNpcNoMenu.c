// red-proof: observed 2026-11-17, tools\cli test --no-build --only LF_Test_PoliceDutyNpcNoMenu, on the
// prefab that still carried LF_OpenPoliceMenuAction and the PoliceMenu preset: passed=0 failed=1
// (see the PR body for the exact lines).

//------------------------------------------------------------------------------------------------
//! The police duty NPC offers no menu (#2535). Duty moved to the duty desk (LF_DutyAction) and the
//! Search and Arrest buttons went in #2509, so the NPC's menu held only Close. The prefab keeps
//! no LF_OpenPoliceMenuAction and the PoliceMenu preset is gone. The arrest RPC stays for the
//! planned wanted-list sheet.
// tier: WORLD
class LF_Test_PoliceDutyNpcNoMenu : LF_Test
{
	static const ResourceName PREFAB = "{05BE7F756ADCA703}Prefabs/Characters/Npcs/PoliceDutyNPC.et";

	override string GetName()
	{
		return "police/duty-npc-no-menu";
	}

	override void Run(LF_TestContext ctx)
	{
		IEntity npc = LF_Utils.SpawnEntityPrefab(PREFAB, "0 -50 0");
		ctx.NotNull(npc, "the police duty NPC prefab still spawns");
		if (npc)
		{
			int menuActions = 0;
			BaseActionsManagerComponent manager = BaseActionsManagerComponent.Cast(npc.FindComponent(BaseActionsManagerComponent));
			if (manager)
			{
				array<BaseUserAction> actions = {};
				manager.GetActionsList(actions);
				foreach (BaseUserAction action : actions)
				{
					if (action && action.ClassName() == "LF_OpenPoliceMenuAction")
						menuActions++;
				}
			}

			ctx.Equal(0, menuActions, "the police duty NPC carries no LF_OpenPoliceMenuAction");
			SCR_EntityHelper.DeleteEntityAndChildren(npc);
		}

		ctx.Equal(-1, typename.StringToEnum(ChimeraMenuPreset, "PoliceMenu"), "the PoliceMenu menu preset is gone");
	}
};
