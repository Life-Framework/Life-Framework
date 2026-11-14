//------------------------------------------------------------------------------------------------
//! Harness stand-ins for the emote and sit proof (#982, agent-box/proofs/emote-sit-proof.mjs). The
//! harness cannot hold a key, so:
//!   emote open      (client) opens the emote radial the way holding F1 does: the base game commanding
//!                   component loads the menu its input pair names and asks the radial to open
//!   emote close     (client) closes it
//!   emote <command> (client) runs one radial entry (CMD_Salute, Emote_SitGround, ...) through the base
//!                   game PrepareExecuteCommand, as a click on that entry does
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
		if (m_RadialMenuController && m_RadialMenuController.IsMenuOpen())
			m_RadialMenuController.CloseMenu();

		return "ok closed";
	}
}
