//------------------------------------------------------------------------------------------------
//! Harness stand-ins for the emote and sit proof (#982, agent-box/proofs/emote-sit-proof.mjs). The
//! harness cannot hold a key, so:
//!   emote open      (client) opens the emote radial the way holding F1 does: the base game commanding
//!                   component loads the menu its input pair names and asks the radial to open
//!   emote close     (client) closes it
//!   emote <command> (client) runs one radial entry (CMD_Salute, Emote_SitGround, ...) through the base
//!                   game PrepareExecuteCommand, as a click on that entry does
//!   emote sit       (client) performs the nearest Sit within 3 m, or this body's Stand up, through the
//!                   character's ActionsPerformerComponent (the call the F key ends in), so the base game
//!                   perform, the server's seat data and the broadcast all run
//!   emote seen      (client) what this machine shows of every other player: gesture, loiter and type
//! Everything after is the real path: the base game gesture or loiter and its replication.
//------------------------------------------------------------------------------------------------
class LF_EmoteHarness
{
	//------------------------------------------------------------------------------------------------
	static string Run(string arg)
	{
		if (arg == "seen")
			return Seen();

		if (arg == "sit")
			return Sit();

		SCR_PlayerControllerCommandingComponent commanding = SCR_PlayerControllerCommandingComponent.GetLocalPlayerControllerCommandingComponent();
		if (!commanding)
			return "error no_commanding_component";

		if (arg == "open")
			return commanding.LF_HarnessOpenMenu();

		if (arg == "close")
			return commanding.LF_HarnessCloseMenu();

		SCR_CommandingManagerComponent manager = SCR_CommandingManagerComponent.GetInstance();
		if (!manager || !manager.FindCommand(arg))
			return "error unknown_command " + arg;

		commanding.PrepareExecuteCommand(arg);
		return "ok ran " + arg;
	}

	//------------------------------------------------------------------------------------------------
	static string Sit()
	{
		SCR_ChimeraCharacter character = LF_Utils.GetLocalCharacter();
		if (!character)
			return "error no_character";

		ActionsPerformerComponent performer = ActionsPerformerComponent.Cast(character.FindComponent(ActionsPerformerComponent));
		if (!performer)
			return "error no_actions_performer";

		LF_EmoteHarnessQuery query = new LF_EmoteHarnessQuery();
		GetGame().GetWorld().QueryEntitiesBySphere(character.GetOrigin(), 3, query.Add, null, EQueryEntitiesFlags.ALL);
		LF_SitAction best;
		float bestDistance = 3;
		foreach (IEntity seat : query.m_aFound)
		{
			BaseActionsManagerComponent manager = BaseActionsManagerComponent.Cast(seat.FindComponent(BaseActionsManagerComponent));
			if (!manager)
				continue;

			array<BaseUserAction> actions = {};
			manager.GetActionsList(actions);
			foreach (BaseUserAction action : actions)
			{
				LF_SitAction sit = LF_SitAction.Cast(action);
				if (!sit || !sit.CanBePerformedScript(character))
					continue;

				if (sit.IsOccupiedBy(character))
					return Perform(performer, sit);

				vector at[4];
				sit.LF_GetSeatTransform(at);
				float distance = vector.Distance(at[3], character.GetOrigin());
				if (distance < bestDistance)
				{
					best = sit;
					bestDistance = distance;
				}
			}
		}

		if (!best)
			return "error no_free_seat_within_3m";

		return Perform(performer, best);
	}

	//------------------------------------------------------------------------------------------------
	protected static string Perform(ActionsPerformerComponent performer, LF_SitAction sit)
	{
		performer.PerformAction(sit);
		return string.Format("ok performed on %1", sit.GetOwner().GetName());
	}

	//------------------------------------------------------------------------------------------------
	static string Seen()
	{
		PlayerManager players = GetGame().GetPlayerManager();
		IEntity self = LF_Utils.GetLocalCharacter();
		array<int> ids = {};
		players.GetPlayers(ids);
		string text = "ok";
		foreach (int id : ids)
		{
			SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(players.GetPlayerControlledEntity(id));
			if (!character || character == self)
				continue;

			SCR_CharacterControllerComponent controller = SCR_CharacterControllerComponent.Cast(character.GetCharacterController());
			if (!controller)
				continue;

			int loiterType = -1;
			SCR_CharacterCommandHandlerComponent handler = SCR_CharacterCommandHandlerComponent.Cast(controller.GetAnimationComponent().GetCommandHandler());
			ELoiteringType current;
			if (handler && handler.IsLoitering(current))
				loiterType = current;

			text += string.Format(" p%1:gesture=%2,loiter=%3,at=%4", id, controller.IsPlayingGesture(), loiterType, character.GetOrigin());
		}

		return text;
	}
}

//------------------------------------------------------------------------------------------------
class LF_EmoteHarnessQuery
{
	ref array<IEntity> m_aFound = {};

	//------------------------------------------------------------------------------------------------
	bool Add(IEntity entity)
	{
		m_aFound.Insert(entity);
		return true;
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_PlayerControllerCommandingComponent
{
	//------------------------------------------------------------------------------------------------
	//! Harness: what OpenCommandingMenu does for the first input pair, without the key.
	string LF_HarnessOpenMenu()
	{
		if (!m_CommandingMenuPairsConfig || m_CommandingMenuPairsConfig.m_aActionConfigPairs.IsEmpty())
			return "error no_menu_pairs";

		Resource holder = BaseContainerTools.LoadContainer(m_CommandingMenuPairsConfig.m_aActionConfigPairs[0].GetConfig());
		if (!holder || !holder.IsValid())
			return "error menu_config_missing";

		m_CommandingMenuConfig = SCR_PlayerCommandingMenuConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(holder.GetResource().ToBaseContainer()));
		if (!m_CommandingMenuConfig || !m_RadialMenuController)
			return "error no_menu";

		m_RadialMenuController.OnInputOpen();
		return string.Format("ok open=%1 action=%2", m_RadialMenuController.IsMenuOpen(), m_CommandingMenuPairsConfig.m_aActionConfigPairs[0].GetActionName());
	}

	//------------------------------------------------------------------------------------------------
	string LF_HarnessCloseMenu()
	{
		bool wasOpen = m_RadialMenuController && m_RadialMenuController.IsMenuOpen();
		if (wasOpen)
			m_RadialMenuController.CloseMenu();

		return string.Format("ok closed wasOpen=%1", wasOpen);
	}
}
