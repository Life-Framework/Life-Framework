//------------------------------------------------------------------------------------------------
//! LF_SiteCaptureHud.c: the capture bar of a contested site (#2686, gang war #1022). A player
//! standing inside a site's radius sees a small panel under the lawless tag: the site's name, who
//! holds it, and how far the challenger's capture has got. The server decides and sends; the client
//! only draws.
//! - LF_SiteCaptureFeed (server): after each site's sweep, finds the players inside the radius and
//!   sends each an owner RPC when what they would see changed (a player who walks out gets one
//!   hide). A sweep sends a player at most one message; an unchanged bar sends nothing.
//! - LF_SiteCaptureHud (client): draws it from LF_SiteCaptureBar.layout in the theme's colours.
//! A host's own player gets the same owner RPC: on a server an Owner Rpc to an entity it owns runs
//! inside the call (docs/architecture/rpc-routing.md), so there is no host branch.

//------------------------------------------------------------------------------------------------
class LF_SiteCaptureFeed
{
	protected static ref map<int, string> s_mSite;
	protected static ref map<int, string> s_mKey;
	protected static int s_iShows;
	protected static int s_iHides;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mSite = null;
		s_mKey = null;
		s_iShows = 0;
		s_iHides = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Capture progress as a whole percent of the capture time, 0 to 100.
	static int Percent(float progress, float captureSeconds)
	{
		if (captureSeconds <= 0 || progress <= 0)
			return 0;

		return Math.ClampInt(Math.Floor(progress * 100 / captureSeconds), 0, 100);
	}

	//------------------------------------------------------------------------------------------------
	//! What a player at the site would see, as one comparable string.
	static string KeyOf(string hideoutId, string holderName, string capturerName, int percent)
	{
		return string.Format("%1|%2|%3|%4", hideoutId, holderName, capturerName, percent);
	}

