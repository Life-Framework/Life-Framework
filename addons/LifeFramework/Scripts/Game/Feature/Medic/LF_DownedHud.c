//------------------------------------------------------------------------------------------------
//! The downed player's HUD line (#5155): "Bleeding out 2:40. You can give up in 0:25." and, once the
//! delay has passed, "Bleeding out 2:40. Hold X to give up, or wait for help." Client only; the
//! server decides (LF_BleedOut.GiveUp). The give-up wait counts from the moment this client saw
//! the downed flag, so it can trail the server's own clock by the network delay; the server's
//! too-early toast names the exact seconds if the player asks one second early. The bleed time is
//! the replicated copy (stepped every few seconds), so a paused stretcher patient reads right.
class LF_DownedHud
{
	static const ResourceName LAYOUT = "{F365D6B9AD021C98}UI/Layouts/HUD/LF_DownedClock.layout";
	static const int TICK_MS = 500; // lf-fixed: UI refresh

	protected static Widget s_wRoot;
	protected static TextWidget s_wLine;
	protected static int s_iDownedAtMs;
	protected static bool s_bTicking;

	//------------------------------------------------------------------------------------------------
	//! Pure: whole seconds of give-up wait left after `elapsedMs` down; 0 once it is allowed.
	static int GiveUpSecondsLeft(int elapsedMs, float delaySeconds)
	{
		float left = delaySeconds - elapsedMs / 1000.0;
		if (left <= 0)
			return 0;

		return Math.Ceil(left);
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the localization key and whether it takes the wait seconds.
	static string LineKey(int giveUpSecondsLeft)
	{
		if (giveUpSecondsLeft > 0)
			return "#LF-DownedHud_Wait";

		return "#LF-DownedHud_Ready";
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		Remove();
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the local character's downed or stun flag changed.
	static void OnLocal(SCR_ChimeraCharacter character)
	{
		if (!character || !character.LF_IsDowned() || character.LF_IsStunned())
		{
			Remove();
			return;
		}

		if (s_wRoot)
			return;

		Show();
	}

	//------------------------------------------------------------------------------------------------
	protected static void Show()
	{
		if (!GetGame() || RplSession.Mode() == RplMode.Dedicated)
			return;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		s_wRoot = workspace.CreateWidgets(LAYOUT);
		if (!s_wRoot)
		{
			LF_Debug.Error(LF_Downed.FEATURE, "the downed HUD line did not load: " + LAYOUT);
			return;
		}

		s_wLine = TextWidget.Cast(s_wRoot.FindAnyWidget("Line"));
		s_iDownedAtMs = System.GetTickCount();
		if (!s_bTicking && GetGame().GetCallqueue())
		{
			GetGame().GetCallqueue().CallLater(Tick, TICK_MS, true);
			s_bTicking = true;
		}

		LF_Debug.Info(LF_Downed.FEATURE, "downed HUD line shown");
		Tick();
	}

	//------------------------------------------------------------------------------------------------
	static void Remove()
	{
		if (s_bTicking && GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(Tick);

		s_bTicking = false;
		if (s_wRoot)
		{
			s_wRoot.RemoveFromHierarchy();
			LF_Debug.Info(LF_Downed.FEATURE, "downed HUD line removed");
		}

		s_wRoot = null;
		s_wLine = null;
	}

	//------------------------------------------------------------------------------------------------
	protected static string TextNow(SCR_ChimeraCharacter character)
	{
		int wait = GiveUpSecondsLeft(System.GetTickCount() - s_iDownedAtMs, LF_MedicConfig.Get().m_fGiveUpDelaySeconds);
		string bleed = LF_JailBooking.Clock(character.LF_GetBleedSecondsLeft());
		return WidgetManager.Translate(LineKey(wait), bleed, LF_JailBooking.Clock(wait));
	}

	//------------------------------------------------------------------------------------------------
	protected static void Tick()
	{
		SCR_ChimeraCharacter me = SCR_ChimeraCharacter.Cast(LF_Utils.GetLocalCharacter());
		if (!me || !s_wRoot || !me.LF_IsDowned() || me.LF_IsStunned())
		{
			Remove();
			return;
		}

		if (s_wLine)
			s_wLine.SetText(TextNow(me));
	}

	//------------------------------------------------------------------------------------------------
	//! The harness's `seen`: the line on screen, or "none".
	static string Describe()
	{
		if (!s_wRoot || !s_wLine)
			return "none";

		return s_wLine.GetText();
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_DownedHud's statics (LF_StaticReset finds it by reflection).
class LF_DownedHud_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_DownedHud";
	}

	override void Apply()
	{
		LF_DownedHud.ResetStatics();
	}
}
