// LF_Emote.c — the emote radial (#982). The menu is the base game's: SCR_PlayerControllerCommandingComponent
// opens Configs/Emote/LF_EmoteMenu.conf on LF_OpenEmoteMenu (hold F1, or hold the right shoulder with
// view on a gamepad), and the entries are the base game gesture and loiter commands from
// Configs/Commanding/Commands.conf (SCR_CommandingManagerComponent on the game mode). Both kinds are
// local-only commands: the owner plays the gesture or starts the loiter and the base game replicates it.
// What LF adds: emotes need no AI group (roleplay has none), and a cuffed or stunned body cannot emote.

class LF_Emote
{
	static const string FEATURE = "Emote";

	//! The system a server switches the emote radial and sitting off with (#1075).
	static const string SYSTEM = "Emote";

	//------------------------------------------------------------------------------------------------
	//! The rule, with no world.
	static bool Allowed(bool restrained, bool stunned)
	{
		return !restrained && !stunned;
	}

	//------------------------------------------------------------------------------------------------
	//! Uses only replicated flags, so the radial greys an entry on the client for the same reason the
	//! server would.
	static bool AllowedFor(IEntity character)
	{
		return Allowed(LF_Restraint.IsRestrained(character), LF_Stun.IsStunned(character));
	}

	//------------------------------------------------------------------------------------------------
	//! A command the radial may run on this machine with no group: a local-only, non-group command.
	static bool IsGroupFree(SCR_BaseRadialCommand command)
	{
		if (!command)
			return false;

		if (command.Type().IsInherited(SCR_BaseGroupCommand))
			return false;

		return command.HasLocalEffectOnly();
	}
}

//------------------------------------------------------------------------------------------------
//! One-shot gestures (salute, point, halt, come here). A modded config class repeats the base
//! game container attributes: without them the class drops out of configs ("Unknown class").
[BaseContainerProps(), SCR_BaseGroupCommandTitleField("m_sCommandName")]
modded class SCR_EmoteBaseCommand
{
	//------------------------------------------------------------------------------------------------
	override bool CanBePerformed(notnull SCR_ChimeraCharacter user)
	{
		if (!LF_Systems.IsEnabled(LF_Emote.SYSTEM))
		{
			SetCannotPerformReason("#LF-Systems_Off");
			return false;
		}

		if (!LF_Emote.AllowedFor(user))
		{
			SetCannotPerformReason("#LF-Emote_Restrained");
			return false;
		}

		return super.CanBePerformed(user);
	}

	//------------------------------------------------------------------------------------------------
	override bool Execute(IEntity cursorTarget, IEntity groupEnt, vector targetPosition, int playerID, bool isClient)
	{
		if (!LF_Systems.IsEnabled(LF_Emote.SYSTEM))
		{
			LF_Debug.Info(LF_Emote.FEATURE, string.Format("emote %1 refused: Emote is off on this server", GetCommandName()));
			return false;
		}

		if (SCR_PlayerController.GetLocalPlayerId() == playerID)
		{
			IEntity character = SCR_PlayerController.GetLocalControlledEntity();
			if (character && !LF_Emote.AllowedFor(character))
			{
				LF_Debug.Info(LF_Emote.FEATURE, string.Format("emote %1 refused: restrained or stunned", GetCommandName()));
				return false;
			}
		}

		return super.Execute(cursorTarget, groupEnt, targetPosition, playerID, isClient);
	}
}

//------------------------------------------------------------------------------------------------
//! Held emotes (sit on the ground, lean, smoke, cheer). StartLoiter checks CanBePerformed again after
//! the hands are emptied, so a body cuffed meanwhile does not start one.
[BaseContainerProps(), SCR_BaseGroupCommandTitleField("m_sCommandName")]
modded class SCR_ContinuousLoiterCommand
{
	//------------------------------------------------------------------------------------------------
	override bool CanBePerformed(notnull SCR_ChimeraCharacter user)
	{
		if (!LF_Systems.IsEnabled(LF_Emote.SYSTEM))
		{
			SetCannotPerformReason("#LF-Systems_Off");
			return false;
		}

		if (!LF_Emote.AllowedFor(user))
		{
			SetCannotPerformReason("#LF-Emote_Restrained");
			return false;
		}

		return super.CanBePerformed(user);
	}
}

//------------------------------------------------------------------------------------------------
//! The base game runs every radial command through the player's AI group and returns early when the
//! world has no groups manager. A local-only emote never touches the group, so LF runs it straight.
modded class SCR_PlayerControllerCommandingComponent
{
	//------------------------------------------------------------------------------------------------
	override protected void ExecuteCommand(vector targetPosition)
	{
		SCR_CommandingManagerComponent commandingManager = SCR_CommandingManagerComponent.GetInstance();
		SCR_BaseRadialCommand command;
		if (commandingManager)
			command = commandingManager.FindCommand(m_sExecutedCommandName);

		if (!LF_Emote.IsGroupFree(command) || SCR_GroupsManagerComponent.GetInstance())
		{
			super.ExecuteCommand(targetPosition);
			return;
		}

		if (m_PhysicsHelper)
		{
			m_PhysicsHelper.GetOnTraceFinished().Remove(ExecuteCommand);
			m_PhysicsHelper = null;
		}

		m_bIsCommandExecuting = false;

		PlayerController controller = PlayerController.Cast(GetOwner());
		if (!controller)
			return;

		IEntity character = controller.GetControlledEntity();
		if (!character)
		{
			LF_Debug.Warn(LF_Emote.FEATURE, string.Format("emote %1 not played: no character", m_sExecutedCommandName));
			return;
		}

		if (command.IsTargetSelf())
		{
			m_SelectedEntity = character;
			targetPosition = character.GetOrigin();
		}

		if (!LF_Systems.IsEnabled(LF_Emote.SYSTEM))
			return;

		DeleteShownCommand();
		command.VisualizeCommand(targetPosition);
		bool played = command.Execute(m_SelectedEntity, null, targetPosition, controller.GetPlayerId(), true);
		LF_Debug.Info(LF_Emote.FEATURE, string.Format("emote %1 by player %2: %3", m_sExecutedCommandName, controller.GetPlayerId(), played));
	}
}

//------------------------------------------------------------------------------------------------
//! LF adds the commanding manager to the game mode, which opens the base game RPC_RequestExecuteCommand
//! to every client. The base game reads the client-sent index unchecked.
modded class SCR_CommandingManagerComponent
{
	//------------------------------------------------------------------------------------------------
	//! The index comes from a client (RPC_RequestExecuteCommand); the base game reads it unchecked.
	override string FindCommandNameFromIndex(int commandIndex)
	{
		if (!m_aCommands || !m_aCommands.IsIndexValid(commandIndex))
		{
			LF_Debug.Warn(LF_Emote.FEATURE, string.Format("radial command index %1 refused: not in the command list", commandIndex));
			return string.Empty;
		}

		return super.FindCommandNameFromIndex(commandIndex);
	}
}
