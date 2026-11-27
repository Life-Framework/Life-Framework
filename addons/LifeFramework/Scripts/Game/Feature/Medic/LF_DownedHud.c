//------------------------------------------------------------------------------------------------
//! The downed player's HUD line (#5155): "Bleeding out 2:40. You can give up in 0:25." and, once the
//! delay has passed, "Bleeding out 2:40. Hold X to give up, or wait for help." Client only; the
//! server decides (LF_BleedOut.GiveUp). The give-up wait counts from the replicated "seconds
//! downed" (stepped every few seconds, SCR_ChimeraCharacter.LF_GetDownedForShown) plus this
//! client's own time since that value arrived, so a player who rejoins into their downed logout
//! body reads the true wait, not a fresh one. The too-early toast names the exact seconds if the
//! player asks one second early. The bleed time is the replicated copy too, so a paused stretcher
//! patient reads right.
//!
//! Shown from three places, all through OnLocal: the downed or stun flag reaching the owner
//! (LF_OnDownedReplicated), the server setting the flag on a listen host's own character (the host
//! gets no replication callback for its own change), and this machine taking control of a body that
//! is already downed (SCR_PlayerController.OnControlledEntityChanged: a rejoin takes the body back
//! with the flag long set).
class LF_DownedHud
{
	static const ResourceName LAYOUT = "{F365D6B9AD021C98}UI/Layouts/HUD/LF_DownedClock.layout";
	static const int TICK_MS = 500; // lf-fixed: UI refresh

	protected static Widget s_wRoot;
	protected static TextWidget s_wLine;
	protected static int s_iBaseSecs = -1;
	protected static int s_iBaseAtMs;
	protected static bool s_bTicking;

	//------------------------------------------------------------------------------------------------
	//! Pure: should the line be on screen for a character in this state?
	static bool ShouldShow(bool downed, bool stunned)
	{
		return downed && !stunned;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: milliseconds downed, from the last replicated whole-second copy and the time since it arrived.
	static int ElapsedMs(int baseSecs, int sinceBaseMs)
	{
		return LF_Utils.MaxInt(baseSecs, 0) * 1000 + LF_Utils.MaxInt(sinceBaseMs, 0);
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: a new replicated copy restarts the local count from it.
	static bool NeedsRebase(int baseSecs, int replicatedSecs)
	{
		return baseSecs != replicatedSecs;
	}

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
		if (!character || !ShouldShow(character.LF_IsDowned(), character.LF_IsStunned()))
		{
			Remove();
			return;
		}

		if (s_wRoot)
			return;

		Show();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the downed or stun flag of `character` changed. A listen host owns its own character
	//! and gets no replication callback for a change it makes, so it drives the line directly.
	static void OnServerChanged(SCR_ChimeraCharacter character)
	{
		if (!character || RplSession.Mode() == RplMode.Dedicated || character != LF_Utils.GetLocalCharacter())
			return;

		OnLocal(character);
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
		s_iBaseSecs = -1;
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
		int replicated = character.LF_GetDownedForShown();
		if (NeedsRebase(s_iBaseSecs, replicated))
		{
			s_iBaseSecs = replicated;
			s_iBaseAtMs = System.GetTickCount();
		}

		int wait = GiveUpSecondsLeft(ElapsedMs(s_iBaseSecs, System.GetTickCount() - s_iBaseAtMs), LF_MedicConfig.Get().m_fGiveUpDelaySeconds);
		string bleed = LF_JailBooking.Clock(character.LF_GetBleedSecondsLeft());
		return WidgetManager.Translate(LineKey(wait), bleed, LF_JailBooking.Clock(wait));
	}

	//------------------------------------------------------------------------------------------------
	protected static void Tick()
	{
		SCR_ChimeraCharacter me = SCR_ChimeraCharacter.Cast(LF_Utils.GetLocalCharacter());
		if (!me || !s_wRoot || !ShouldShow(me.LF_IsDowned(), me.LF_IsStunned()))
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
//! Taking control of a body that is already downed (a rejoin into the logout body, #1975) brings no
//! flag change, so the line is asked for here.
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	override void OnControlledEntityChanged(IEntity from, IEntity to)
	{
		super.OnControlledEntityChanged(from, to);

		if (RplSession.Mode() == RplMode.Dedicated || GetGame().GetPlayerController() != this)
			return;

		LF_DownedHud.OnLocal(SCR_ChimeraCharacter.Cast(to));
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
