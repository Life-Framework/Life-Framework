//------------------------------------------------------------------------------------------------
//! LF_DebugMenu — dev tool to trigger gameplay states in the DebugWorld.
//!
//! Registered as the DebugMenu preset in Configs/System/chimeraMenus.conf. The enum constant
//! lives in the shared modded enum (Feature/Quantity/ChimeraMenuPreset.c); that constant, this
//! preset's name, and the Class it names must match exactly or the menu silently never opens.
//!
//! Every action routes client->server through the requesting player's owned character and is
//! gated there by LF_DevGate: allowed only when the game mode opts in (m_bDevToolsEnabled, set on
//! GameMode_Roleplay_Debug.et) or the sender is an administrator. The client-side check on the
//! open key is cosmetic. Never trust the client.
//------------------------------------------------------------------------------------------------
class LF_DebugMenu : ChimeraMenuBase
{
	//! Per-action press timestamps for the dual-route debounce in ActionReady().
	protected ref map<string, float> m_mActionStampMs;

	//! Two routes report the SAME physical press: a hint button's m_OnActivated invoker
	//! (mouse) and the per-frame poll in OnMenuUpdate (keyboard / gamepad). Both are needed —
	//! drop the poll and Enter/pad-A stop working, drop the buttons and clicks stop.
	//! 150 ms is comfortably longer than one frame at any playable rate.
	protected static const float ACTION_DEBOUNCE_MS = 150;

	protected ButtonWidget m_wGiveCashButton;
	protected ButtonWidget m_wTakeCashButton;
	protected ButtonWidget m_wWanted0Button;
	protected ButtonWidget m_wWanted5Button;
	protected ButtonWidget m_wFactionPoliceButton;
	protected ButtonWidget m_wFactionCivilianButton;
	protected ButtonWidget m_wSurvivalFullButton;
	protected ButtonWidget m_wSurvivalEmptyButton;
	protected ButtonWidget m_wGrantXpButton;
	protected ButtonWidget m_wGrantSpButton;
	protected ButtonWidget m_wJobFarmerButton;
	protected ButtonWidget m_wJobUnemployedButton;

	//------------------------------------------------------------------------------------------------
	override void OnMenuOpen()
	{
		super.OnMenuOpen();

		Widget root = GetRootWidget();
		if (!root)
			return;

		m_wGiveCashButton = ButtonWidget.Cast(root.FindAnyWidget("GiveCashButton"));
		m_wTakeCashButton = ButtonWidget.Cast(root.FindAnyWidget("TakeCashButton"));
		m_wWanted0Button = ButtonWidget.Cast(root.FindAnyWidget("Wanted0Button"));
		m_wWanted5Button = ButtonWidget.Cast(root.FindAnyWidget("Wanted5Button"));
		m_wFactionPoliceButton = ButtonWidget.Cast(root.FindAnyWidget("FactionPoliceButton"));
		m_wFactionCivilianButton = ButtonWidget.Cast(root.FindAnyWidget("FactionCivilianButton"));
		m_wSurvivalFullButton = ButtonWidget.Cast(root.FindAnyWidget("SurvivalFullButton"));
		m_wSurvivalEmptyButton = ButtonWidget.Cast(root.FindAnyWidget("SurvivalEmptyButton"));
		m_wGrantXpButton = ButtonWidget.Cast(root.FindAnyWidget("GrantXpButton"));
		m_wGrantSpButton = ButtonWidget.Cast(root.FindAnyWidget("GrantSpButton"));
		m_wJobFarmerButton = ButtonWidget.Cast(root.FindAnyWidget("JobFarmerButton"));
		m_wJobUnemployedButton = ButtonWidget.Cast(root.FindAnyWidget("JobUnemployedButton"));

		SCR_InputButtonComponent hintBack = SCR_InputButtonComponent.GetInputButtonComponent("HintBack", root);
		if (hintBack)
			hintBack.m_OnActivated.Insert(OnClose);
	}