	//------------------------------------------------------------------------------------------------
	//! After a site's sweep (server): bring each player's bar in line with this site's state.
	//! `state` is null before the site's first sweep.
	static void Push(string hideoutId, vector centre, float radius, LF_SiteHoldState state, float captureSeconds)
	{
		if (!Replication.IsServer() || !state || !GetGame() || !GetGame().GetPlayerManager())
			return;

		if (!s_mSite)
			s_mSite = new map<int, string>();
		if (!s_mKey)
			s_mKey = new map<int, string>();

		string holderName = LF_SiteFlagComponent.HolderNameOf(state.m_sHolderOrgId);
		string capturerName;
		if (state.m_fCaptureProgress > 0)
			capturerName = LF_SiteFlagComponent.HolderNameOf(state.m_sCapturerOrgId);

		int percent = 0;
		if (!capturerName.IsEmpty())
			percent = Percent(state.m_fCaptureProgress, captureSeconds);

		string key = KeyOf(hideoutId, holderName, capturerName, percent);
		PlayerManager players = GetGame().GetPlayerManager();
		array<int> playerIds = {};
		players.GetPlayers(playerIds);
		Prune(playerIds);
		foreach (int playerId : playerIds)
		{
			bool inside;
			IEntity body = players.GetPlayerControlledEntity(playerId);
			if (body && vector.Distance(body.GetOrigin(), centre) <= radius)
				inside = true;

			SCR_PlayerController controller = SCR_PlayerController.Cast(players.GetPlayerController(playerId));
			if (!controller)
				continue;

			string shownAt = s_mSite.Get(playerId);
			if (inside)
			{
				if (shownAt == hideoutId && s_mKey.Get(playerId) == key)
					continue;

				if (shownAt != hideoutId)
					LF_Debug.Info("Sites", string.Format("capture bar of %1 shown to player %2", hideoutId, playerId));

				s_mSite.Set(playerId, hideoutId);
				s_mKey.Set(playerId, key);
				s_iShows++;
				controller.LF_SiteCaptureShow(hideoutId, holderName, capturerName, percent);
			}
			else if (shownAt == hideoutId)
			{
				s_mSite.Remove(playerId);
				s_mKey.Remove(playerId);
				s_iHides++;
				LF_Debug.Info("Sites", string.Format("capture bar of %1 hidden for player %2", hideoutId, playerId));
				controller.LF_SiteCaptureHide();
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Forget players who left, so a reused player id starts with a fresh bar.
	protected static void Prune(notnull array<int> online)
	{
		array<int> gone = {};
		foreach (int playerId, string site : s_mSite)
		{
			if (online.Find(playerId) < 0)
				gone.Insert(playerId);
		}

		foreach (int goneId : gone)
		{
			s_mSite.Remove(goneId);
			s_mKey.Remove(goneId);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Show messages sent since the last reset (tests and debug read it).
	static int Shows()
	{
		return s_iShows;
	}

}

//------------------------------------------------------------------------------------------------
class LF_SiteCaptureHud
{
	static const ResourceName LAYOUT = "{876C243B745EF845}UI/Layouts/HUD/LF_SiteCaptureBar.layout";
	static const float BAR_WIDTH = 328;	// lf-fixed: the track's width in LF_SiteCaptureBar.layout
	static const float BAR_HEIGHT = 8;	// lf-fixed: the track's height in LF_SiteCaptureBar.layout

	protected static Widget s_wRoot;
	protected static bool s_bShown;
	protected static string s_sDescribe = "hidden";

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		if (s_wRoot)
			s_wRoot.RemoveFromHierarchy();

		s_wRoot = null;
		s_bShown = false;
		s_sDescribe = "hidden";
	}

	//------------------------------------------------------------------------------------------------
	//! The name a site shows: the pack's `LF-SiteName_<hideout id>` row, else a neutral line.
	static string SiteLabel(string hideoutId)
	{
		// Built from two parts: the key is per hideout id and only a pack declares it (never a core row).
		string key = "#LF" + "-SiteName_" + hideoutId;
		string text = WidgetManager.Translate(key);
		if (text.IsEmpty() || text == key || text == key.Substring(1, key.Length() - 1))
			return WidgetManager.Translate("#LF-SiteCapture_Fallback");

		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! The server sent the bar's content: draw it. Called from the player controller RPC.
	static void Show(string hideoutId, string holderName, string capturerName, int percent)
	{
		if (System.IsConsoleApp())
			return;

		if (!Ensure())
			return;

		string holderLine = WidgetManager.Translate("#LF-SiteCapture_Unclaimed");
		if (!holderName.IsEmpty())
			holderLine = WidgetManager.Translate("#LF-SiteCapture_HeldBy", holderName);

		string progressLine = WidgetManager.Translate("#LF-SiteCapture_Idle");
		if (!capturerName.IsEmpty())
			progressLine = WidgetManager.Translate("#LF-SiteCapture_Capturing", capturerName, percent.ToString());

		string label = SiteLabel(hideoutId);
		SetText("Title", label, LF_UITheme.Paper());
		SetText("Holder", holderLine, LF_UITheme.Paper());
		SetText("Progress", progressLine, LF_UITheme.Paper());
		SetBar(percent / 100.0);
		Widget panel = s_wRoot.FindAnyWidget("Panel");
		if (panel)
			panel.SetVisible(true);

		s_bShown = true;
		s_sDescribe = string.Format("shown site=%1 holder=%2 capturer=%3 percent=%4", hideoutId, holderName, capturerName, percent);
	}

	//------------------------------------------------------------------------------------------------
	//! The player left the zone (or the server stopped showing it): take the bar down.
	static void Hide()
	{
		s_bShown = false;
		s_sDescribe = "hidden";
		if (!s_wRoot)
			return;

		Widget panel = s_wRoot.FindAnyWidget("Panel");
		if (panel)
			panel.SetVisible(false);
	}

	//------------------------------------------------------------------------------------------------
	protected static bool Ensure()
	{
		if (s_wRoot)
			return true;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (workspace)
			s_wRoot = workspace.CreateWidgets(LAYOUT);

		if (!s_wRoot)
		{
			LF_Debug.Error("Sites", "the site capture bar did not load; the contest runs without it");
			return false;
		}

		Widget back = s_wRoot.FindAnyWidget("Back");
		if (back)
		{
			back.SetColor(LF_UITheme.Metal());
			back.SetOpacity(0.9);
		}

		Widget edge = s_wRoot.FindAnyWidget("Edge");
		if (edge)
			edge.SetColor(LF_UITheme.Rule());

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static void SetText(string name, string text, Color color)
	{
		TextWidget widget = TextWidget.Cast(s_wRoot.FindAnyWidget(name));
		if (!widget)
			return;

		widget.SetText(text);
		widget.SetColor(color);
	}

	//------------------------------------------------------------------------------------------------
	protected static void SetBar(float share)
	{
		Widget fill = s_wRoot.FindAnyWidget("Fill");
		if (!fill)
			return;

		FrameSlot.SetSize(fill, BAR_WIDTH * Math.Clamp(share, 0, 1), BAR_HEIGHT);
		fill.SetColor(LF_UITheme.Rule());
	}

	//------------------------------------------------------------------------------------------------
	//! For the harness and tests: what the bar shows now.
	static string Describe()
	{
		return s_sDescribe;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsShown()
	{
		return s_bShown;
	}
}

//------------------------------------------------------------------------------------------------
//! The wire of the capture bar: the server tells a player's own client what to draw. Nothing goes
//! the other way.
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	//! Server: show or update the capture bar on this player's screen.
	void LF_SiteCaptureShow(string hideoutId, string holderName, string capturerName, int percent)
	{
		Rpc(RpcDo_LF_SiteCaptureShow, hideoutId, holderName, capturerName, percent);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_SiteCaptureShow(string hideoutId, string holderName, string capturerName, int percent)
	{
		LF_SiteCaptureHud.Show(hideoutId, holderName, capturerName, percent);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: take the capture bar down.
	void LF_SiteCaptureHide()
	{
		Rpc(RpcDo_LF_SiteCaptureHide);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_SiteCaptureHide()
	{
		LF_SiteCaptureHud.Hide();
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of the capture bar statics (LF_StaticReset finds both by reflection).
class LF_SiteCaptureFeed_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_SiteCaptureFeed";
	}

	override void Apply()
	{
		LF_SiteCaptureFeed.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
class LF_SiteCaptureHud_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_SiteCaptureHud";
	}

	override void Apply()
	{
		LF_SiteCaptureHud.ResetStatics();
	}
}
