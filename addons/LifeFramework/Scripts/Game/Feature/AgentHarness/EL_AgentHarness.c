// EL_AgentHarness — player-equivalent controls for an agent on a rendering instance.
//
// A play session (Workbench or game client) polls $profile:agent-box/command.txt.
// Each line is one action a player could take: press the interaction key
// (PerformAction on SCR_InteractionHandlerComponent, the F-key listener),
// scroll that list (SelectAction), perform a named ScriptedUserAction after
// the same CanBeShownScript and CanBePerformedScript checks, pulse a menu
// input, toggle the debug menu, stand beside a named world entity, or take a
// screenshot through EL_ScreenshotCapture.
// Results land in $profile:agent-box/result.txt. A headless dedicated server
// never starts the poll: EnsureLoop() runs only for the local player.

class EL_AgentCommand
{
	int Id;
	string Verb;
	string Arg1;
	string Arg2;
	float PosX;
	float PosY;
	float PosZ;
	string Error;

	//------------------------------------------------------------------------------------------------
	//! Parse one command line. Returns false and sets Error on a bad line.
	//! The line is space-separated tokens:
	//!   <id> use <ClassName> [EntityName]
	//!   <id> interact confirm|next|prev
	//!   <id> input <ActionName>
	//!   <id> open <MenuName> [show|hide|toggle]
	//!   <id> stand <EntityName>
	//!   <id> look
	//!   <id> capture ui <Name>
	//!   <id> capture world <Name> <x> <y> <z>
	static bool Parse(string line, out EL_AgentCommand cmd)
	{
		cmd = new EL_AgentCommand();
		line.TrimInPlace();
		if (line.IsEmpty())
		{
			cmd.Error = "empty";
			return false;
		}

		array<string> parts = {};
		line.Split(" ", parts, true);
		if (parts.Count() < 2)
		{
			cmd.Error = "need_id_and_verb";
			return false;
		}

		cmd.Id = parts[0].ToInt();
		if (cmd.Id <= 0)
		{
			cmd.Error = "bad_id";
			return false;
		}

		cmd.Verb = parts[1];
		if (cmd.Verb == "use")
			return ParseUse(parts, cmd);
		if (cmd.Verb == "interact")
			return ParseInteract(parts, cmd);
		if (cmd.Verb == "input")
			return ParseInput(parts, cmd);
		if (cmd.Verb == "open")
			return ParseOpen(parts, cmd);
		if (cmd.Verb == "stand")
			return ParseStand(parts, cmd);
		if (cmd.Verb == "look")
			return true;
		if (cmd.Verb == "capture")
			return ParseCapture(parts, cmd);

		cmd.Error = "unknown_verb";
		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool ParseUse(array<string> parts, EL_AgentCommand cmd)
	{
		if (parts.Count() < 3)
		{
			cmd.Error = "need_class";
			return false;
		}

		cmd.Arg1 = parts[2];
		if (!IsClassToken(cmd.Arg1))
		{
			cmd.Error = "bad_class";
			return false;
		}

		if (parts.Count() >= 4)
		{
			cmd.Arg2 = parts[3];
			if (!IsClassToken(cmd.Arg2))
			{
				cmd.Error = "bad_entity";
				return false;
			}
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool ParseInteract(array<string> parts, EL_AgentCommand cmd)
	{
		if (parts.Count() < 3)
		{
			cmd.Error = "need_interact";
			return false;
		}

		cmd.Arg1 = parts[2];
		if (cmd.Arg1 != "confirm" && cmd.Arg1 != "next" && cmd.Arg1 != "prev")
		{
			cmd.Error = "bad_interact";
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool ParseStand(array<string> parts, EL_AgentCommand cmd)
	{
		if (parts.Count() < 3)
		{
			cmd.Error = "need_entity";
			return false;
		}

		cmd.Arg1 = parts[2];
		if (!IsClassToken(cmd.Arg1))
		{
			cmd.Error = "bad_entity";
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool ParseInput(array<string> parts, EL_AgentCommand cmd)
	{
		if (parts.Count() < 3)
		{
			cmd.Error = "need_action";
			return false;
		}

		cmd.Arg1 = parts[2];
		if (!IsMenuAction(cmd.Arg1))
		{
			cmd.Error = "action_not_allowed";
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool ParseOpen(array<string> parts, EL_AgentCommand cmd)
	{
		if (parts.Count() < 3)
		{
			cmd.Error = "need_menu";
			return false;
		}

		cmd.Arg1 = parts[2];
		if (cmd.Arg1 != "DebugMenu")
		{
			cmd.Error = "menu_not_allowed";
			return false;
		}

		cmd.Arg2 = "toggle";
		if (parts.Count() >= 4)
		{
			cmd.Arg2 = parts[3];
			if (cmd.Arg2 != "show" && cmd.Arg2 != "hide" && cmd.Arg2 != "toggle")
			{
				cmd.Error = "bad_open_mode";
				return false;
			}
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool ParseCapture(array<string> parts, EL_AgentCommand cmd)
	{
		if (parts.Count() < 4)
		{
			cmd.Error = "need_capture_mode";
			return false;
		}

		cmd.Arg1 = parts[2];
		cmd.Arg2 = parts[3];
		if (!IsClassToken(cmd.Arg2))
		{
			cmd.Error = "bad_capture_name";
			return false;
		}

		if (cmd.Arg1 == "ui")
			return true;

		if (cmd.Arg1 != "world")
		{
			cmd.Error = "bad_capture_mode";
			return false;
		}

		if (parts.Count() < 7)
		{
			cmd.Error = "need_position";
			return false;
		}

		cmd.PosX = parts[4].ToFloat();
		cmd.PosY = parts[5].ToFloat();
		cmd.PosZ = parts[6].ToFloat();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsClassToken(string token)
	{
		if (!token || token.IsEmpty())
			return false;

		for (int i = 0; i < token.Length(); i++)
		{
			string ch = token.Substring(i, 1);
			bool ok = (ch >= "A" && ch <= "Z") || (ch >= "a" && ch <= "z") || (ch >= "0" && ch <= "9") || ch == "_";
			if (!ok)
				return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Menu navigation plus the debug-menu key. Movement keys are refused:
	//! the harness must not drive the character by injecting WASD.
	static bool IsMenuAction(string actionName)
	{
		if (actionName == "MenuBack")
			return true;
		if (actionName == "MenuUp")
			return true;
		if (actionName == "MenuDown")
			return true;
		if (actionName == "MenuLeft")
			return true;
		if (actionName == "MenuRight")
			return true;
		if (actionName == "MenuSelect")
			return true;
		if (actionName == "EL_OpenDebugMenu")
			return true;
		return false;
	}
}

class EL_AgentHarness
{
	protected static const string FEATURE = "AgentHarness";
	protected static const string COMMAND_PATH = "$profile:agent-box/command.txt";
	protected static const string RESULT_PATH = "$profile:agent-box/result.txt";

	protected static bool s_bLoop;
	protected static int s_iEnsureTries;
	protected static int s_iCaptureId;
	protected static string s_sHeldAction;
	protected static string s_sWantedClass;
	protected static string s_sWantedName;
	protected static IEntity s_pFoundEntity;
	protected static float s_fBestDist;
	protected static vector s_vOrigin;
	protected static string s_sLookJoined;
	protected static int s_iLookCount;

	//------------------------------------------------------------------------------------------------
	static void EnsureLoop()
	{
		if (s_bLoop)
			return;

		Game game = GetGame();
		if (!game)
			return;

		// A headless dedicated server never has a local controller. Do not
		// schedule retries there: the logic suite must not grow a poll.
		if (RplSession.Mode() == RplMode.Dedicated)
			return;

		PlayerController local = game.GetPlayerController();
		if (!local)
		{
			// Faction and spawn menus exist before the pawn. Keep trying until
			// the local controller appears, then the same poll serves menus
			// and world actions.
			s_iEnsureTries++;
			if (s_iEnsureTries > 60)
			{
				EL_Debug.Warn(FEATURE, "no local controller after 30s; poll not started");
				return;
			}

			game.GetCallqueue().CallLater(EnsureLoop, 500, false);
			return;
		}

		s_bLoop = true;
		game.GetCallqueue().CallLater(Poll, 400, true);
		EL_Debug.Log(FEATURE, "poll started");
	}

	//------------------------------------------------------------------------------------------------
	static void Poll()
	{
		FinishCapture();

		if (!FileIO.FileExists(COMMAND_PATH))
			return;

		FileHandle handle = FileIO.OpenFile(COMMAND_PATH, FileMode.READ);
		if (!handle)
		{
			EL_Debug.Error(FEATURE, "cannot read command file");
			return;
		}

		string line;
		handle.ReadLine(line);
		handle.Close();
		FileIO.DeleteFile(COMMAND_PATH);

		EL_AgentCommand cmd;
		if (!EL_AgentCommand.Parse(line, cmd))
		{
			int id = 0;
			string err = "parse";
			if (cmd)
			{
				id = cmd.Id;
				err = cmd.Error;
			}
			WriteResult(id, "error", err);
			return;
		}

		if (s_iCaptureId > 0)
		{
			WriteResult(cmd.Id, "error", "capture_busy");
			return;
		}

		Dispatch(cmd);
	}

	//------------------------------------------------------------------------------------------------
	protected static void FinishCapture()
	{
		if (s_iCaptureId <= 0)
			return;
		if (EL_ScreenshotCapture.IsBusy())
			return;

		WriteResult(s_iCaptureId, EL_ScreenshotCapture.GetLastResult(), EL_ScreenshotCapture.GetLastPath());
		s_iCaptureId = 0;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Dispatch(EL_AgentCommand cmd)
	{
		if (cmd.Verb == "use")
		{
			DoUse(cmd);
			return;
		}
		if (cmd.Verb == "interact")
		{
			DoInteract(cmd);
			return;
		}
		if (cmd.Verb == "stand")
		{
			DoStand(cmd);
			return;
		}
		if (cmd.Verb == "look")
		{
			DoLook(cmd);
			return;
		}
		if (cmd.Verb == "input")
		{
			DoInput(cmd);
			return;
		}
		if (cmd.Verb == "open")
		{
			DoOpen(cmd);
			return;
		}
		if (cmd.Verb == "capture")
		{
			DoCapture(cmd);
			return;
		}

		WriteResult(cmd.Id, "error", "unknown_verb");
	}

	//------------------------------------------------------------------------------------------------
	//! Same listener the F key uses: SCR_InteractionHandlerComponent.ActionPerform.
	//! next/prev are the SelectAction wheel, which moves the highlighted entry.
	protected static void DoInteract(EL_AgentCommand cmd)
	{
		PlayerController controller = GetGame().GetPlayerController();
		if (!controller)
		{
			WriteResult(cmd.Id, "error", "no_controller");
			return;
		}

		SCR_InteractionHandlerComponent handler = SCR_InteractionHandlerComponent.Cast(controller.FindComponent(SCR_InteractionHandlerComponent));
		if (!handler)
		{
			IEntity controlled = controller.GetControlledEntity();
			if (controlled)
				handler = SCR_InteractionHandlerComponent.Cast(controlled.FindComponent(SCR_InteractionHandlerComponent));
		}

		if (!handler)
		{
			WriteResult(cmd.Id, "error", "no_interaction");
			return;
		}

		if (cmd.Arg1 == "confirm")
			handler.ActionPerform(1, EActionTrigger.DOWN);
		else if (cmd.Arg1 == "next")
			handler.ActionScroll(1, EActionTrigger.VALUE);
		else
			handler.ActionScroll(-1, EActionTrigger.VALUE);

		EL_Debug.Log(FEATURE, string.Format("interact %1", cmd.Arg1));
		WriteResult(cmd.Id, "ok", cmd.Arg1);
	}

	//------------------------------------------------------------------------------------------------
	protected static void DoStand(EL_AgentCommand cmd)
	{
		SCR_ChimeraCharacter character = EL_Utils.GetLocalCharacter();
		if (!character)
		{
			WriteResult(cmd.Id, "error", "no_character");
			return;
		}

		BaseWorld world = GetGame().GetWorld();
		if (!world || !world.FindEntityByName(cmd.Arg1))
		{
			WriteResult(cmd.Id, "error", "entity_not_found");
			return;
		}

		character.EL_AskHarnessStand(cmd.Arg1);
		WriteResult(cmd.Id, "ok", cmd.Arg1);
	}

	//------------------------------------------------------------------------------------------------
	//! Actions whose CanBeShownScript is true within 4m. That is the F-key list.
	protected static void DoLook(EL_AgentCommand cmd)
	{
		IEntity user = EL_Utils.GetLocalCharacter();
		if (!user)
		{
			WriteResult(cmd.Id, "error", "no_character");
			return;
		}

		BaseWorld world = GetGame().GetWorld();
		if (!world)
		{
			WriteResult(cmd.Id, "error", "no_world");
			return;
		}

		s_sLookJoined = "";
		s_iLookCount = 0;
		s_vOrigin = user.GetOrigin();
		world.QueryEntitiesBySphere(s_vOrigin, 4, CollectLookEntity, null, EQueryEntitiesFlags.ALL);

		if (s_sLookJoined.IsEmpty())
			WriteResult(cmd.Id, "ok", "none");
		else
			WriteResult(cmd.Id, "ok", s_sLookJoined);
	}

	//------------------------------------------------------------------------------------------------
	protected static bool CollectLookEntity(IEntity entity)
	{
		if (!entity || s_iLookCount >= 12)
			return true;

		BaseActionsManagerComponent mgr = BaseActionsManagerComponent.Cast(entity.FindComponent(BaseActionsManagerComponent));
		if (!mgr)
			return true;

		IEntity user = EL_Utils.GetLocalCharacter();
		array<BaseUserAction> actions = {};
		mgr.GetActionsList(actions);
		foreach (BaseUserAction action : actions)
		{
			if (s_iLookCount >= 12)
				break;

			ScriptedUserAction scripted = ScriptedUserAction.Cast(action);
			if (!scripted || !scripted.CanBeShownScript(user))
				continue;

			string hit = scripted.Type().ToString();
			string name = entity.GetName();
			if (!name.IsEmpty())
				hit = hit + "@" + name;

			if (!s_sLookJoined.IsEmpty())
				s_sLookJoined = s_sLookJoined + ",";
			s_sLookJoined = s_sLookJoined + hit;
			s_iLookCount++;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static void DoUse(EL_AgentCommand cmd)
	{
		IEntity user = EL_Utils.GetLocalCharacter();
		if (!user)
		{
			WriteResult(cmd.Id, "error", "no_character");
			return;
		}

		IEntity target = FindTarget(user, cmd.Arg1, cmd.Arg2);
		if (!target)
		{
			WriteResult(cmd.Id, "error", "action_not_found");
			return;
		}

		ScriptedUserAction action = FindOnEntity(target, cmd.Arg1);
		if (!action)
		{
			WriteResult(cmd.Id, "error", "action_not_found");
			return;
		}

		if (!action.CanBeShownScript(user))
		{
			WriteResult(cmd.Id, "error", "cannot_show");
			return;
		}

		if (!action.CanBePerformedScript(user))
		{
			WriteResult(cmd.Id, "error", "cannot_perform");
			return;
		}

		action.PerformAction(target, user);
		EL_Debug.Log(FEATURE, string.Format("performed %1", cmd.Arg1));
		WriteResult(cmd.Id, "ok", cmd.Arg1);
	}

	//------------------------------------------------------------------------------------------------
	protected static void DoInput(EL_AgentCommand cmd)
	{
		InputManager input = GetGame().GetInputManager();
		if (!input)
		{
			WriteResult(cmd.Id, "error", "no_input");
			return;
		}

		s_sHeldAction = cmd.Arg1;
		input.SetActionValue(cmd.Arg1, 1);
		GetGame().GetCallqueue().CallLater(ClearInput, 80, false);
		EL_Debug.Log(FEATURE, string.Format("input %1", cmd.Arg1));
		WriteResult(cmd.Id, "ok", cmd.Arg1);
	}

	//------------------------------------------------------------------------------------------------
	protected static void ClearInput()
	{
		if (s_sHeldAction == "")
			return;

		InputManager input = GetGame().GetInputManager();
		if (input)
			input.SetActionValue(s_sHeldAction, 0);
		s_sHeldAction = "";
	}

	//------------------------------------------------------------------------------------------------
	//! Same toggle the EL_OpenDebugMenu key uses.
	protected static void DoOpen(EL_AgentCommand cmd)
	{
		MenuManager menuManager = GetGame().GetMenuManager();
		if (!menuManager)
		{
			WriteResult(cmd.Id, "error", "no_menu");
			return;
		}

		bool open = false;
		if (menuManager.FindMenuByPreset(ChimeraMenuPreset.DebugMenu))
			open = true;
		// show/hide are for screenshots: repeating a shot must not close the menu.
		// toggle is the debug key (F10).
		if (cmd.Arg2 == "show")
		{
			if (open)
			{
				WriteResult(cmd.Id, "ok", "already_open");
				return;
			}

			menuManager.OpenMenu(ChimeraMenuPreset.DebugMenu);
			WriteResult(cmd.Id, "ok", "opened");
			return;
		}

		if (cmd.Arg2 == "hide")
		{
			if (!open)
			{
				WriteResult(cmd.Id, "ok", "already_closed");
				return;
			}

			menuManager.CloseMenuByPreset(ChimeraMenuPreset.DebugMenu);
			WriteResult(cmd.Id, "ok", "closed");
			return;
		}

		if (open)
		{
			menuManager.CloseMenuByPreset(ChimeraMenuPreset.DebugMenu);
			WriteResult(cmd.Id, "ok", "closed");
			return;
		}

		menuManager.OpenMenu(ChimeraMenuPreset.DebugMenu);
		WriteResult(cmd.Id, "ok", "opened");
	}

	//------------------------------------------------------------------------------------------------
	protected static void DoCapture(EL_AgentCommand cmd)
	{
		EL_ScreenshotShot shot = new EL_ScreenshotShot();
		shot.CaptureName = cmd.Arg2;
		shot.SettleTime = 0.4;

		if (cmd.Arg1 == "ui")
		{
			shot.AimAtTarget = false;
		}
		else
		{
			shot.AimAtTarget = true;
			shot.TargetPosition = Vector(cmd.PosX, cmd.PosY, cmd.PosZ);
			shot.CameraOffset = "10 3 10";
			shot.TimeOfDay24h = 12;
			shot.WeatherStateName = "Clear";
		}

		if (!EL_ScreenshotCapture.StartCapture(shot))
		{
			WriteResult(cmd.Id, "error", EL_ScreenshotCapture.GetLastResult());
			return;
		}

		s_iCaptureId = cmd.Id;
		WriteResult(cmd.Id, "pending", cmd.Arg2);
	}

	//------------------------------------------------------------------------------------------------
	protected static IEntity FindTarget(IEntity user, string className, string entityName)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return null;

		s_sWantedClass = className;
		s_sWantedName = entityName;
		s_pFoundEntity = null;
		s_fBestDist = 999999;
		s_vOrigin = user.GetOrigin();

		float radius = 5;
		if (entityName != "")
			radius = 80;

		world.QueryEntitiesBySphere(s_vOrigin, radius, CollectActionEntity, null, EQueryEntitiesFlags.ALL);
		return s_pFoundEntity;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool CollectActionEntity(IEntity entity)
	{
		if (!entity)
			return true;

		if (s_sWantedName != "" && entity.GetName() != s_sWantedName)
			return true;

		if (!FindOnEntity(entity, s_sWantedClass))
			return true;

		float dist = vector.Distance(entity.GetOrigin(), s_vOrigin);
		if (!s_pFoundEntity || dist < s_fBestDist)
		{
			s_pFoundEntity = entity;
			s_fBestDist = dist;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static ScriptedUserAction FindOnEntity(IEntity entity, string className)
	{
		BaseActionsManagerComponent mgr = BaseActionsManagerComponent.Cast(entity.FindComponent(BaseActionsManagerComponent));
		if (!mgr)
			return null;

		array<BaseUserAction> actions = {};
		mgr.GetActionsList(actions);
		foreach (BaseUserAction action : actions)
		{
			if (!action)
				continue;
			if (action.Type().ToString() != className)
				continue;
			return ScriptedUserAction.Cast(action);
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected static void WriteResult(int id, string status, string detail)
	{
		FileIO.MakeDirectory("$profile:agent-box");
		FileHandle handle = FileIO.OpenFile(RESULT_PATH, FileMode.WRITE);
		if (!handle)
		{
			EL_Debug.Error(FEATURE, "cannot write result file");
			return;
		}

		handle.Write(string.Format("%1 %2 %3", id, status, detail));
		handle.Close();
		EL_Debug.Log(FEATURE, string.Format("result %1 %2 %3", id, status, detail));
	}
}