	//------------------------------------------------------------------------------------------------
	override bool OnClick(Widget w, int x, int y, int button)
	{
		if (w == m_wGiveCashButton) { AskAction(LF_EDebugAction.GIVE_CASH, 1000); return true; }
		if (w == m_wTakeCashButton) { AskAction(LF_EDebugAction.TAKE_CASH, 1000); return true; }
		if (w == m_wWanted0Button) { AskAction(LF_EDebugAction.SET_WANTED, 0); return true; }
		if (w == m_wWanted5Button) { AskAction(LF_EDebugAction.SET_WANTED, 5); return true; }
		if (w == m_wFactionPoliceButton) { AskAction(LF_EDebugAction.SET_FACTION, LF_Faction.POLICE); return true; }
		if (w == m_wFactionCivilianButton) { AskAction(LF_EDebugAction.SET_FACTION, LF_Faction.CIVILIAN); return true; }
		if (w == m_wSurvivalFullButton) { AskAction(LF_EDebugAction.SET_SURVIVAL, 100); return true; }
		if (w == m_wSurvivalEmptyButton) { AskAction(LF_EDebugAction.SET_SURVIVAL, 0); return true; }
		if (w == m_wGrantXpButton) { AskAction(LF_EDebugAction.ADD_XP, 500); return true; }
		if (w == m_wGrantSpButton) { AskAction(LF_EDebugAction.ADD_SP, 5); return true; }
		if (w == m_wJobFarmerButton) { AskAction(LF_EDebugAction.SET_JOB, LF_EJobType.FARMER); return true; }
		if (w == m_wJobUnemployedButton) { AskAction(LF_EDebugAction.SET_JOB, LF_EJobType.UNEMPLOYED); return true; }

		return super.OnClick(w, x, y, button);
	}

	//------------------------------------------------------------------------------------------------
	//! Routes a debug action to the server through the requesting player's owned character.
	protected void AskAction(LF_EDebugAction action, int value)
	{
		SCR_ChimeraCharacter character = LF_Utils.GetLocalCharacter();
		if (!character)
		{
			LF_Debug.Error("DebugMenu", string.Format("no local character to apply action %1", typename.EnumToString(LF_EDebugAction, action)));
			return;
		}
		character.LF_AskDebugAction(action, value);
	}

