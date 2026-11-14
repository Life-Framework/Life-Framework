//------------------------------------------------------------------------------------------------
//! The conversation on the talker's screen: the camera on the NPC, the NPC's line as a caption
//! at the bottom, and the replies as a numbered list. Local to one machine; the world goes on
//! around it. The scene shows what the server sent (LF_Dialogue) and sends back a reply index.
//!   keys   : 1 to 4 pick a reply; up/down and confirm pick one too (keyboard or pad);
//!            confirm skips a line that is playing; back leaves.
//! While it is open the character cannot move, look or use a weapon, and the HUD is hidden.
class LF_DialogueScene
{
	static const ResourceName LAYOUT = "{335FE329F6924CEC}UI/Layouts/HUD/LF_Dialogue.layout";
	//! Input is ignored this long after the scene changes, so the key that opened it picks nothing.
	static const int INPUT_GUARD_MS = 350; // lf-fixed: input debounce
	//! The server did not answer: give the player back to the world.
	static const int ANSWER_TIMEOUT_MS = 5000; // lf-fixed: network watchdog
	//! The caption stays this long after the last word.
	static const int LINE_TAIL_MS = 300; // lf-fixed: caption pacing

	protected static Widget s_wRoot;
	protected static EntityID s_NpcId = EntityID.INVALID;
	protected static ref LF_DialogueConfig s_Config;
	//! The last caption this scene put on screen (the harness reads it; the scene widgets are not in `ui text`).
	protected static string s_sLastCaption;
	protected static string s_sNode;
	protected static int s_iMask;
	protected static ref array<string> s_aQueue;
	//! Option index behind each shown row.
	protected static ref array<int> s_aRows;
	protected static int s_iFocus;
	protected static bool s_bChoosing;
	protected static bool s_bWaiting;
	protected static int s_iLineEndMs;
	protected static int s_iGuardUntilMs;
	protected static int s_iWaitSinceMs;
	protected static int s_iShown;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_wRoot = null;
		s_NpcId = EntityID.INVALID;
		s_Config = null;
		s_sLastCaption = string.Empty;
		s_aQueue = null;
		s_aRows = null;
		s_bChoosing = false;
		s_bWaiting = false;
		s_iShown = 0;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsOpen()
	{
		return s_NpcId != EntityID.INVALID;
	}

	//------------------------------------------------------------------------------------------------
	//! State for the harness and the logs: "closed", or node, phase and what is on screen.
	static string Describe()
	{
		if (!IsOpen())
			return "closed";

		string phase = "line";
		if (s_bWaiting)
			phase = "waiting";
		else if (s_bChoosing)
			phase = "choosing";

		int rows = 0;
		if (s_aRows)
			rows = s_aRows.Count();

		return string.Format("open node=%1 phase=%2 replies=%3 camera=%4 shown=%5", s_sNode, phase, rows, LF_DialogueCamera.IsActive(), s_iShown);
	}

