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
	string Arg3;
	string Arg4;
	string Arg5;
	float PosX;
	float PosY;
	float PosZ;
	float CamX;
	float CamY;
	float CamZ;
	bool HasCameraOffset;
	string Error;

	//------------------------------------------------------------------------------------------------
	//! Parse one command line. Returns false and sets Error on a bad line.
	//! The line is space-separated tokens:
	//!   <id> use <ClassName> [EntityName]
	//!   <id> interact confirm|next|prev
	//!   <id> input <ActionName>
	//!   <id> open <MenuName> [show|hide|toggle]
	//!   <id> kit show|hide
	//!   <id> kit line <Part> <Text>
	//!   <id> kit status <Part> <Label> <Status> <Action|none>
	//!   <id> kit quantity <Part> <Total> <Take>
	//!   <id> stand <EntityName>
	//!   <id> look
	//!   <id> ui
	//!   <id> click <WidgetName> [index]
	//!   <id> capture ui <Name>
	//!   <id> capture world <Name> <x> <y> <z> [ox oy oz]
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
		if (cmd.Verb == "kit")
			return ParseKit(parts, cmd);
		if (cmd.Verb == "stand")
			return ParseStand(parts, cmd);
		if (cmd.Verb == "look")
			return true;
		if (cmd.Verb == "ui")
			return true;
		if (cmd.Verb == "click")
			return ParseClick(parts, cmd);
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
	protected static bool ParseClick(array<string> parts, EL_AgentCommand cmd)
	{
		if (parts.Count() < 3)
		{
			cmd.Error = "need_widget";
			return false;
		}

		cmd.Arg1 = parts[2];
		if (!IsClassToken(cmd.Arg1))
		{
			cmd.Error = "bad_widget";
			return false;
		}

		cmd.Arg2 = "0";
		if (parts.Count() >= 4)
		{
			cmd.Arg2 = parts[3];
			if (!IsIndex(cmd.Arg2))
			{
				cmd.Error = "bad_index";
				return false;
			}
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsIndex(string token)
	{
		if (!token || token.IsEmpty())
			return false;

		for (int i = 0; i < token.Length(); i++)
		{
			string ch = token.Substring(i, 1);
			if (ch < "0" || ch > "9")
				return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool ParseKit(array<string> parts, EL_AgentCommand cmd)
	{
		if (parts.Count() < 3)
		{
			cmd.Error = "need_step";
			return false;
		}

		cmd.Arg1 = parts[2];
		if (cmd.Arg1 == "show" || cmd.Arg1 == "hide")
			return true;

		if (cmd.Arg1 == "line")
		{
			if (parts.Count() < 5)
			{
				cmd.Error = "need_line";
				return false;
			}

			cmd.Arg2 = parts[3];
			cmd.Arg3 = parts[4];
			if (!IsClassToken(cmd.Arg2) || !IsClassToken(cmd.Arg3))
			{
				cmd.Error = "bad_kit";
				return false;
			}

			return true;
		}

		if (cmd.Arg1 == "status")
		{
			if (parts.Count() < 7)
			{
				cmd.Error = "need_status";
				return false;
			}

			cmd.Arg2 = parts[3];
			cmd.Arg3 = parts[4];
			cmd.Arg4 = parts[5];
			cmd.Arg5 = parts[6];
			if (!IsClassToken(cmd.Arg2) || !IsClassToken(cmd.Arg3) || !IsClassToken(cmd.Arg4) || !IsClassToken(cmd.Arg5))
			{
				cmd.Error = "bad_kit";
				return false;
			}

			if (cmd.Arg5 == "none")
				cmd.Arg5 = "";

			return true;
		}

		if (cmd.Arg1 == "quantity")
		{
			if (parts.Count() < 6)
			{
				cmd.Error = "need_quantity";
				return false;
			}

			cmd.Arg2 = parts[3];
			cmd.Arg3 = parts[4];
			cmd.Arg4 = parts[5];
			if (!IsClassToken(cmd.Arg2) || !IsIntToken(cmd.Arg3) || !IsIntToken(cmd.Arg4))
			{
				cmd.Error = "bad_kit";
				return false;
			}

			return true;
		}

		cmd.Error = "bad_kit_step";
		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsIntToken(string token)
	{
		if (!token || token.IsEmpty())
			return false;

		int start = 0;
		if (token.Substring(0, 1) == "-")
		{
			if (token.Length() < 2)
				return false;
			start = 1;
		}

		for (int i = start; i < token.Length(); i++)
		{
			string ch = token.Substring(i, 1);
			if (ch < "0" || ch > "9")
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
		cmd.HasCameraOffset = false;
		if (parts.Count() > 7)
		{
			if (parts.Count() < 10)
			{
				cmd.Error = "need_offset";
				return false;
			}

			cmd.CamX = parts[7].ToFloat();
			cmd.CamY = parts[8].ToFloat();
			cmd.CamZ = parts[9].ToFloat();
			cmd.HasCameraOffset = true;
		}

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
	//! Where the character stands for a world shot, and the yaw/pitch that faces
	//! the target. The screenshot is the player view after this pose.
	static void CameraStand(vector target, vector offset, out vector stand, out vector angles)
	{
		stand = target + offset;
		if (vector.DistanceSq(target, stand) < 0.01)
			stand = target + "10 3 10";

		vector dir = target - stand;
		float horiz = Math.Sqrt((dir[0] * dir[0]) + (dir[2] * dir[2]));
		float yaw = Math.Atan2(dir[0], dir[2]) * Math.RAD2DEG;
		float pitch = 0;
		if (horiz > 0.001)
			pitch = Math.Atan2(dir[1], horiz) * Math.RAD2DEG;
		angles = Vector(yaw, pitch, 0);
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
	protected static vector s_vShotStand;
	protected static vector s_vShotAngles;
	protected static bool s_bShotPose;
	protected static string s_sHeldAction;
	protected static string s_sWantedClass;
	protected static string s_sWantedName;
	protected static IEntity s_pFoundEntity;
	protected static float s_fBestDist;
	protected static vector s_vOrigin;
	protected static string s_sLookJoined;
	protected static int s_iLookCount;
	protected static string s_sUiJoined;
	protected static int s_iUiCount;
	protected static string s_sClickName;
	protected static int s_iClickIndex;
	protected static int s_iClickSeen;
	protected static Widget s_wClickTarget;

	//------------------------------------------------------------------------------------------------
	static void EnsureLoop()
	{
		if (s_bLoop)
			return;

		if (!GetGame())
			return;

		// A headless dedicated server never has a local controller. Do not
		// schedule retries there: the logic suite must not grow a poll.
		if (RplSession.Mode() == RplMode.Dedicated)
			return;

		PlayerController local = GetGame().GetPlayerController();
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

			GetGame().GetCallqueue().CallLater(EnsureLoop, 500, false);
			return;
		}

		s_bLoop = true;
		GetGame().GetCallqueue().CallLater(Poll, 400, true);
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
		if (cmd.Verb == "ui")
		{
			DoUi(cmd);
			return;
		}
		if (cmd.Verb == "click")
		{
			DoClick(cmd);
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
		if (cmd.Verb == "kit")
		{
			DoKit(cmd);
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
	//! Visible buttons. SCR_ButtonTextComponent names are the faction and spawn
	//! buttons. Other ButtonWidgets (shop buy/sell, ATM) are listed by widget name.
	//! A repeated name is the next visible match; click uses that index.
	protected static void DoUi(EL_AgentCommand cmd)
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
		{
			WriteResult(cmd.Id, "error", "no_workspace");
			return;
		}

		s_sUiJoined = "";
		s_iUiCount = 0;
		CollectUi(workspace);

		if (s_sUiJoined.IsEmpty())
			WriteResult(cmd.Id, "ok", "none");
		else
			WriteResult(cmd.Id, "ok", s_sUiJoined);
	}

	//------------------------------------------------------------------------------------------------
	protected static void CollectUi(Widget widget)
	{
		if (!widget || s_iUiCount >= 40)
			return;

		if (Shown(widget))
		{
			if (Clickable(widget))
			{
				string name = widget.GetName();
				if (!s_sUiJoined.IsEmpty())
					s_sUiJoined = s_sUiJoined + ",";
				s_sUiJoined = s_sUiJoined + name;
				s_iUiCount++;
			}
		}

		Widget child = widget.GetChildren();
		while (child)
		{
			CollectUi(child);
			child = child.GetSibling();
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Local visibility. A hidden parent hides its children even when the child flag is set.
	protected static bool Shown(Widget widget)
	{
		Widget cursor = widget;
		while (cursor && cursor.GetParent())
		{
			if (!cursor.IsVisible())
				return false;
			cursor = cursor.GetParent();
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Same path as a mouse click. Text buttons fire m_OnClicked, which is what
	//! the faction and spawn menus insert. Other buttons bubble OnClick to the
	//! widget handler the engine would call (shop row, ATM menu).
	protected static void DoClick(EL_AgentCommand cmd)
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
		{
			WriteResult(cmd.Id, "error", "no_workspace");
			return;
		}

		s_sClickName = cmd.Arg1;
		s_iClickIndex = cmd.Arg2.ToInt();
		s_iClickSeen = 0;
		s_wClickTarget = null;
		FindClickTarget(workspace);

		if (!s_wClickTarget)
		{
			WriteResult(cmd.Id, "error", "widget_not_found");
			return;
		}

		SCR_ButtonTextComponent textButton = SCR_ButtonTextComponent.Cast(s_wClickTarget.FindHandler(SCR_ButtonTextComponent));
		if (!textButton)
			textButton = SCR_ButtonTextComponent.GetButtonText(s_sClickName, s_wClickTarget);
		if (textButton)
		{
			textButton.m_OnClicked.Invoke();
			EL_Debug.Log(FEATURE, string.Format("click %1", s_sClickName));
			WriteResult(cmd.Id, "ok", s_sClickName);
			return;
		}

		// Debug menu buttons are plain ButtonWidgets. The menu class is not a
		// widget handler on them, so the parent walk never reaches OnClick.
		MenuManager menuManager = GetGame().GetMenuManager();
		if (menuManager)
		{
			EL_DebugMenu debugMenu = EL_DebugMenu.Cast(menuManager.FindMenuByPreset(ChimeraMenuPreset.DebugMenu));
			if (debugMenu && debugMenu.OnClick(s_wClickTarget, 0, 0, 0))
			{
				EL_Debug.Log(FEATURE, string.Format("click %1", s_sClickName));
				WriteResult(cmd.Id, "ok", s_sClickName);
				return;
			}
		}

		Widget cursor = s_wClickTarget;
		while (cursor)
		{
			ScriptedWidgetComponent handler = ScriptedWidgetComponent.Cast(cursor.FindHandler(ScriptedWidgetComponent));
			if (handler && handler.OnClick(s_wClickTarget, 0, 0, 0))
			{
				EL_Debug.Log(FEATURE, string.Format("click %1", s_sClickName));
				WriteResult(cmd.Id, "ok", s_sClickName);
				return;
			}

			cursor = cursor.GetParent();
		}

		WriteResult(cmd.Id, "error", "click_unhandled");
	}

	//------------------------------------------------------------------------------------------------
	protected static void FindClickTarget(Widget widget)
	{
		if (!widget || s_wClickTarget)
			return;

		if (Shown(widget) && widget.GetName() == s_sClickName && Clickable(widget))
		{
			if (s_iClickSeen == s_iClickIndex)
				s_wClickTarget = widget;
			s_iClickSeen++;
		}

		Widget child = widget.GetChildren();
		while (child)
		{
			FindClickTarget(child);
			child = child.GetSibling();
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Faction and spawn buttons carry SCR_ButtonTextComponent. Shop and ATM
	//! buttons are ButtonWidgets whose name ends in Button and whose handler
	//! is one of our menu classes, so the vanilla HUD does not flood the list.
	protected static bool Clickable(Widget widget)
	{
		string name = widget.GetName();
		if (!EL_AgentCommand.IsClassToken(name))
			return false;
		if (name.Length() < 6 || name.Substring(name.Length() - 6, 6) != "Button")
			return false;

		if (SCR_ButtonTextComponent.Cast(widget.FindHandler(SCR_ButtonTextComponent)))
			return true;

		if (!ButtonWidget.Cast(widget))
			return false;

		if (DebugMenuContains(widget))
			return true;

		Widget cursor = widget;
		while (cursor)
		{
			ScriptedWidgetComponent handler = ScriptedWidgetComponent.Cast(cursor.FindHandler(ScriptedWidgetComponent));
			if (handler)
			{
				string typeName = handler.Type().ToString();
				if (typeName.IndexOf("EL_") == 0 || typeName.IndexOf("LF_") == 0)
					return true;
			}
			cursor = cursor.GetParent();
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! True when the widget sits inside the open debug menu. That menu's buttons
	//! have no SCR_ButtonTextComponent and the menu itself is not a widget handler.
	protected static bool DebugMenuContains(Widget widget)
	{
		MenuManager menuManager = GetGame().GetMenuManager();
		if (!menuManager)
			return false;

		EL_DebugMenu menu = EL_DebugMenu.Cast(menuManager.FindMenuByPreset(ChimeraMenuPreset.DebugMenu));
		if (!menu)
			return false;

		Widget root = menu.GetRootWidget();
		if (!root)
			return false;

		Widget cursor = widget;
		while (cursor)
		{
			if (cursor == root)
				return true;
			cursor = cursor.GetParent();
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected static void DoUse(EL_AgentCommand cmd)
	{
		SCR_ChimeraCharacter character = EL_Utils.GetLocalCharacter();
		if (!character)
		{
			WriteResult(cmd.Id, "error", "no_character");
			return;
		}

		character.EL_AskHarnessUse(cmd.Id, cmd.Arg1, cmd.Arg2);
	}

	//------------------------------------------------------------------------------------------------
	//! Server-side use. Checks and PerformAction run where inventory changes stick.
	//! \param user The character that asked. On a listen host this is not GetLocalCharacter.
	//! \param commandId Result id the harness is waiting on.
	//! \param className Action class.
	//! \param entityName Named host, or empty for the nearest match.
	static void ApplyUse(IEntity user, int commandId, string className, string entityName)
	{
		if (!user)
		{
			WriteResult(commandId, "error", "no_character");
			return;
		}

		IEntity target = FindTarget(user, className, entityName);
		if (!target)
		{
			WriteResult(commandId, "error", "action_not_found");
			return;
		}

		ScriptedUserAction action = FindOnEntity(target, className);
		if (!action)
		{
			WriteResult(commandId, "error", "action_not_found");
			return;
		}

		if (!action.CanBeShownScript(user))
		{
			WriteResult(commandId, "error", "cannot_show");
			return;
		}

		if (!action.CanBePerformedScript(user))
		{
			WriteResult(commandId, "error", "cannot_perform");
			return;
		}

		action.PerformAction(target, user);
		EL_Debug.Log(FEATURE, string.Format("performed %1", className));
		WriteResult(commandId, "ok", className);
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
	//! Opens the unwired kit sample and drives the states a later screen can show.
	//! Nothing in the world calls this. The play-session command is the only opener.
	protected static void DoKit(EL_AgentCommand cmd)
	{
		if (cmd.Arg1 == "show")
		{
			LF_UIScreen screen = LF_UISampleScreen.Open();
			if (!screen)
			{
				WriteResult(cmd.Id, "error", "open_failed");
				return;
			}

			EL_Debug.Log(FEATURE, "kit show");
			WriteResult(cmd.Id, "ok", "opened");
			return;
		}

		if (cmd.Arg1 == "hide")
		{
			LF_UISampleScreen.Hide();
			EL_Debug.Log(FEATURE, "kit hide");
			WriteResult(cmd.Id, "ok", "closed");
			return;
		}

		LF_UIScreen screen = LF_UISampleScreen.Current();
		if (!screen)
		{
			WriteResult(cmd.Id, "error", "not_open");
			return;
		}

		if (cmd.Arg1 == "line")
		{
			if (!screen.SetLine(cmd.Arg2, cmd.Arg3))
			{
				WriteResult(cmd.Id, "error", "no_part");
				return;
			}

			EL_Debug.Log(FEATURE, "kit line " + cmd.Arg2);
			WriteResult(cmd.Id, "ok", cmd.Arg2);
			return;
		}

		if (cmd.Arg1 == "status")
		{
			if (!screen.SetStatusRow(cmd.Arg2, cmd.Arg3, cmd.Arg4, cmd.Arg5))
			{
				WriteResult(cmd.Id, "error", "no_part");
				return;
			}

			EL_Debug.Log(FEATURE, "kit status " + cmd.Arg2);
			WriteResult(cmd.Id, "ok", cmd.Arg2);
			return;
		}

		if (cmd.Arg1 == "quantity")
		{
			if (!screen.SetQuantity(cmd.Arg2, cmd.Arg3.ToInt(), cmd.Arg4.ToInt()))
			{
				WriteResult(cmd.Id, "error", "no_part");
				return;
			}

			EL_Debug.Log(FEATURE, "kit quantity " + cmd.Arg2);
			WriteResult(cmd.Id, "ok", cmd.Arg2);
			return;
		}

		WriteResult(cmd.Id, "error", "bad_kit_step");
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
	//! Player view for a world shot. Called once when the shot is asked, and
	//! again just before the screenshot so the body is still on the offset.
	protected static void ReapplyWorldPose()
	{
		if (!s_bShotPose)
			return;

		SCR_ChimeraCharacter character = EL_Utils.GetLocalCharacter();
		if (!character)
		{
			EL_Debug.Error(FEATURE, "world shot has no character to pose");
			return;
		}

		character.EL_AskHarnessPose(s_vShotStand[0], s_vShotStand[1], s_vShotStand[2], s_vShotAngles[0], s_vShotAngles[1]);
	}

	//------------------------------------------------------------------------------------------------
	protected static void DoCapture(EL_AgentCommand cmd)
	{
		EL_ScreenshotShot shot = new EL_ScreenshotShot();
		shot.CaptureName = cmd.Arg2;
		shot.SettleTime = 0.4;

		s_bShotPose = false;
		if (cmd.Arg1 == "ui")
		{
			shot.AimAtTarget = false;
		}
		else
		{
			// The character camera ignores CameraManager.SetCamera. Stand the
			// player at the offset, face the target, and shoot that view.
			shot.AimAtTarget = false;
			shot.TargetPosition = Vector(cmd.PosX, cmd.PosY, cmd.PosZ);
			vector offset = "10 3 10";
			if (cmd.HasCameraOffset)
				offset = Vector(cmd.CamX, cmd.CamY, cmd.CamZ);
			vector stand;
			vector angles;
			EL_AgentCommand.CameraStand(shot.TargetPosition, offset, stand, angles);
			s_vShotStand = stand;
			s_vShotAngles = angles;
			s_bShotPose = true;
			ReapplyWorldPose();
			Game game = GetGame();
			if (!game)
			{
				EL_Debug.Error(FEATURE, "world shot has no game");
			}
			else
			{
				if (game.GetWorld())
					game.BeginPreload(game.GetWorld(), shot.TargetPosition, 200);
				// Gravity drops an elevated body during the settle. Put it back
				// on the offset, then leave time for that pose to replicate
				// before MakeScreenshot.
				GetGame().GetCallqueue().CallLater(ReapplyWorldPose, 700, false);
			}
			shot.SettleTime = 1.2;
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
