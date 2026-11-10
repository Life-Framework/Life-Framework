// LF_AgentHarness â€” player-equivalent controls for an agent on a rendering instance.
//
// A play session (Workbench or game client) polls $profile:agent-box/command.txt.
// Each line is one action a player could take: press the interaction key
// (PerformAction on SCR_InteractionHandlerComponent, the F-key listener),
// scroll that list (SelectAction), perform a named ScriptedUserAction after
// the same CanBeShownScript and CanBePerformedScript checks, pulse a menu
// input, toggle the debug menu, stand beside a named world entity, or take a
// screenshot through LF_ScreenshotCapture.
// Results land in $profile:agent-box/result.txt. A headless dedicated server
// never starts the poll: EnsureLoop() runs only for the local player.

class LF_AgentCommand
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
	//! The session a command is addressed to; empty when the sender named none.
	string Session;

	static const string SESSION_PREFIX = "s:";

	//------------------------------------------------------------------------------------------------
	//! Whether a client accepts a command. A session client (started with -lfSession <id>) runs
	//! only commands addressed to that id, so a command written for another worktree's session
	//! (the profiles live in one shared session dir) is refused instead of clicking through
	//! someone else's game. A client with no session (Workbench, default profile) runs only
	//! commands that name none.
	//! \param own This client's -lfSession value, or empty.
	//! \param addressed The command's s:<id>, or empty.
	static bool SessionAccepts(string own, string addressed)
	{
		return own == addressed;
	}

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
	//!   <id> state
	//!   <id> ui [text]
	//!   <id> click <WidgetName> [index]
	//!   <id> type <PartName> <Text>
	//!   <id> capture ui <Name>
	//!   <id> capture world <Name> <x> <y> <z> [ox oy oz]
	//!   <id> film start <Name> <fps> [maxSeconds]
	//!   <id> film stop|status
	static bool Parse(string line, out LF_AgentCommand cmd)
	{
		cmd = new LF_AgentCommand();
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

		// <id> s:<session> <verb> ...: the session the sender addressed (agent-box/session.mjs).
		if (parts[1].StartsWith(SESSION_PREFIX))
		{
			cmd.Session = parts[1].Substring(SESSION_PREFIX.Length(), parts[1].Length() - SESSION_PREFIX.Length());
			parts.RemoveOrdered(1);
			if (parts.Count() < 2 || !IsClassToken(cmd.Session))
			{
				cmd.Error = "bad_session";
				return false;
			}
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
		if (cmd.Verb == "state")
			return true;
		if (cmd.Verb == "die")
			return true;
		if (cmd.Verb == "giveup")
			return true;
		if (cmd.Verb == "radio")
			return true;
		if (cmd.Verb == "scanner" || cmd.Verb == "drill" || cmd.Verb == "keycard" || cmd.Verb == "charge" || cmd.Verb == "apples" || cmd.Verb == "crate" || cmd.Verb == "moonshine" || cmd.Verb == "stash" || cmd.Verb == "park")
			return true;
		if (cmd.Verb == "lockpick")
			return true;
		if (cmd.Verb == "peers")
			return true;
		if (cmd.Verb == "shoot")
			return ParseShoot(parts, cmd);
		if (cmd.Verb == "siren")
			return ParseSiren(parts, cmd);
		if (cmd.Verb == "ui")
			return ParseUi(parts, cmd);
		if (cmd.Verb == "click")
			return ParseClick(parts, cmd);
		if (cmd.Verb == "type")
			return ParseType(parts, cmd);
		if (cmd.Verb == "capture")
			return ParseCapture(parts, cmd);
		if (cmd.Verb == "film")
			return ParseFilm(parts, cmd);

		cmd.Error = "unknown_verb";
		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool ParseUse(array<string> parts, LF_AgentCommand cmd)
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
	//! ui lists clickable names; ui text also lists every field and line of an open UI kit sheet.
	protected static bool ParseUi(array<string> parts, LF_AgentCommand cmd)
	{
		if (parts.Count() < 3)
			return true;

		cmd.Arg1 = parts[2];
		if (cmd.Arg1 != "text")
		{
			cmd.Error = "bad_ui_mode";
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool ParseInteract(array<string> parts, LF_AgentCommand cmd)
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
	protected static bool ParseClick(array<string> parts, LF_AgentCommand cmd)
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
	//! type <PartName> <Text>: both tokens are letters, digits, or underscore.
	protected static bool ParseType(array<string> parts, LF_AgentCommand cmd)
	{
		if (parts.Count() < 4)
		{
			cmd.Error = "need_field_and_text";
			return false;
		}

		cmd.Arg1 = parts[2];
		cmd.Arg2 = parts[3];
		if (!IsClassToken(cmd.Arg1) || !IsClassToken(cmd.Arg2))
		{
			cmd.Error = "bad_field_or_text";
			return false;
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
	protected static bool ParseKit(array<string> parts, LF_AgentCommand cmd)
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
	//! `siren <EntityName> <mode>`: mode is an index into the vehicle's siren modes (0 is off).
	protected static bool ParseSiren(array<string> parts, LF_AgentCommand cmd)
	{
		if (parts.Count() < 4 || !IsClassToken(parts[2]))
		{
			cmd.Error = "need_entity_and_mode";
			return false;
		}

		cmd.Arg1 = parts[2];
		cmd.Arg2 = parts[3];
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool ParseShoot(array<string> parts, LF_AgentCommand cmd)
	{
		if (parts.Count() < 3 || parts[2].ToInt() <= 0)
		{
			cmd.Error = "need_player_id";
			return false;
		}

		cmd.Arg1 = parts[2];
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool ParseStand(array<string> parts, LF_AgentCommand cmd)
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

		// Optional side: e (default), w, n, s. Stand on that side of the entity, facing it.
		cmd.Arg2 = "e";
		if (parts.Count() > 3)
		{
			cmd.Arg2 = parts[3];
			if (cmd.Arg2 != "e" && cmd.Arg2 != "w" && cmd.Arg2 != "n" && cmd.Arg2 != "s")
			{
				cmd.Error = "bad_side";
				return false;
			}
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool ParseInput(array<string> parts, LF_AgentCommand cmd)
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

		// Optional value, for an axis such as CharacterTurnUp (negative looks down).
		if (parts.Count() > 3)
			cmd.Arg2 = parts[3];

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool ParseOpen(array<string> parts, LF_AgentCommand cmd)
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
	protected static bool ParseCapture(array<string> parts, LF_AgentCommand cmd)
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
	//! film start <Name> <fps> [maxSeconds] | film stop | film status (LF_FilmRecorder).
	protected static bool ParseFilm(array<string> parts, LF_AgentCommand cmd)
	{
		if (parts.Count() < 3)
		{
			cmd.Error = "need_film_mode";
			return false;
		}

		cmd.Arg1 = parts[2];
		if (cmd.Arg1 == "stop" || cmd.Arg1 == "status")
			return true;
		if (cmd.Arg1 != "start")
		{
			cmd.Error = "bad_film_mode";
			return false;
		}

		if (parts.Count() < 5)
		{
			cmd.Error = "need_film_name_fps";
			return false;
		}

		cmd.Arg2 = parts[3];
		cmd.Arg3 = parts[4];
		if (!IsClassToken(cmd.Arg2))
		{
			cmd.Error = "bad_film_name";
			return false;
		}
		if (!IsIntToken(cmd.Arg3))
		{
			cmd.Error = "bad_film_fps";
			return false;
		}

		if (parts.Count() >= 6)
		{
			cmd.Arg4 = parts[5];
			if (!IsIntToken(cmd.Arg4))
			{
				cmd.Error = "bad_film_seconds";
				return false;
			}
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
	//! One `ui text` entry, kind:name=text. The entries are joined with " | " on one result line, so
	//! a bar in the text becomes a slash and line breaks become spaces.
	static string UiEntry(string kind, string name, string text)
	{
		string clean = text;
		clean.Replace("|", "/");
		clean.Replace("\r", "");
		clean.Replace("\n", " ");
		return kind + ":" + name + "=" + clean;
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
		if (actionName == "LF_OpenDebugMenu")
			return true;
		// The melee swing: what a player presses to hit a rock or a tree with the held tool.
		if (actionName == "CharacterMelee")
			return true;
		// The inventory key: opens the inventory screen, a menu.
		if (actionName == "Inventory")
			return true;
		// Draw the tool from a weapon slot, and the first/third person camera key.
		if (actionName == "SwitchWeaponCategory1" || actionName == "SwitchWeaponCategory2" || actionName == "SwitchWeaponCategory3")
			return true;
		if (actionName == "SwitchCameraType")
			return true;
		// Look up or down (mouse pitch), so a swing can reach a low rock.
		if (actionName == "CharacterTurnUp")
			return true;
		return false;
	}
}

class LF_AgentHarness
{
	protected static const string FEATURE = "AgentHarness";
	//! Result token when LF_DevGate refuses a harness RPC (playtest mission, non-admin).
	static const string REFUSED_DEV_GATE = "dev_gate_closed";
	protected static const string COMMAND_PATH = "$profile:agent-box/command.txt";
	protected static const string RESULT_PATH = "$profile:agent-box/result.txt";
	//! Root widget name of LF_UITheme.FRAME_LAYOUT: everything a UI kit sheet draws sits under it.
	protected static const string KIT_FRAME = "LF_UIFrame";
	protected static const int UI_TEXT_MAX = 80;
	//! stand puts the character this far along +X from the entity, facing it.
	static const float STAND_SIDE = 1.2;
	//! Metres in front of a sign's face (its local +Z) where stand puts the reader.
	static const float SIGN_STAND_FRONT = 1.4;
	//! How far an unnamed part may sit from the named prop it belongs to (CollectActionEntity).
	static const float PART_REACH = 1.5;

	protected static bool s_bLoop;
	protected static IEntity s_pLoopMode;
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
		if (!GetGame())
			return;

		// Workbench keeps script statics between play sessions when nothing
		// recompiles. The old poll died with the old game, so a second Play
		// answered every command with no_session. A new game mode entity marks
		// a new session.
		IEntity mode = GetGame().GetGameMode();
		if (s_bLoop && s_pLoopMode == mode)
			return;

		if (s_bLoop)
		{
			s_bLoop = false;
			s_iEnsureTries = 0;
			s_iCaptureId = 0;
			s_bShotPose = false;
			LF_Debug.Log(FEATURE, "new play session; restarting poll");
		}

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
				LF_Debug.Warn(FEATURE, "no local controller after 30s; poll not started");
				return;
			}

			GetGame().GetCallqueue().CallLater(EnsureLoop, 500, false);
			return;
		}

		s_bLoop = true;
		s_pLoopMode = mode;
		GetGame().GetCallqueue().CallLater(Poll, 400, true);
		LF_Debug.Log(FEATURE, "poll started");
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
			LF_Debug.Error(FEATURE, "cannot read command file");
			return;
		}

		string line;
		handle.ReadLine(line);
		handle.Close();
		FileIO.DeleteFile(COMMAND_PATH);

		LF_AgentCommand cmd;
		if (!LF_AgentCommand.Parse(line, cmd))
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

		string own = OwnSession();
		if (!LF_AgentCommand.SessionAccepts(own, cmd.Session))
		{
			LF_Debug.Warn(FEATURE, string.Format("refused command %1 for session '%2'; this client is session '%3'", cmd.Id, cmd.Session, own));
			WriteResult(cmd.Id, "error", "wrong_session");
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
	//! This client's session id from -lfSession (agent-box/session.mjs up), or empty.
	protected static string OwnSession()
	{
		string value;
		if (!System.GetCLIParam("lfSession", value))
			return "";
		return value;
	}

	//------------------------------------------------------------------------------------------------
	protected static void FinishCapture()
	{
		if (s_iCaptureId <= 0)
			return;
		if (LF_ScreenshotCapture.IsBusy())
			return;

		WriteResult(s_iCaptureId, LF_ScreenshotCapture.GetLastResult(), LF_ScreenshotCapture.GetLastPath());
		s_iCaptureId = 0;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Dispatch(LF_AgentCommand cmd)
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
		if (cmd.Verb == "state")
		{
			DoState(cmd);
			return;
		}
		if (cmd.Verb == "die")
		{
			DoDie(cmd);
			return;
		}
		if (cmd.Verb == "giveup")
		{
			DoGiveUp(cmd);
			return;
		}
		if (cmd.Verb == "radio")
		{
			DoRadio(cmd);
			return;
		}
		if (cmd.Verb == "scanner" || cmd.Verb == "drill" || cmd.Verb == "keycard" || cmd.Verb == "charge" || cmd.Verb == "apples" || cmd.Verb == "crate" || cmd.Verb == "moonshine" || cmd.Verb == "stash" || cmd.Verb == "park")
		{
			SCR_ChimeraCharacter scannerCharacter = LF_Utils.GetLocalCharacter();
			if (!scannerCharacter)
				WriteResult(cmd.Id, "error", "no_character");
			else
				scannerCharacter.LF_AskHarnessScanner(cmd.Id, cmd.Verb);
			return;
		}
		if (cmd.Verb == "lockpick")
		{
			SCR_ChimeraCharacter pickCharacter = LF_Utils.GetLocalCharacter();
			if (!pickCharacter)
				WriteResult(cmd.Id, "error", "no_character");
			else
				pickCharacter.LF_AskHarnessScanner(cmd.Id, cmd.Verb);
			return;
		}
		if (cmd.Verb == "peers")
		{
			DoPeers(cmd);
			return;
		}
		if (cmd.Verb == "shoot")
		{
			DoShoot(cmd);
			return;
		}
		if (cmd.Verb == "siren")
		{
			DoSiren(cmd);
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
		if (cmd.Verb == "type")
		{
			DoType(cmd);
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
		if (cmd.Verb == "film")
		{
			DoFilm(cmd);
			return;
		}

		WriteResult(cmd.Id, "error", "unknown_verb");
	}

	//------------------------------------------------------------------------------------------------
	//! Same listener the F key uses: SCR_InteractionHandlerComponent.ActionPerform.
	//! next/prev are the SelectAction wheel, which moves the highlighted entry.
	protected static void DoInteract(LF_AgentCommand cmd)
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

		LF_Debug.Log(FEATURE, string.Format("interact %1", cmd.Arg1));
		WriteResult(cmd.Id, "ok", cmd.Arg1);
	}

	//------------------------------------------------------------------------------------------------
	protected static void DoStand(LF_AgentCommand cmd)
	{
		SCR_ChimeraCharacter character = LF_Utils.GetLocalCharacter();
		if (!character)
		{
			WriteResult(cmd.Id, "error", "no_character");
			return;
		}

		// A replicated NPC (Npc_Base.et, Runtime override) is built on a client from its prefab and
		// carries no layer name there, so only the server can resolve every name.
		BaseWorld world = GetGame().GetWorld();
		if (!world || (Replication.IsServer() && !ResolveNamed(cmd.Arg1)))
		{
			WriteResult(cmd.Id, "error", "entity_not_found");
			return;
		}

		// The server answers: ok once it moved the character, or the refusal.
		character.LF_AskHarnessStand(cmd.Id, cmd.Arg1, cmd.Arg2);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: where the character should stand beside a named entity. The dev gate is checked
	//! first, so a closed gate refuses before anything about the character or the world is read.
	//! The move itself happens on the character's owner (SCR_ChimeraCharacter.LF_AskHarnessStand).
	//! \param character The requesting player's character.
	//! \param entityName World entity to stand beside.
	//! \param gateOpen LF_DevGate allowed this player.
	//! \param[out] destination Where to put the character.
	//! \return empty on success, else the refusal token sent back to the harness.
	static string ServerStand(IEntity character, string entityName, bool gateOpen, out vector destination, string side = "e")
	{
		if (!gateOpen)
			return REFUSED_DEV_GATE;

		if (!character)
			return "no_character";

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return "no_world";

		IEntity target = ResolveNamed(entityName);
		if (!target)
		{
			LF_Debug.Error(FEATURE, string.Format("stand refused: %1 is not in the world", entityName));
			return "entity_not_found";
		}

		// One metre to the side keeps the character inside a 3m action radius
		// without planting them inside the prop.
		destination = target.GetOrigin() + StandOffset(side);

		// A sign is read from the front: a wall plate or fascia hangs above the
		// pavement with the wall right behind it, so beside it is inside the wall
		// or in the air. Stand on the ground in front of its face instead.
		if (target.FindComponent(LF_SignComponent))
		{
			vector transform[4];
			target.GetWorldTransform(transform);
			vector front = target.GetOrigin() + transform[2] * SIGN_STAND_FRONT;
			front[1] = world.GetSurfaceY(front[0], front[2]);
			destination = front;
		}

		LF_Debug.Log(FEATURE, string.Format("stand beside %1 at %2", entityName, destination));
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Yaw in degrees that faces the stand target from where stand puts the character, so the
	//! interaction key (which aims along the view) reaches the entity after a stand.
	static float StandYaw(string side = "e")
	{
		vector stand;
		vector angles;
		LF_AgentCommand.CameraStand(vector.Zero, StandOffset(side), stand, angles);
		return angles[0];
	}

	//------------------------------------------------------------------------------------------------
	//! Where stand puts the character relative to the entity: STAND_SIDE metres east (+X, the
	//! default), west, north (+Z), or south.
	static vector StandOffset(string side)
	{
		if (side == "w")
			return Vector(-STAND_SIDE, 0, 0);
		if (side == "n")
			return Vector(0, 0, STAND_SIDE);
		if (side == "s")
			return Vector(0, 0, -STAND_SIDE);

		return Vector(STAND_SIDE, 0, 0);
	}

	//------------------------------------------------------------------------------------------------
	//! The server reads the state; the reply arrives through Reply.
	protected static void DoState(LF_AgentCommand cmd)
	{
		SCR_ChimeraCharacter character = LF_Utils.GetLocalCharacter();
		if (!character)
		{
			WriteResult(cmd.Id, "error", "no_character");
			return;
		}

		character.LF_AskHarnessState(cmd.Id);
	}

	//------------------------------------------------------------------------------------------------
	//! Harness `shoot <playerId>` (dev gate): the server deals one lethal kinetic hit from the local
	//! character to that player's body through the vanilla damage path (HandleDamage), so the safe
	//! zone hook and the murder hook see it as they would see a bullet.
	protected static void DoShoot(LF_AgentCommand cmd)
	{
		SCR_ChimeraCharacter character = LF_Utils.GetLocalCharacter();
		if (!character)
		{
			WriteResult(cmd.Id, "error", "no_character");
			return;
		}

		character.LF_AskHarnessShoot(cmd.Id, cmd.Arg1.ToInt());
	}

	//------------------------------------------------------------------------------------------------
	//! Harness `siren <EntityName> <mode>` (dev gate): the server sets the named vehicle's siren mode
	//! through the same rule the driver's key uses (LF_SirenManagerComponent.ApplyRequest).
	protected static void DoSiren(LF_AgentCommand cmd)
	{
		SCR_ChimeraCharacter character = LF_Utils.GetLocalCharacter();
		if (!character)
		{
			WriteResult(cmd.Id, "error", "no_character");
			return;
		}

		character.LF_AskHarnessSiren(cmd.Id, cmd.Arg1, cmd.Arg2.ToInt());
	}

	//------------------------------------------------------------------------------------------------
	//! Kill the local character on the server (dev gate), so a session can prove the death and
	//! hospital respawn path. The death screen opens as for any death.
	protected static void DoDie(LF_AgentCommand cmd)
	{
		SCR_ChimeraCharacter character = LF_Utils.GetLocalCharacter();
		if (!character)
		{
			WriteResult(cmd.Id, "error", "no_character");
			return;
		}

		character.LF_AskHarnessDie(cmd.Id);
	}

	//------------------------------------------------------------------------------------------------
	//! Press the give-up key for the local player: the same request the key sends (#256). The server
	//! decides; read the outcome with `state`.
	protected static void DoGiveUp(LF_AgentCommand cmd)
	{
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller)
		{
			WriteResult(cmd.Id, "error", "no_controller");
			return;
		}

		controller.LF_AskGiveUp();
		WriteResult(cmd.Id, "ok", "giveup_sent");
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one line of key=value tokens for the harness `state` verb:
	//! faction, duty, wanted, rank, cash carried, bank balance, job, organization.
	static string StateLine(IEntity character)
	{
		if (!character)
			return "no_character";

		string uid = LF_Utils.GetPlayerUID(character);
		LF_PlayerAccount account;
		LF_PlayerAccountManager manager = LF_PlayerAccountManager.GetInstance();
		if (manager && !uid.IsEmpty())
			account = manager.GetAccount(uid);

		string faction = "none";
		int duty = 0;
		int wanted = 0;
		int rank = 0;
		if (account)
		{
			faction = typename.EnumToString(LF_Faction, account.GetFaction());
			if (account.IsOnDuty())
				duty = 1;
			wanted = account.GetWantedLevel();
			rank = account.GetPoliceRank();
		}

		int bank = 0;
		if (!uid.IsEmpty())
			bank = LF_ATMManager.GetOrCreate(uid).GetBalance();

		// The job the player signed up for (the counter sets it on the job component); the active
		// run alone read UNEMPLOYED between runs, so a sign-up never showed in state.
		LF_EJobType jobType = LF_CivilianJob.GetActiveJob(uid);
		LF_PlayerJobComponent jobs = LF_Component<LF_PlayerJobComponent>.Find(character);
		if (jobs && jobType == LF_EJobType.UNEMPLOYED)
			jobType = jobs.GetJob();
		string job = typename.EnumToString(LF_EJobType, jobType);
		string org = "none";
		LF_Organization organization = LF_OrganizationManager.GetByCharacter(LF_Hideout.CharacterIdOf(character));
		if (organization)
			org = Token(organization.GetName());

		vector origin = character.GetOrigin();
		string pos = string.Format("%1,%2,%3", Math.Round(origin[0] * 10) / 10, Math.Round(origin[1] * 10) / 10, Math.Round(origin[2] * 10) / 10);
		int bills = 0;
		array<IEntity> items = {};
		SCR_InventoryStorageManagerComponent inventory = LF_Component<SCR_InventoryStorageManagerComponent>.Find(character);
		if (inventory)
			inventory.GetItems(items);
		array<IEntity> counted = {};
		foreach (IEntity item : items)
		{
			if (item && counted.Find(item) < 0 && item.FindComponent(LF_HospitalBillComponent))
			{
				counted.Insert(item);
				bills++;
			}
		}

		string line = string.Format("faction=%1 duty=%2 wanted=%3 rank=%4 cash=%5 bank=%6 job=%7 org=%8", faction, duty, wanted, rank, LF_MoneyUtils.GetCash(character), bank, job, org);
		line += " player=" + LF_PlayerInteraction.PlayerIdOf(character).ToString();
		// Police tickets carried: unpaid count, and the amount owed on the oldest unpaid one.
		int unpaidTickets = 0;
		array<LF_FineTicketComponent> tickets = {};
		LF_FineTicketItem.FindCarried(character, tickets);
		foreach (LF_FineTicketComponent ticket : tickets)
		{
			if (ticket.IsPrinted() && !ticket.IsPaid())
				unpaidTickets++;
		}

		int owed = 0;
		LF_FineTicketComponent firstUnpaid = LF_FineTicketItem.FirstUnpaid(character);
		if (firstUnpaid)
			owed = firstUnpaid.GetAmount();

		line += string.Format(" pos=%1 bills=%2 tickets=%3 owed=%4", pos, bills, unpaidTickets, owed);
		int downed = 0;
		if (LF_Downed.IsDowned(character))
			downed = 1;
		line += " zone=" + LF_Zones.RuleName(LF_Zones.RuleAt(origin));
		line += string.Format(" downed=%1 life=%2 bleed=%3", downed, LifeStateToken(character), BleedToken(character));
		line += " " + VitalsToken(character);
		line += string.Format(" idcard=%1 quests=%2", IdCardToken(character, account), QuestToken(character));
		return line + " " + KeepsToken(uid);
	}

	//------------------------------------------------------------------------------------------------
	//! What the restart judge reads besides money (#348): seconds left on a sentence, the stash's item
	//! count (-1 with no stash placed), cars stored in the garage, and the plate of the car in the world.
	protected static string KeepsToken(string uid)
	{
		int jail = 0;
		int stored = 0;
		string parked = "none";
		if (!uid.IsEmpty())
		{
			if (LF_Jail.IsServing(uid))
				jail = Math.Round(LF_Jail.GetRemaining(uid));

			stored = LF_VehicleOwnership.StoredFor(uid).Count();
			string worldId = LF_VehicleOwnership.LatestInWorld(uid);
			if (!worldId.IsEmpty())
				parked = Token(LF_VehicleOwnership.GetPlate(worldId));
		}

		int stash = -1;
		IEntity stashEntity = GetGame().GetWorld().FindEntityByName("Roadmap_Hideout_Stash");
		if (stashEntity)
		{
			BaseInventoryStorageComponent storage = LF_Component<LF_HideoutStashStorageComponent>.Find(stashEntity);
			if (storage)
			{
				array<IEntity> stashed = {};
				storage.GetAll(stashed, false);
				stash = stashed.Count();
			}
		}

		return string.Format("jail=%1 garage=%2 parked=%3 stash=%4", jail, stored, parked, stash);
	}

	//------------------------------------------------------------------------------------------------
	//! Resilience, blood and health (scaled, 2 decimals) as THIS machine reads them (#427).
	static string VitalsToken(IEntity entity)
	{
		SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(entity.FindComponent(SCR_CharacterDamageManagerComponent));
		if (!damage)
			return "res=none blood=none hp=none";

		return string.Format("res=%1 blood=%2 hp=%3", Math.Round(damage.GetResilienceHitZone().GetHealthScaled() * 100) / 100, Math.Round(damage.GetBloodHitZone().GetHealthScaled() * 100) / 100, Math.Round(damage.GetDefaultHitZone().GetHealthScaled() * 100) / 100);
	}

	//! Seconds of bleed-out this client reads for the character, -1 when it is not downed (#256).
	protected static int BleedToken(IEntity entity)
	{
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(entity);
		if (!character)
			return -1;

		return character.LF_GetBleedSecondsLeft();
	}

	//------------------------------------------------------------------------------------------------
	//! "Name_Surname" of the card the character would show, else "none".
	protected static string IdCardToken(IEntity character, LF_PlayerAccount account)
	{
		string holderId;
		if (account && account.GetActiveCharacter())
			holderId = account.GetActiveCharacter().GetId();

		LF_IdCardComponent card = LF_IdCard.FindShown(character, holderId);
		if (!card)
			return "none";

		return Token(card.GetFirstName() + "_" + card.GetLastName());
	}

	//------------------------------------------------------------------------------------------------
	//! Turned-in quest ids joined with "," (the arrival chain progress), else "none".
	protected static string QuestToken(IEntity character)
	{
		LF_QuestTrackerComponent tracker = LF_QuestTrackerComponent.Cast(character.FindComponent(LF_QuestTrackerComponent));
		if (!tracker)
			return "none";

		array<string> ids = tracker.GetTurnedInQuestIds();
		string joined;
		foreach (string questId : ids)
		{
			if (!joined.IsEmpty())
				joined += ",";
			joined += questId;
		}

		return Token(joined);
	}

	//------------------------------------------------------------------------------------------------
	//! One space-free token for a key=value result.
	protected static string Token(string text)
	{
		string token = text;
		token.Replace(" ", "_");
		if (token.IsEmpty())
			return "none";
		return token;
	}

	//------------------------------------------------------------------------------------------------
	//! Harness `radio`: the radios THIS client's own character carries, as this client reads them:
	//! `radios=N f=<kHz of the first transceiver>,...`. Proves the duty tuning replicated.
	protected static void DoRadio(LF_AgentCommand cmd)
	{
		PlayerController controller = GetGame().GetPlayerController();
		IEntity body;
		if (controller)
			body = controller.GetControlledEntity();

		array<BaseRadioComponent> radios = {};
		LF_ServiceRadio.FindCarried(body, radios);
		string freqs;
		foreach (BaseRadioComponent radio : radios)
		{
			if (!freqs.IsEmpty())
				freqs += ",";

			BaseTransceiver transceiver = radio.GetTransceiver(0);
			if (transceiver)
				freqs += transceiver.GetFrequency().ToString();
			else
				freqs += "none";
		}

		WriteResult(cmd.Id, "ok", string.Format("radios=%1 f=%2", radios.Count(), freqs));
	}

	//------------------------------------------------------------------------------------------------
	//! Harness `peers`: what THIS client sees of every player body it has streamed in: the
	//! replicated downed flag and the vanilla life state. A late joiner reads a downed player here.
	protected static void DoPeers(LF_AgentCommand cmd)
	{
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
		{
			WriteResult(cmd.Id, "error", "no_player_manager");
			return;
		}

		array<int> ids = {};
		playerManager.GetPlayers(ids);
		string joined;
		foreach (int id : ids)
		{
			IEntity body = playerManager.GetPlayerControlledEntity(id);
			if (!body)
				continue;

			int downed = 0;
			if (LF_Downed.IsDowned(body))
				downed = 1;

			if (!joined.IsEmpty())
				joined += " | ";

			joined += string.Format("player=%1 downed=%2 life=%3", id, downed, LifeStateToken(body));
			// #261: who has this body (replicated), whether it sits in a vehicle seat, where it is.
			int carrier = 0;
			SCR_ChimeraCharacter peer = SCR_ChimeraCharacter.Cast(body);
			if (peer)
				carrier = peer.LF_GetCarrierPlayerId();

			int seated = 0;
			if (SCR_CompartmentAccessComponent.GetVehicleIn(body))
				seated = 1;

			vector at = body.GetOrigin();
			joined += string.Format(" carrier=%1 seat=%2 at=%3,%4,%5", carrier, seated, Math.Round(at[0] * 10) / 10, Math.Round(at[1] * 10) / 10, Math.Round(at[2] * 10) / 10);
		}

		if (joined.IsEmpty())
			joined = "none";

		WriteResult(cmd.Id, "ok", joined);
	}

	//------------------------------------------------------------------------------------------------
	//! ALIVE, INCAPACITATED (vanilla unconscious), DEAD, or "none" for a body without a controller.
	static string LifeStateToken(IEntity entity)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(entity);
		if (!character)
			return "none";

		CharacterControllerComponent controller = character.GetCharacterController();
		if (!controller)
			return "none";

		return typename.EnumToString(ECharacterLifeState, controller.GetLifeState());
	}

	//------------------------------------------------------------------------------------------------
	//! Actions whose CanBeShownScript is true within 4m. That is the F-key list.
	protected static void DoLook(LF_AgentCommand cmd)
	{
		IEntity user = LF_Utils.GetLocalCharacter();
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

		IEntity user = LF_Utils.GetLocalCharacter();
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
	protected static void DoUi(LF_AgentCommand cmd)
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
		{
			WriteResult(cmd.Id, "error", "no_workspace");
			return;
		}

		s_sUiJoined = "";
		s_iUiCount = 0;
		if (cmd.Arg1 == "text")
			CollectUiText(workspace, false);
		else
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
	//! ui text: every clickable with its label, and every field and printed line of an open UI kit
	//! sheet, named by the part a screen gave it (the name click and type take).
	protected static void CollectUiText(Widget widget, bool inButton)
	{
		if (!widget || s_iUiCount >= UI_TEXT_MAX)
			return;

		if (!Shown(widget))
			return;

		bool button = false;
		if (!inButton && Clickable(widget))
		{
			button = true;
			AddUiText(LF_AgentCommand.UiEntry("button", widget.GetName(), Translated(ButtonLabel(widget))));
		}
		else if (!inButton && InKitSheet(widget))
		{
			EditBoxWidget box = EditBoxWidget.Cast(widget);
			TextWidget text = TextWidget.Cast(widget);
			if (box)
				AddUiText(LF_AgentCommand.UiEntry("field", KitPartName(widget), box.GetText()));
			else if (text && !text.GetText().IsEmpty())
				AddUiText(LF_AgentCommand.UiEntry("line", KitPartName(widget), Translated(text.GetText())));
		}

		Widget child = widget.GetChildren();
		while (child)
		{
			CollectUiText(child, inButton || button);
			child = child.GetSibling();
		}
	}

	//------------------------------------------------------------------------------------------------
	//! What a button shows: the first visible non-empty text inside it. SCR_ButtonTextComponent
	//! GetText() returned the layout default ("Back to main menu") on the faction menu.
	protected static string ButtonLabel(Widget button)
	{
		string shown = FirstText(button.GetChildren());
		if (!shown.IsEmpty())
			return shown;

		SCR_ButtonTextComponent textButton = SCR_ButtonTextComponent.Cast(button.FindHandler(SCR_ButtonTextComponent));
		if (textButton)
			return textButton.GetText();

		return "";
	}

	//------------------------------------------------------------------------------------------------
	protected static string FirstText(Widget widget)
	{
		while (widget)
		{
			if (widget.IsVisible())
			{
				TextWidget text = TextWidget.Cast(widget);
				if (text && !text.GetText().IsEmpty())
					return text.GetText();

				string inner = FirstText(widget.GetChildren());
				if (!inner.IsEmpty())
					return inner;
			}

			widget = widget.GetSibling();
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	protected static void AddUiText(string entry)
	{
		if (!s_sUiJoined.IsEmpty())
			s_sUiJoined = s_sUiJoined + " | ";
		s_sUiJoined = s_sUiJoined + entry;
		s_iUiCount++;
	}

	//------------------------------------------------------------------------------------------------
	protected static string Translated(string text)
	{
		if (text.StartsWith("#"))
			return WidgetManager.Translate(text);
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! True inside a sheet built by LF_UIScreen (its frame layout root is LF_UIFrame).
	protected static bool InKitSheet(Widget widget)
	{
		Widget cursor = widget;
		while (cursor)
		{
			if (cursor.GetName() == KIT_FRAME)
				return true;
			cursor = cursor.GetParent();
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! The kit part a widget belongs to: the ancestor that sits directly in Content or ListContent.
	protected static string KitPartName(Widget widget)
	{
		Widget cursor = widget;
		while (cursor && cursor.GetParent())
		{
			string parentName = cursor.GetParent().GetName();
			if (parentName == "Content" || parentName == "ListContent")
				return cursor.GetName();
			cursor = cursor.GetParent();
		}

		return widget.GetName();
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
	protected static void DoClick(LF_AgentCommand cmd)
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
			LF_Debug.Log(FEATURE, string.Format("click %1", s_sClickName));
			WriteResult(cmd.Id, "ok", s_sClickName);
			return;
		}

		// Debug menu buttons are plain ButtonWidgets. The menu class is not a
		// widget handler on them, so the parent walk never reaches OnClick.
		MenuManager menuManager = GetGame().GetMenuManager();
		if (menuManager)
		{
			LF_DebugMenu debugMenu = LF_DebugMenu.Cast(menuManager.FindMenuByPreset(ChimeraMenuPreset.DebugMenu));
			if (debugMenu && debugMenu.OnClick(s_wClickTarget, 0, 0, 0))
			{
				LF_Debug.Log(FEATURE, string.Format("click %1", s_sClickName));
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
				LF_Debug.Log(FEATURE, string.Format("click %1", s_sClickName));
				WriteResult(cmd.Id, "ok", s_sClickName);
				return;
			}

			cursor = cursor.GetParent();
		}

		WriteResult(cmd.Id, "error", "click_unhandled");
	}

	//------------------------------------------------------------------------------------------------
	//! Writes text into the edit box of a visible field (a UI kit FIELD part or any widget that
	//! wraps an EditBox), the way a player would type it.
	protected static void DoType(LF_AgentCommand cmd)
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
		{
			WriteResult(cmd.Id, "error", "no_workspace");
			return;
		}

		Widget field = workspace.FindAnyWidget(cmd.Arg1);
		if (!field || !Shown(field))
		{
			WriteResult(cmd.Id, "error", "widget_not_found");
			return;
		}

		EditBoxWidget box = EditBoxWidget.Cast(field);
		if (!box)
			box = EditBoxWidget.Cast(field.FindAnyWidget("EditBox"));
		if (!box)
		{
			WriteResult(cmd.Id, "error", "not_a_field");
			return;
		}

		box.SetText(cmd.Arg2);
		LF_Debug.Log(FEATURE, string.Format("type %1", cmd.Arg1));
		WriteResult(cmd.Id, "ok", cmd.Arg1);
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
		if (!LF_AgentCommand.IsClassToken(name))
			return false;
		// Every UI kit button (BUTTON, CLOSE, KEYS, a status or list row action) carries the kit's
		// click relay, whatever the screen named it (IdClose, Exit, Buy, Key1Button).
		if (LF_UIButtonRelay.Cast(widget.FindHandler(LF_UIButtonRelay)))
			return true;
		// A UI kit list row's action button is <row>_Action (the shop's Order buttons).
		bool kitRowAction = name.Length() > 7 && name.Substring(name.Length() - 7, 7) == "_Action";
		if (kitRowAction)
			return SCR_ButtonTextComponent.Cast(widget.FindHandler(SCR_ButtonTextComponent)) != null;

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
				if (typeName.IndexOf("LF_") == 0 || typeName.IndexOf("LF_") == 0)
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

		LF_DebugMenu menu = LF_DebugMenu.Cast(menuManager.FindMenuByPreset(ChimeraMenuPreset.DebugMenu));
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
	protected static void DoUse(LF_AgentCommand cmd)
	{
		SCR_ChimeraCharacter character = LF_Utils.GetLocalCharacter();
		if (!character)
		{
			WriteResult(cmd.Id, "error", "no_character");
			return;
		}

		// A local-effect action (ATM terminal, shop price list) runs PerformAction only on the
		// acting client when the F key uses it, so the harness performs it here the same way. Only this
		// mod's actions: a vanilla door is local-effect too, but its perform asks the server.
		// Vanilla pick up and equip run on the acting client too: the inventory manager there
		// sends the server request, the same path the F key takes.
		bool vanillaInventory = cmd.Arg1 == "SCR_PickUpItemAction" || cmd.Arg1 == "SCR_EquipWeaponAction" || cmd.Arg1 == "SCR_EquipWeaponHolsterAction";
		if (!Replication.IsServer() && vanillaInventory)
		{
			IEntity item = FindTarget(character, cmd.Arg1, cmd.Arg2);
			if (!item && cmd.Arg2 != "")
				item = FindTarget(character, cmd.Arg1, "");
			ScriptedUserAction itemAction;
			if (item)
				itemAction = FindShownOnEntity(item, cmd.Arg1, character);
			if (!itemAction)
			{
				WriteResult(cmd.Id, "error", "cannot_show");
				return;
			}

			if (!itemAction.CanBePerformedScript(character))
			{
				WriteResult(cmd.Id, "error", "cannot_perform");
				return;
			}

			itemAction.PerformAction(item, character);
			LF_Debug.Log(FEATURE, string.Format("performed %1 on this client", cmd.Arg1));
			WriteResult(cmd.Id, "ok", cmd.Arg1);
			return;
		}

		if (!Replication.IsServer() && (cmd.Arg1.StartsWith("LF_") || cmd.Arg1.StartsWith("LF_")))
		{
			IEntity target = FindTarget(character, cmd.Arg1, cmd.Arg2);
			// A replicated NPC has no layer name on a client: take the nearest host of the action.
			if (!target && cmd.Arg2 != "")
				target = FindTarget(character, cmd.Arg1, "");
			ScriptedUserAction localAction;
			if (target)
				localAction = FindShownOnEntity(target, cmd.Arg1, character);

			if (localAction && localAction.HasLocalEffectOnlyScript())
			{
				if (!localAction.CanBeShownScript(character))
				{
					WriteResult(cmd.Id, "error", "cannot_show");
					return;
				}

				if (!localAction.CanBePerformedScript(character))
				{
					WriteResult(cmd.Id, "error", "cannot_perform");
					return;
				}

				localAction.PerformAction(target, character);
				LF_Debug.Log(FEATURE, string.Format("performed %1 on this client", cmd.Arg1));
				WriteResult(cmd.Id, "ok", cmd.Arg1);
				return;
			}
		}

		character.LF_AskHarnessUse(cmd.Id, cmd.Arg1, cmd.Arg2);
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
			UseReply(user, commandId, "error", "no_character");
			return;
		}

		IEntity target = FindTarget(user, className, entityName);
		if (!target)
		{
			UseReply(user, commandId, "error", "action_not_found");
			return;
		}

		ScriptedUserAction action = FindShownOnEntity(target, className, user);
		if (!action)
		{
			UseReply(user, commandId, "error", "action_not_found");
			return;
		}

		if (!action.CanBeShownScript(user))
		{
			UseReply(user, commandId, "error", "cannot_show");
			return;
		}

		if (!action.CanBePerformedScript(user))
		{
			UseReply(user, commandId, "error", "cannot_perform");
			return;
		}

		action.PerformAction(target, user);
		LF_Debug.Log(FEATURE, string.Format("performed %1", className));
		UseReply(user, commandId, "ok", className);
	}

	//------------------------------------------------------------------------------------------------
	protected static void DoInput(LF_AgentCommand cmd)
	{
		InputManager input = GetGame().GetInputManager();
		if (!input)
		{
			WriteResult(cmd.Id, "error", "no_input");
			return;
		}

		float value = 1;
		if (cmd.Arg2 != "")
			value = cmd.Arg2.ToFloat();

		// The melee key and the mouse pitch reach the character through its input
		// context, the same flags the command handler reads (SCR_CharacterCommandHandler.HandleMelee).
		if (cmd.Arg1 == "CharacterMelee" || cmd.Arg1 == "CharacterTurnUp")
		{
			SCR_ChimeraCharacter character = LF_Utils.GetLocalCharacter();
			CharacterControllerComponent controller;
			if (character)
				controller = character.GetCharacterController();
			if (!controller || !controller.GetInputContext())
			{
				WriteResult(cmd.Id, "error", "no_character");
				return;
			}

			CharacterInputContext context = controller.GetInputContext();
			if (cmd.Arg1 == "CharacterMelee")
			{
				context.SetMeleeAttack(true);
				vector aimNow = context.GetAimingAngles();
				character.LF_AskHarnessMelee(aimNow[0], aimNow[1]);
			}
			else
			{
				vector aim = context.GetAimingAngles();
				aim[1] = aim[1] + value * Math.DEG2RAD;
				context.SetAimingAngles(aim);
			}

			LF_Debug.Log(FEATURE, string.Format("input %1 %2", cmd.Arg1, value));
			WriteResult(cmd.Id, "ok", cmd.Arg1);
			return;
		}

		s_sHeldAction = cmd.Arg1;
		input.SetActionValue(cmd.Arg1, value);
		GetGame().GetCallqueue().CallLater(ClearInput, 80, false);
		LF_Debug.Log(FEATURE, string.Format("input %1", cmd.Arg1));
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
	protected static void DoKit(LF_AgentCommand cmd)
	{
		if (cmd.Arg1 == "show")
		{
			LF_UIScreen screen = LF_UISampleScreen.Open();
			if (!screen)
			{
				WriteResult(cmd.Id, "error", "open_failed");
				return;
			}

			LF_Debug.Log(FEATURE, "kit show");
			WriteResult(cmd.Id, "ok", "opened");
			return;
		}

		if (cmd.Arg1 == "hide")
		{
			LF_UISampleScreen.Hide();
			LF_Debug.Log(FEATURE, "kit hide");
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

			LF_Debug.Log(FEATURE, "kit line " + cmd.Arg2);
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

			LF_Debug.Log(FEATURE, "kit status " + cmd.Arg2);
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

			LF_Debug.Log(FEATURE, "kit quantity " + cmd.Arg2);
			WriteResult(cmd.Id, "ok", cmd.Arg2);
			return;
		}

		WriteResult(cmd.Id, "error", "bad_kit_step");
	}

	//------------------------------------------------------------------------------------------------
	//! Same toggle the LF_OpenDebugMenu key uses.
	protected static void DoOpen(LF_AgentCommand cmd)
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

		SCR_ChimeraCharacter character = LF_Utils.GetLocalCharacter();
		if (!character)
		{
			LF_Debug.Error(FEATURE, "world shot has no character to pose");
			return;
		}

		character.LF_AskHarnessPose(s_vShotStand[0], s_vShotStand[1], s_vShotStand[2], s_vShotAngles[0], s_vShotAngles[1]);
	}

	//------------------------------------------------------------------------------------------------
	protected static void DoCapture(LF_AgentCommand cmd)
	{
		LF_ScreenshotShot shot = new LF_ScreenshotShot();
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
			LF_AgentCommand.CameraStand(shot.TargetPosition, offset, stand, angles);
			s_vShotStand = stand;
			s_vShotAngles = angles;
			s_bShotPose = true;
			ReapplyWorldPose();
			Game game = GetGame();
			if (!game)
			{
				LF_Debug.Error(FEATURE, "world shot has no game");
			}
			else
			{
				if (game.GetWorld())
					game.BeginPreload(game.GetWorld(), shot.TargetPosition, 200);
				// Gravity drops an elevated body during the settle. Put it back
				// on the offset, then leave time for that pose to replicate
				// before MakeScreenshot. Callqueue is on the live game, not the
				// Game handle this local stores.
				GetGame().GetCallqueue().CallLater(ReapplyWorldPose, 700, false);
			}
			shot.SettleTime = 1.2;
			shot.TimeOfDay24h = 12;
			shot.WeatherStateName = "Clear";
		}

		if (!LF_ScreenshotCapture.StartCapture(shot))
		{
			WriteResult(cmd.Id, "error", LF_ScreenshotCapture.GetLastResult());
			return;
		}

		s_iCaptureId = cmd.Id;
		WriteResult(cmd.Id, "pending", cmd.Arg2);
	}

	//------------------------------------------------------------------------------------------------
	//! In-engine recording (LF_FilmRecorder). It runs beside every other verb,
	//! so a journey keeps playing while the client films itself.
	protected static void DoFilm(LF_AgentCommand cmd)
	{
		string result;
		if (cmd.Arg1 == "start")
			result = LF_FilmRecorder.Start(cmd.Arg2, cmd.Arg3.ToInt(), cmd.Arg4.ToInt());
		else if (cmd.Arg1 == "stop")
			result = LF_FilmRecorder.Stop();
		else
			result = "ok " + LF_FilmRecorder.Status();

		if (result.StartsWith("ok "))
		{
			WriteResult(cmd.Id, "ok", result.Substring(3, result.Length() - 3));
			return;
		}
		if (result.StartsWith("error:"))
		{
			WriteResult(cmd.Id, "error", result.Substring(6, result.Length() - 6));
			return;
		}
		WriteResult(cmd.Id, "error", result);
	}

	//------------------------------------------------------------------------------------------------
	//! A named world entity, or Player<id> for the character that player controls.
	static IEntity ResolveNamed(string entityName)
	{
		IEntity player = PlayerNamed(entityName);
		if (player)
			return player;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return null;

		return world.FindEntityByName(entityName);
	}

	//------------------------------------------------------------------------------------------------
	//! Player<id> (for example Player2) resolves to that player's character on this machine.
	static IEntity PlayerNamed(string entityName)
	{
		if (!entityName.StartsWith("Player") || entityName.Length() <= 6)
			return null;

		string digits = entityName.Substring(6, entityName.Length() - 6);
		int playerId = digits.ToInt();
		if (playerId <= 0 || playerId.ToString() != digits)
			return null;

		if (!GetGame().GetPlayerManager())
			return null;

		return GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
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

		// Another player is named Player<id>: their character, if it carries the action.
		IEntity player = PlayerNamed(entityName);
		if (player)
		{
			if (FindOnEntity(player, className))
				return player;

			return null;
		}

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

		// A named prop's actions can sit on an unnamed part of it (the duty desk's punch clock):
		// an unnamed entity within PART_REACH of the named one stands in for it.
		if (s_sWantedName != "" && entity.GetName() != s_sWantedName)
		{
			IEntity named = GetGame().GetWorld().FindEntityByName(s_sWantedName);
			if (!named || !entity.GetName().IsEmpty() || vector.Distance(named.GetOrigin(), entity.GetOrigin()) > PART_REACH)
				return true;
		}

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
	//! Several entries of one class on an entity (the interaction menu's role slots): the first one
	//! the user is offered, else the first of that class.
	protected static ScriptedUserAction FindShownOnEntity(IEntity entity, string className, IEntity user)
	{
		BaseActionsManagerComponent mgr = BaseActionsManagerComponent.Cast(entity.FindComponent(BaseActionsManagerComponent));
		if (!mgr)
			return null;

		array<BaseUserAction> actions = {};
		mgr.GetActionsList(actions);
		foreach (BaseUserAction action : actions)
		{
			ScriptedUserAction scripted = ScriptedUserAction.Cast(action);
			if (scripted && action.Type().ToString() == className && scripted.CanBeShownScript(user))
				return scripted;
		}

		return FindOnEntity(entity, className);
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
	protected static void UseReply(IEntity user, int commandId, string status, string detail)
	{
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(user);
		if (character)
			character.LF_HarnessReply(commandId, status, detail);
		else
			WriteResult(commandId, status, detail);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: a result the server sent back for a stand, use, or state request. A state reply
	//! also carries the last toast this client showed, which is how an alert is seen.
	static void Reply(int id, string status, string detail)
	{
		string text = detail;
		if (status == "ok" && text.StartsWith("faction="))
		{
			string toast = SCR_PlayerController.LF_GetLastToast();
			text = text + " toast=" + Token(toast);
			text = text + " known=" + LF_KnownNames.KnownCount().ToString() + " name=" + Token(LF_KnownNames.GetShown());
			// M1-20: radio dispatches this client heard, the newest, and the site alarm it can hear.
			text = text + " dispatch=" + SCR_PlayerController.LF_GetDispatchCount().ToString() + " lastdispatch=" + Token(SCR_PlayerController.LF_GetLastDispatch());
			string alarm = "none";
			IEntity listener = LF_Utils.GetLocalCharacter();
			if (listener)
				alarm = LF_SiteAlarmComponent.HeardAt(listener.GetOrigin());

			text = text + " alarm=" + alarm;
		}

		WriteResult(id, status, text);
	}

	//------------------------------------------------------------------------------------------------
	protected static void WriteResult(int id, string status, string detail)
	{
		FileIO.MakeDirectory("$profile:agent-box");
		FileHandle handle = FileIO.OpenFile(RESULT_PATH, FileMode.WRITE);
		if (!handle)
		{
			LF_Debug.Error(FEATURE, "cannot write result file");
			return;
		}

		handle.Write(string.Format("%1 %2 %3", id, status, detail));
		handle.Close();
		LF_Debug.Log(FEATURE, string.Format("result %1 %2 %3", id, status, detail));
	}
}