	//------------------------------------------------------------------------------------------------
	//! The play-session harness (`dialogue`, `dialogue pick 0`, `dialogue skip`,
	//! `dialogue mouth 0.8`, `dialogue rig Jaw:0,8,0,0,0,0;...`): the
	//! scene's state, or a held mouth pose on the NPC of the open scene so a shot can show it.
	static string Harness(string verb, string value)
	{
		if (verb == "rig")
		{
			// Shut the mouth with the old pose first, or its bones stay where they were left.
			if (IsOpen())
				LF_DialogueVoice.SetMouth(GetGame().GetWorld().FindEntityByID(s_NpcId), 0);

			return "rig=" + LF_DialogueVoice.SetRig(value);
		}

		if (verb == "closeup")
		{
			LF_DialogueCamera.SetProofCloseUp(value == "1");
			// The caption bar would cover the mouth in the close-up: hide the scene's widgets while it runs.
			if (s_wRoot)
				s_wRoot.SetVisible(value != "1");

			return "closeup=" + value;
		}

		if (verb == "nodrig" || verb == "blinkrig")
			return verb + "=" + LF_DialogueVoice.SetExtraRig(verb == "blinkrig", value);

		if (verb == "bones")
			return LF_DialogueVoice.BoneNames(GetGame().GetWorld().FindEntityByID(s_NpcId), value);

		if (verb == "mouth")
		{
			if (!IsOpen())
				return "closed";

			LF_DialogueVoice.HoldMouth(GetGame().GetWorld().FindEntityByID(s_NpcId), value.ToFloat());
			return "mouth=" + value;
		}

		if (verb == "pick")
			return "picked=" + PickRow(value.ToInt());

		if (verb == "shown")
			return "shown=" + s_sLastCaption;

		if (verb == "caption")
			return "caption=" + LF_DialogueOverrides.Text(value) + " overrides=" + LF_DialogueOverrides.Count();

		if (verb == "skip")
		{
			SkipLine();
			return Describe();
		}

		return string.Format("%1 lines=%2 mouthframes=%3", Describe(), LF_DialogueVoice.PlayedCount(), LF_DialogueVoice.MouthFrames());
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the local player starts talking to an NPC. The scene opens at once (the camera
	//! starts to move) and the server is asked for the first node.
	static bool Begin(IEntity npc)
	{
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		LF_DialogueConfig config = LF_DialogueComponent.ConfigOf(npc);
		if (!controller || !config)
		{
			LF_Debug.Error(LF_Dialogue.FEATURE, "conversation not opened: the NPC has no playable dialogue");
			return false;
		}

		if (IsOpen())
			Close("a new conversation starts", true);

		if (!Open(npc, config))
			return false;

		s_bWaiting = true;
		s_iWaitSinceMs = System.GetTickCount();
		controller.LF_AskDialogueStart(npc);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool Open(IEntity npc, LF_DialogueConfig config)
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return false;

		s_wRoot = workspace.CreateWidgets(LAYOUT);
		if (!s_wRoot)
		{
			LF_Debug.Error(LF_Dialogue.FEATURE, "conversation not opened: the dialogue layout did not load");
			return false;
		}

		s_NpcId = npc.GetID();
		s_Config = config;
		s_sNode = "";
		s_iMask = 0;
		s_aQueue = {};
		s_aRows = {};
		s_bChoosing = false;
		s_bWaiting = false;
		s_iGuardUntilMs = System.GetTickCount() + INPUT_GUARD_MS;
		SetText("Speaker", LF_DialogueOverrides.Text(config.m_sSpeakerKey));
		SetText("Caption", "");
		SetText("Hint", "");
		ShowRows();
		Freeze(true);
		if (!LF_DialogueCamera.Start(npc))
			LF_Debug.Warn(LF_Dialogue.FEATURE, "the conversation runs without its camera");

		LF_DialogueVoice.Attend(npc, true);
		GetGame().GetCallqueue().CallLater(Tick, 0, true);
		LF_Debug.Info(LF_Dialogue.FEATURE, string.Format("scene open with '%1'", config.m_sId));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the server says where the conversation is. `spoken` is what the NPC says now (line
	//! ids, or bare caption keys), `mask` the replies offered at `nodeId` (none: it ends after).
	static void Show(IEntity npc, string nodeId, string spoken, int mask)
	{
		if (!npc)
		{
			LF_Debug.Warn(LF_Dialogue.FEATURE, "a conversation node arrived for an NPC this machine does not have");
			return;
		}

		if (!IsOpen() || npc.GetID() != s_NpcId)
		{
			if (IsOpen())
				Close("the server moved the conversation", false);

			LF_DialogueConfig config = LF_DialogueComponent.ConfigOf(npc);
			if (!config || !Open(npc, config))
				return;
		}

		s_sNode = nodeId;
		s_iMask = mask;
		s_bWaiting = false;
		s_bChoosing = false;
		s_aQueue.Clear();
		spoken.Split(LF_DialogueFlow.SEPARATOR, s_aQueue, true);
		s_iShown++;
		LF_Debug.Info(LF_Dialogue.FEATURE, string.Format("node '%1/%2': says [%3], replies mask %4", s_Config.m_sId, nodeId, spoken, mask));
		NextLine();
	}

	//------------------------------------------------------------------------------------------------
	//! Say the next queued entry, or offer the replies, or end.
	protected static void NextLine()
	{
		IEntity npc = GetGame().GetWorld().FindEntityByID(s_NpcId);
		s_aRows.Clear();
		ShowRows();
		if (!s_aQueue.IsEmpty() && npc)
		{
			string entry = s_aQueue[0];
			s_aQueue.RemoveOrdered(0);
			LF_DialogueLine line = LF_DialogueFlow.FindLine(s_Config, entry);
			string caption = entry;
			if (line)
				caption = line.m_sTextKey;

			s_sLastCaption = LF_DialogueOverrides.Text(caption);
			SetText("Caption", s_sLastCaption);
			SetText("Hint", "#LF-Dialogue_HintLine");
			float hold = LF_DialogueVoice.Say(npc, s_Config, entry);
			s_iLineEndMs = System.GetTickCount() + hold * 1000 + LINE_TAIL_MS;
			// A held confirm key must not skip the line it just brought up.
			s_iGuardUntilMs = System.GetTickCount() + INPUT_GUARD_MS;
			s_bChoosing = false;
			return;
		}

		if (s_iMask == 0)
		{
			Close("the conversation ended", false);
			return;
		}

		LF_DialogueNode node = LF_DialogueFlow.FindNode(s_Config, s_sNode);
		if (node && node.m_aOptions)
		{
			for (int i = 0; i < node.m_aOptions.Count(); i++)
			{
				if (LF_DialogueFlow.IsOffered(s_iMask, i))
					s_aRows.Insert(i);
			}
		}

		if (s_aRows.IsEmpty())
		{
			LF_Debug.Error(LF_Dialogue.FEATURE, string.Format("node '%1/%2' offers replies this machine cannot show; leaving", s_Config.m_sId, s_sNode));
			Close("no reply to show", true);
			return;
		}

		s_bChoosing = true;
		s_iFocus = 0;
		s_iGuardUntilMs = System.GetTickCount() + INPUT_GUARD_MS;
		SetText("Hint", "#LF-Dialogue_HintChoose");
		ShowRows();
	}

	//------------------------------------------------------------------------------------------------
	protected static void SetText(string widget, string text)
	{
		if (!s_wRoot)
			return;

		TextWidget target = TextWidget.Cast(s_wRoot.FindAnyWidget(widget));
		if (target)
			target.SetText(text);
	}

	//------------------------------------------------------------------------------------------------
	//! Paint the reply rows: "1. text", the focused one in brass.
	protected static void ShowRows()
	{
		if (!s_wRoot)
			return;

		LF_DialogueNode node = LF_DialogueFlow.FindNode(s_Config, s_sNode);
		for (int row = 0; row < LF_DialogueFlow.MAX_OPTIONS; row++)
		{
			TextWidget target = TextWidget.Cast(s_wRoot.FindAnyWidget("Option" + row));
			if (!target)
				continue;

			bool used = s_bChoosing && s_aRows && row < s_aRows.Count() && node && node.m_aOptions;
			target.SetVisible(used);
			if (!used)
				continue;

			target.SetTextFormat("%1.  %2", (row + 1).ToString(), LF_DialogueOverrides.Text(node.m_aOptions[s_aRows[row]].m_sTextKey));
			if (row == s_iFocus)
				target.SetColor(Color.FromRGBA(240, 205, 130, 255));
			else
				target.SetColor(Color.FromRGBA(209, 204, 189, 255));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Hold the character still for the scene, or let it go.
	protected static void Freeze(bool frozen)
	{
		SCR_ChimeraCharacter character = LF_Utils.GetLocalCharacter();
		if (!character)
			return;

		CharacterControllerComponent controller = character.GetCharacterController();
		if (!controller)
			return;

		controller.SetDisableMovementControls(frozen);
		controller.SetDisableViewControls(frozen);
		controller.SetDisableWeaponControls(frozen);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: pick the reply on a shown row (0-based). The harness calls this too.
	static bool PickRow(int row)
	{
		if (!IsOpen() || !s_bChoosing || !s_aRows || row < 0 || row >= s_aRows.Count())
			return false;

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller)
			return false;

		int index = s_aRows[row];
		s_bChoosing = false;
		s_bWaiting = true;
		s_iWaitSinceMs = System.GetTickCount();
		ShowRows();
		SetText("Hint", "");
		controller.LF_AskDialogueChoose(index);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: skip the line that is playing. The harness calls this too.
	static void SkipLine()
	{
		if (!IsOpen() || s_bChoosing || s_bWaiting)
			return;

		IEntity npc = GetGame().GetWorld().FindEntityByID(s_NpcId);
		LF_DialogueVoice.Stop(npc);
		NextLine();
	}

	//------------------------------------------------------------------------------------------------
	protected static void Tick()
	{
		if (!IsOpen())
			return;

		BaseWorld world = GetGame().GetWorld();
		SCR_ChimeraCharacter character = LF_Utils.GetLocalCharacter();
		if (!world || !world.FindEntityByID(s_NpcId))
		{
			Close("the NPC is gone", true);
			return;
		}

		if (!character || character.GetCharacterController().GetLifeState() != ECharacterLifeState.ALIVE)
		{
			Close("the player is down", true);
			return;
		}

		int now = System.GetTickCount();
		if (s_bWaiting && now - s_iWaitSinceMs > ANSWER_TIMEOUT_MS)
		{
			LF_Debug.Warn(LF_Dialogue.FEATURE, "the server did not answer; leaving the conversation");
			Close("no answer", true);
			return;
		}

		if (!s_bChoosing && !s_bWaiting && now >= s_iLineEndMs)
		{
			NextLine();
			return;
		}

		InputManager input = GetGame().GetInputManager();
		if (!input)
			return;

		// The menu contexts carry MenuSelect, MenuBack, MenuUp, MenuDown and the LF_Keypad digits
		// for keyboard and pad; they only answer while activated every frame.
		input.ActivateContext("MenuContext");
		input.ActivateContext("MenuWithEditorContext");
		if (now < s_iGuardUntilMs)
			return;

		if (input.GetActionTriggered("MenuBack"))
		{
			Close("the player left", true);
			return;
		}

		if (!s_bChoosing)
		{
			if (!s_bWaiting && input.GetActionTriggered("MenuSelect"))
				SkipLine();

			return;
		}

		for (int row = 0; row < s_aRows.Count(); row++)
		{
			if (input.GetActionTriggered("LF_Keypad" + (row + 1)))
			{
				PickRow(row);
				return;
			}
		}

		int step = 0;
		if (input.GetActionTriggered("MenuDown"))
			step = 1;
		else if (input.GetActionTriggered("MenuUp"))
			step = -1;

		if (step != 0)
		{
			s_iFocus = (s_iFocus + step + s_aRows.Count()) % s_aRows.Count();
			ShowRows();
			return;
		}

		if (input.GetActionTriggered("MenuSelect"))
			PickRow(s_iFocus);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: close the scene and give the player back. Safe when it is already closed.
	//! \param tellServer The server does not know yet (the player left, or this machine gave up).
	static void Close(string reason, bool tellServer)
	{
		if (!IsOpen())
			return;

		GetGame().GetCallqueue().Remove(Tick);
		BaseWorld world = GetGame().GetWorld();
		if (world)
		{
			IEntity npc = world.FindEntityByID(s_NpcId);
			LF_DialogueVoice.Stop(npc);
			LF_DialogueVoice.Attend(npc, false);
		}

		LF_DialogueVoice.HoldMouth(null, -1);

		if (s_wRoot)
			s_wRoot.RemoveFromHierarchy();

		Freeze(false);
		LF_DialogueCamera.Stop();
		string id = s_Config.m_sId;
		ResetStatics();
		LF_Debug.Info(LF_Dialogue.FEATURE, string.Format("scene with '%1' closed: %2", id, reason));
		if (!tellServer)
			return;

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (controller)
			controller.LF_AskDialogueLeave();
	}
}