	//------------------------------------------------------------------------------------------------
	//! Input contexts must be re-activated EVERY FRAME. Without this only raw mouse clicks work —
	//! Enter, ESC, tab keys and the whole gamepad appear dead, which reads as a keybinding problem
	//! and is not one.
	override void OnMenuUpdate(float tDelta)
	{
		super.OnMenuUpdate(tDelta);

		InputManager input = GetGame().GetInputManager();
		if (!input)
			return;

		input.ActivateContext("MenuContext");

		// MenuUp / MenuDown are NOT in MenuContext — they live in MenuWithEditorContext and
		// friends, so polling them under MenuContext alone can never fire. This context is a pure
		// menu-action superset, so co-activating it is safe. Remove only if this menu polls no
		// directional actions at all.
		input.ActivateContext("MenuWithEditorContext");

		// Action LISTENERS do not fire reliably in this menu family, so actions are POLLED
		// here instead. The debounce lives inside each handler, not in these conditions — see
		// the note on ActionReady().
		if (input.GetActionTriggered("MenuBack"))
		{
			OnClose();
			return;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Call this at the TOP OF EVERY HANDLER, never in the poll condition.
	//!
	//! Both routes to a handler must pass through the debounce. Guarding only the poll leaves
	//! the hint button's m_OnActivated invoker free to fire the handler directly, so a mouse
	//! click runs it undebounced and the poll then runs it again in the same frame.
	//!
	//! Time-based, NOT a per-frame flag: the invoker and the poll dispatch at different points
	//! in the frame, so a flag cleared between them lets the double-fire back in. Symptom when
	//! this is missing or flag-based: one press advances two steps, or Close() runs twice and
	//! takes the menu underneath with it.
	protected bool ActionReady(string key)
	{
		if (!m_mActionStampMs)
			m_mActionStampMs = new map<string, float>();

		// A menu opened outside a mission (e.g. from the main menu) has no world. Failing open
		// is correct here: a missing debounce is a double-press, a null deref is a crash.
		ChimeraWorld world = GetGame().GetWorld();
		if (!world)
			return true;

		float nowMs = world.GetWorldTime();

		float lastMs;
		if (m_mActionStampMs.Find(key, lastMs) && nowMs - lastMs < ACTION_DEBOUNCE_MS)
			return false;

		m_mActionStampMs.Set(key, nowMs);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void OnClose()
	{
		if (!ActionReady("MenuBack"))
			return;

		Close();
	}
}

//------------------------------------------------------------------------------------------------
//! Client->server bridge for debug menu actions. Hosted on the owned character: a client can
//! only RPC on entities it owns, and the character is the one entity the player owns. The server
//! checks LF_DevGate (game-mode opt-in or administrator) and the action id before applying;
//! replies are not needed — each action logs through LF_Debug.
modded class SCR_ChimeraCharacter
{
	//------------------------------------------------------------------------------------------------
	//! Ask the server to apply a debug action to this player.
	//! \param action Validated on the server; unknown values are refused.
	//! \param value Action payload (amount, wanted level, faction id, ...).
	void LF_AskDebugAction(LF_EDebugAction action, int value)
	{
		Rpc(RpcAsk_LF_DebugAction, action, value);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_DebugAction(LF_EDebugAction action, int value)
	{
		if (!LF_DevGate.CheckCharacterRequest(this, "DebugMenu", "debug action"))
			return;

		if (!LF_DebugActionUtils.IsValidAction(action))
		{
			LF_Debug.Error("DebugMenu", string.Format("debug action refused: %1 is not a known action", typename.EnumToString(LF_EDebugAction, action)));
			return;
		}

		LF_DebugActionUtils.Apply(this, action, value);
	}
}

//------------------------------------------------------------------------------------------------
enum LF_EDebugAction
{
	GIVE_CASH = 0,
	TAKE_CASH,
	SET_WANTED,
	SET_FACTION,
	SET_SURVIVAL,
	ADD_XP,
	ADD_SP,
	SET_JOB,
	COUNT
}

//------------------------------------------------------------------------------------------------
//! Pure helpers for the debug menu: action validation and server-side application.
//! The validation halves are LOGIC-tier tested (no world needed); Apply touches entities.
class LF_DebugActionUtils
{
	//------------------------------------------------------------------------------------------------
	//! \param actionId Action id to check (enums compare equal to their underlying int).
	//! \return true when the id is within the known enum range.
	static bool IsValidAction(int actionId)
	{
		return actionId >= 0 && actionId < LF_EDebugAction.COUNT;
	}

	//------------------------------------------------------------------------------------------------
	//! Server-side application. The requesting player's own character and account are mutated;
	//! nothing here trusts the client beyond the enum/range validation done by the caller.
	//! \param character The requesting player's owned character (RPC sender).
	//! \param action Validated action to apply.
	//! \param value Action payload.
	static void Apply(IEntity character, LF_EDebugAction action, int value)
	{
		if (!character)
		{
			LF_Debug.Error("DebugMenu", "debug action failed: no character");
			return;
		}

		string playerUid = LF_Utils.GetPlayerUID(character);
		if (playerUid.IsEmpty())
		{
			LF_Debug.Error("DebugMenu", "debug action failed: no player uid");
			return;
		}

		LF_PlayerAccountManager accountManager = LF_PlayerAccountManager.GetInstance();
		LF_PlayerAccount account = null;
		if (accountManager)
			account = accountManager.GetAccount(playerUid);

		switch (action)
		{
			case LF_EDebugAction.GIVE_CASH:
				ApplyCash(character, Math.AbsInt(value), false);
				break;

			case LF_EDebugAction.TAKE_CASH:
				ApplyCash(character, Math.AbsInt(value), true);
				break;

			case LF_EDebugAction.SET_WANTED:
				SetWanted(account, value);
				break;

			case LF_EDebugAction.SET_FACTION:
				SetFaction(account, value);
				break;

			case LF_EDebugAction.SET_SURVIVAL:
				ApplySurvival(character, value);
				break;

			case LF_EDebugAction.ADD_XP:
				ApplyXp(character, Math.AbsInt(value));
				break;

			case LF_EDebugAction.ADD_SP:
				ApplySkillPoints(character, Math.AbsInt(value));
				break;

			case LF_EDebugAction.SET_JOB:
				SetJob(character, value);
				break;
		}

		if (accountManager && account)
			accountManager.SaveAndReleaseAccount(account);
	}

	//------------------------------------------------------------------------------------------------
	//! Adds or removes the player's cash. Amounts are clamped to sane bounds server-side.
	static void ApplyCash(IEntity character, int amount, bool remove)
	{
		int cash = LF_MoneyUtils.GetCash(character);
		if (cash < 0)
		{
			LF_Debug.Error("DebugMenu", "cash action failed: no resolvable inventory");
			return;
		}

		if (remove)
			LF_MoneyUtils.RemoveCash(character, amount, "debug");
		else
			LF_MoneyUtils.AddCash(character, amount, "debug");

		string verb;
		if (remove)
			verb = "removed";
		else
			verb = "given";

		LF_Debug.Info("DebugMenu", string.Format("cash %1 %2 (was %3)", verb, amount, cash));
	}

	//------------------------------------------------------------------------------------------------
	//! Sets the player's wanted level, clamped to the engine's 0..5 range.
	static void SetWanted(LF_PlayerAccount account, int value)
	{
		if (!account)
		{
			LF_Debug.Error("DebugMenu", "wanted action failed: no account");
			return;
		}
		account.SetWantedLevel(value);
		LF_Debug.Info("DebugMenu", string.Format("wanted level set to %1", account.GetWantedLevel()));
	}

	//------------------------------------------------------------------------------------------------
	//! Sets the player's faction (and for POLICE, puts them on duty).
	static void SetFaction(LF_PlayerAccount account, int value)
	{
		if (!account)
		{
			LF_Debug.Error("DebugMenu", "faction action failed: no account");
			return;
		}

		LF_Faction faction;
		if (value == LF_Faction.POLICE)
			faction = LF_Faction.POLICE;
		else
			faction = LF_Faction.CIVILIAN;
		account.SetFaction(faction);
		account.SetOnDuty(faction == LF_Faction.POLICE);
		LF_Debug.Info("DebugMenu", string.Format("faction set to %1, duty %2", typename.EnumToString(LF_Faction, faction), account.IsOnDuty()));
	}

	//------------------------------------------------------------------------------------------------
	//! Sets hunger and thirst to the given level (0..100). Health is the character's real health.
	static void ApplySurvival(IEntity character, int value)
	{
		LF_CharacterSurvivalComponent survival = LF_Component<LF_CharacterSurvivalComponent>.Find(character);
		if (!survival)
		{
			LF_Debug.Error("DebugMenu", "survival action failed: no survival component");
			return;
		}

		LF_SurvivalStats stats = survival.GetSurvivalStats();
		if (!stats)
		{
			LF_Debug.Error("DebugMenu", "survival action failed: no survival stats");
			return;
		}

		int clamped = Math.Clamp(value, 0, 100);
		stats.SetHunger(clamped);
		stats.SetThirst(clamped);
		survival.SyncReplication();
		LF_Debug.Info("DebugMenu", string.Format("survival set to %1", clamped));
	}

	//------------------------------------------------------------------------------------------------
	static void ApplyXp(IEntity character, int amount)
	{
		LF_PlayerLevelComponent level = LF_Component<LF_PlayerLevelComponent>.Find(character);
		if (!level)
		{
			LF_Debug.Error("DebugMenu", "xp action failed: no level component");
			return;
		}
		level.AddExperience(amount, "DebugMenu");
	}

	//------------------------------------------------------------------------------------------------
	static void ApplySkillPoints(IEntity character, int amount)
	{
		LF_PlayerLevelComponent level = LF_Component<LF_PlayerLevelComponent>.Find(character);
		if (!level)
		{
			LF_Debug.Error("DebugMenu", "sp action failed: no level component");
			return;
		}
		level.AddSkillPoints(amount, "DebugMenu");
	}

	//------------------------------------------------------------------------------------------------
	static void SetJob(IEntity character, int value)
	{
		LF_PlayerJobComponent job = LF_Component<LF_PlayerJobComponent>.Find(character);
		if (!job)
		{
			LF_Debug.Error("DebugMenu", "job action failed: no job component");
			return;
		}
		if (value == LF_EJobType.FARMER)
			job.SetJob(LF_EJobType.FARMER);
		else
			job.SetJob(LF_EJobType.UNEMPLOYED);
	}
}
