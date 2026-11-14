// red-proof: observed 2026-11-14 with `tools\cli test --no-build --only LF_Test_SitWorld`: passing a zero offset in
// LF_SitAction.LF_GetSeatTransform failed it with `- ...LF_Seat_BenchStreet_01_green.et sit place is on the seat, not away
// from it (0 not in [0.05, 1.2])` and `- ... two places do not overlap`. Restored; green.
// Disabling the index guard in the modded SCR_CommandingManagerComponent.FindCommandNameFromIndex stopped the run with
// `Virtual Machine Exception Reason: Index out of bounds.` at `a command index past the list is refused`. Restored; green.
// Before the modded command classes repeated [BaseContainerProps()], every `- radial entry CMD_Salute is a base game
// command` (and the other nine) failed. Fixed; green.
// tier: WORLD
//! emote/sit-world (#982): every LF seat prefab offers one Sit per place with the chair pose, each
//! place in its own spot; the game mode carries the base game commanding manager, and every entry of
//! the LF emote radial resolves to a base game command that runs with no group.
class LF_Test_SitWorld : LF_Test
{
	protected static const ResourceName MENU = "{AC9E96BADCDA8796}Configs/Emote/LF_EmoteMenu.conf";
	protected static const ResourceName MENU_ACTIONS = "{19D56867FAFF944C}Configs/Emote/LF_EmoteMenuActions.conf";

	override string GetName()
	{
		return "emote/sit-world";
	}

	override void Run(LF_TestContext ctx)
	{
		RunSeat(ctx, "{BE839ACB6CB2726A}Prefabs/Props/Seats/LF_Seat_BenchStreet_01_green.et", 2);
		RunSeat(ctx, "{24388E37E11BA18F}Prefabs/Props/Seats/LF_Seat_BenchStreet_01_red.et", 2);
		RunSeat(ctx, "{CECA6D57E2CAB7A8}Prefabs/Props/Seats/LF_Seat_BenchStreet_02.et", 2);
		RunSeat(ctx, "{69D6D9BFDFEC3225}Prefabs/Props/Seats/LF_Seat_Chair_02_green.et", 1);
		RunRadial(ctx);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunSeat(LF_TestContext ctx, ResourceName prefab, int places)
	{
		Resource res = Resource.Load(prefab);
		ctx.True(res.IsValid(), prefab + " loads");
		if (!res.IsValid())
			return;

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = "5 0 5";
		IEntity seat = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		ctx.NotNull(seat, prefab + " spawns");
		if (!seat)
			return;

		BaseActionsManagerComponent manager = BaseActionsManagerComponent.Cast(seat.FindComponent(BaseActionsManagerComponent));
		ctx.NotNull(manager, prefab + " has an actions manager");
		if (manager)
		{
			array<BaseUserAction> actions = {};
			manager.GetActionsList(actions);
			array<vector> spots = {};
			foreach (BaseUserAction action : actions)
			{
				LF_SitAction sit = LF_SitAction.Cast(action);
				if (!sit)
					continue;

				ctx.True(sit.LF_HasPose(), prefab + " sit carries the chair pose");
				vector at[4];
				sit.LF_GetSeatTransform(at);
				ctx.InRange(vector.DistanceXZ(at[3], seat.GetOrigin()), 0.05, 1.2, prefab + " sit place is on the seat, not away from it");
				foreach (vector other : spots)
				{
					ctx.True(vector.DistanceXZ(at[3], other) > 0.5, prefab + " two places do not overlap");
				}

				spots.Insert(at[3]);
			}

			ctx.Equal(places, spots.Count(), prefab + " offers one Sit per place");
		}

		SCR_EntityHelper.DeleteEntityAndChildren(seat);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunRadial(LF_TestContext ctx)
	{
		SCR_CommandingManagerComponent commanding = SCR_CommandingManagerComponent.GetInstance();
		ctx.NotNull(commanding, "the game mode carries SCR_CommandingManagerComponent");
		if (!commanding)
			return;

		// A client sends the command index (RPC_RequestExecuteCommand); a bad one is refused, not read.
		ctx.True(commanding.FindCommandIndex("CMD_Salute") >= 0, "the game mode knows the emote commands");
		ctx.EqualStr(string.Empty, commanding.FindCommandNameFromIndex(9999), "a command index past the list is refused");
		ctx.EqualStr(string.Empty, commanding.FindCommandNameFromIndex(-1), "a negative command index is refused");

		SCR_PlayerCommandingMenuActionsSetup pairs = SCR_PlayerCommandingMenuActionsSetup.Cast(LoadConfig(MENU_ACTIONS));
		ctx.NotNull(pairs, "the emote input pair config loads");
		if (pairs)
		{
			ctx.Equal(1, pairs.m_aActionConfigPairs.Count(), "one input opens one menu");
			if (pairs.m_aActionConfigPairs.Count() == 1)
			{
				ctx.EqualStr("LF_OpenEmoteMenu", pairs.m_aActionConfigPairs[0].GetActionName(), "the emote radial opens on LF_OpenEmoteMenu");
				ctx.EqualStr(MENU, pairs.m_aActionConfigPairs[0].GetConfig(), "the input opens the LF emote menu");
			}
		}

		SCR_PlayerCommandingMenuConfig menu = SCR_PlayerCommandingMenuConfig.Cast(LoadConfig(MENU));
		ctx.NotNull(menu, "the emote menu config loads");
		if (!menu)
			return;

		array<string> names = {};
		CollectCommands(menu.GetRootCategory(), names);
		ctx.True(names.Count() >= 8, "the radial offers gestures and held emotes");

		foreach (string name : names)
		{
			SCR_BaseRadialCommand command = commanding.FindCommand(name);
			ctx.NotNull(command, "radial entry " + name + " is a base game command");
			ctx.True(LF_Emote.IsGroupFree(command), "radial entry " + name + " runs with no group");
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void CollectCommands(SCR_PlayerCommandingMenuCategoryElement category, notnull array<string> names)
	{
		if (!category)
			return;

		foreach (SCR_PlayerCommandingMenuBaseElement element : category.GetCategoryElements())
		{
			SCR_PlayerCommandingMenuCommand command = SCR_PlayerCommandingMenuCommand.Cast(element);
			if (command)
			{
				names.Insert(command.GetCommandName());
				continue;
			}

			CollectCommands(SCR_PlayerCommandingMenuCategoryElement.Cast(element), names);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected Managed LoadConfig(ResourceName path)
	{
		Resource holder = BaseContainerTools.LoadContainer(path);
		if (!holder || !holder.IsValid())
			return null;

		return BaseContainerTools.CreateInstanceFromContainer(holder.GetResource().ToBaseContainer());
	}
}
