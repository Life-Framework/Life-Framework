//------------------------------------------------------------------------------------------------
//! How much of a contract's time is left, in words (#2914). Never a number on screen.
enum LF_EContractTimeWord
{
	NONE,		//!< the contract has no time limit
	PLENTY,		//!< more than half the time is left
	SOON,		//!< between a fifth and a half is left
	HURRY,		//!< under a fifth is left
	EXPIRED		//!< no time is left: the line goes
}

//------------------------------------------------------------------------------------------------
//! A running contract in the player's tracker (#2914, docs/architecture/reputation.md "Spending
//! standing: contracts"): one line saying who it is for, what to do and how much time is left in
//! words, and under it what walking away costs, in words, never points.
//!
//! WIRE. LF_ReputationContracts calls Show when a contract is taken and Hide when it ends (finished,
//! abandoned, timed out, dropped). The server sends "contractId|limitSeconds|stakeFlag" to that
//! player's own controller with an Owner RPC (RpcDo_LF_ContractLine); an empty payload clears the
//! line. Nobody else gets it. The client counts the time down itself from the moment it received the
//! line (no per-second traffic), moves the word through its bands, and drops the line when the time
//! is gone. The server still settles the contract as before.
//!
//! WORDS. The title is the key "Title_" plus the contract id under the tracker's key family (the pack's row with the
//! same Id carries the client's name, "Crates for the harbour master"); a contract with no such row
//! reads `#LF-ContractTracker_Title_Default`. Core wording is neutral.
//------------------------------------------------------------------------------------------------
class LF_ContractTracker
{
	static const string FEATURE = "Reputation";
	static const string KEY_TITLE_DEFAULT = "#LF-ContractTracker_Title_Default";
	static const string KEY_COST = "#LF-ContractTracker_Cost";
	static const string KEY_LINE = "#LF-ContractTracker_Line";
	static const string KEY_TIME_PLENTY = "#LF-ContractTracker_Time_Plenty";
	static const string KEY_TIME_SOON = "#LF-ContractTracker_Time_Soon";
	static const string KEY_TIME_HURRY = "#LF-ContractTracker_Time_Hurry";

	static const float SOON_FRACTION = 0.5;		// lf-fixed: word band edge, not tuning
	static const float HURRY_FRACTION = 0.2;	// lf-fixed: word band edge, not tuning
	//! The journal keeps this many recent sends (tests, harness).
	static const int JOURNAL_MAX = 32;	// lf-fixed: a debug ring size

	//! Server: the last sends, newest last, as "uid|payload" (payload empty for a clear). Recorded
	//! whether or not the player is online. Session-only.
	protected static ref array<string> s_aJournal;

	//! Client: the contract line this client shows. Empty id is no line.
	protected static string s_sClientId;
	protected static int s_iClientLimit;
	protected static bool s_bClientCost;
	protected static int s_iClientReceivedMs;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_aJournal = null;
		ClearClient();
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the band of the time left. `limit` 0 is no limit.
	static LF_EContractTimeWord TimeWord(float remaining, float limit)
	{
		if (limit <= 0)
			return LF_EContractTimeWord.NONE;

		if (remaining <= 0)
			return LF_EContractTimeWord.EXPIRED;

		float fraction = remaining / limit;
		if (fraction > SOON_FRACTION)
			return LF_EContractTimeWord.PLENTY;

		if (fraction > HURRY_FRACTION)
			return LF_EContractTimeWord.SOON;

		return LF_EContractTimeWord.HURRY;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the `#` key of a band, empty for none and expired.
	static string TimeKey(LF_EContractTimeWord word)
	{
		if (word == LF_EContractTimeWord.PLENTY)
			return KEY_TIME_PLENTY;

		if (word == LF_EContractTimeWord.SOON)
			return KEY_TIME_SOON;

		if (word == LF_EContractTimeWord.HURRY)
			return KEY_TIME_HURRY;

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: "contractId|limitSeconds|stakeFlag".
	static string Encode(string contractId, int limitSeconds, bool hasStake)
	{
		int stake = 0;
		if (hasStake)
			stake = 1;

		return string.Format("%1|%2|%3", contractId, limitSeconds, stake);
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: reads Encode's payload.
	//! \return False for an empty or malformed payload.
	static bool Decode(string payload, out string contractId, out int limitSeconds, out bool hasStake)
	{
		contractId = string.Empty;
		limitSeconds = 0;
		hasStake = false;
		TStringArray parts = {};
		payload.Split("|", parts, false);
		if (parts.Count() != 3 || parts[0].IsEmpty())
			return false;

		contractId = parts[0];
		limitSeconds = parts[1].ToInt();
		hasStake = parts[2].ToInt() > 0;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the title key for a contract id.
	static string TitleKey(string contractId)
	{
		// Joined in pieces so the key check does not read the open-ended prefix as one key.
		return "#" + "LF-ContractTracker_" + "Title_" + contractId;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a contract was taken. Sends the line to the character's own client.
	static void Show(string playerUid, string contractId)
	{
		if (Replication.IsRunning() && !Replication.IsServer())
			return;

		LF_ReputationContractDef def = LF_ReputationContracts.FindDef(contractId);
		if (!def)
		{
			LF_Debug.Warn(FEATURE, string.Format("contract line for %1 not sent: '%2' is not in the config", playerUid, contractId));
			return;
		}

		Send(playerUid, Encode(contractId, Math.Round(def.m_fTimeLimitSeconds), def.m_iStakePoints > 0), "taken");
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the contract ended in any way. Clears the line on the character's own client.
	static void Hide(string playerUid)
	{
		if (Replication.IsRunning() && !Replication.IsServer())
			return;

		Send(playerUid, string.Empty, "ended");
	}

	//------------------------------------------------------------------------------------------------
	protected static void Send(string playerUid, string payload, string reason)
	{
		if (playerUid.IsEmpty())
		{
			LF_Debug.Warn(FEATURE, string.Format("contract line not sent: empty player id (%1)", reason));
			return;
		}

		if (!s_aJournal)
			s_aJournal = {};

		s_aJournal.Insert(playerUid + "|" + payload);
		while (s_aJournal.Count() > JOURNAL_MAX)
		{
			s_aJournal.RemoveOrdered(0);
		}

		int playerId = LF_ReputationSync.FindPlayerId(playerUid);
		if (playerId < 0)
			return;

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
		if (!controller)
		{
			LF_Debug.Warn(FEATURE, string.Format("contract line for %1 not sent: player %2 has no controller (%3)", playerUid, playerId, reason));
			return;
		}

		controller.LF_SendContractLine(payload);
		LF_Debug.Info(FEATURE, string.Format("contract line sent to player %1 (%2)", playerId, reason));
	}

	//------------------------------------------------------------------------------------------------
	//! Tests and harness: copies the send journal ("uid|payload", newest last).
	static void GetJournal(notnull array<string> entries)
	{
		entries.Clear();
		if (s_aJournal)
			entries.Copy(s_aJournal);
	}

	//------------------------------------------------------------------------------------------------
	static void ClearJournal()
	{
		s_aJournal = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the server sent this player's line, or an empty payload to clear it. `nowMs` is the
	//! client tick the countdown starts from.
	static void Receive(string payload, int nowMs)
	{
		string contractId;
		int limit;
		bool hasStake;
		if (payload.IsEmpty() || !Decode(payload, contractId, limit, hasStake))
		{
			if (!payload.IsEmpty())
				LF_Debug.Warn(FEATURE, string.Format("contract line ignored: '%1' is not a line", payload));

			ClearClient();
			LF_ContractTrackerHud.Refresh();
			return;
		}

		s_sClientId = contractId;
		s_iClientLimit = limit;
		s_bClientCost = hasStake;
		s_iClientReceivedMs = nowMs;
		LF_ContractTrackerHud.Refresh();
	}

	//------------------------------------------------------------------------------------------------
	//! Client: forgets the line (nothing is drawn by this call; Receive and the HUD tick do that).
	static void ClearClient()
	{
		s_sClientId = string.Empty;
		s_iClientLimit = 0;
		s_bClientCost = false;
		s_iClientReceivedMs = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: whether a line is held (and its time is not gone).
	static bool ClientHasLine()
	{
		return !s_sClientId.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	static string ClientContractId()
	{
		return s_sClientId;
	}

	//------------------------------------------------------------------------------------------------
	static bool ClientHasCost()
	{
		return s_bClientCost;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the time word at `nowMs`. The line goes when the time is gone: this clears it and
	//! answers EXPIRED once.
	static LF_EContractTimeWord ClientTimeWord(int nowMs)
	{
		if (s_sClientId.IsEmpty())
			return LF_EContractTimeWord.NONE;

		float remaining = s_iClientLimit - (nowMs - s_iClientReceivedMs) / 1000.0;	// lf-float: seconds of client tick time
		LF_EContractTimeWord word = TimeWord(remaining, s_iClientLimit);
		if (word == LF_EContractTimeWord.EXPIRED)
			ClearClient();

		return word;
	}
}

//------------------------------------------------------------------------------------------------
//! The tracker line on screen: a small paper tag built from UI kit parts (the frame sheet and two
//! lines painted from LF_UITheme), no layout of its own. Client only.
class LF_ContractTrackerHud
{
	static const int TICK_MS = 1000;	// lf-fixed: UI refresh, the words change slowly
	static const float TAG_WIDTH = 420;	// lf-fixed: tag size in pixels at the 1920x1080 reference
	static const float TAG_HEIGHT = 56;	// lf-fixed: tag size in pixels at the 1920x1080 reference
	static const float RIM = 3;			// lf-fixed: metal rim around the tag, pixels
	static const float TAG_MARGIN = 16;	// lf-fixed: gap to the screen corner
	static const float TAG_TOP = 64;	// lf-fixed: below the clip stamp tag
	static const float OPACITY = 0.9;	// lf-fixed: readable but not heavy

	protected static Widget s_wRoot;
	protected static TextWidget s_wLine;
	protected static TextWidget s_wCost;
	protected static bool s_bTicking;

	//------------------------------------------------------------------------------------------------
	static void Remove()
	{
		if (s_bTicking && GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(Tick);

		s_bTicking = false;
		if (s_wRoot)
			s_wRoot.RemoveFromHierarchy();

		s_wRoot = null;
		s_wLine = null;
		s_wCost = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Show or hide to match the client's line. No workspace (a dedicated server, a test) draws nothing.
	static void Refresh()
	{
		if (!LF_ContractTracker.ClientHasLine())
		{
			Remove();
			return;
		}

		if (s_wRoot || !GetGame())
			return;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		s_wRoot = workspace.CreateWidgets(LF_UITheme.FRAME_LAYOUT);
		if (!s_wRoot)
		{
			LF_Debug.Error(LF_ContractTracker.FEATURE, "the contract tag frame part did not load; the line stays hidden");
			return;
		}

		HideChild("BackgroundBlur");
		HideChild("Rule");
		HideChild("Content");
		Widget sheet = s_wRoot.FindAnyWidget("Sheet");
		if (!sheet)
		{
			LF_Debug.Error(LF_ContractTracker.FEATURE, "the contract tag frame has no Sheet widget");
			Remove();
			return;
		}

		FrameSlot.SetAnchorMin(sheet, 0, 0);
		FrameSlot.SetAnchorMax(sheet, 0, 0);
		FrameSlot.SetPos(sheet, TAG_MARGIN, TAG_TOP);
		FrameSlot.SetSize(sheet, TAG_WIDTH, TAG_HEIGHT);
		Paint("Metal", LF_UITheme.Metal());
		Paint("Paper", LF_UITheme.Paper());
		Widget paper = s_wRoot.FindAnyWidget("Paper");
		if (paper)
			FrameSlot.SetOffsets(paper, RIM, RIM, RIM, RIM);

		s_wLine = MakeLine(workspace, sheet, 10, 6, 10, 28, LF_UITheme.CaptionSize(), LF_UITheme.Ink());
		s_wCost = MakeLine(workspace, sheet, 10, 29, 10, 3, LF_UITheme.CaptionSize() - 2, LF_UITheme.Ink());
		if (!s_wLine || !s_wCost)
		{
			LF_Debug.Error(LF_ContractTracker.FEATURE, "the contract tag line part did not load; the line stays hidden");
			Remove();
			return;
		}

		s_wRoot.SetOpacity(OPACITY);
		s_wRoot.SetZOrder(100);
		s_bTicking = true;
		GetGame().GetCallqueue().CallLater(Tick, TICK_MS, true);
		Tick();
	}

	//------------------------------------------------------------------------------------------------
	protected static TextWidget MakeLine(WorkspaceWidget workspace, Widget sheet, float left, float top, float right, float bottom, float size, Color color)
	{
		TextWidget text = TextWidget.Cast(workspace.CreateWidgets(LF_UITheme.LINE_LAYOUT, sheet));
		if (!text)
			return null;

		FrameSlot.SetAnchorMin(text, 0, 0);
		FrameSlot.SetAnchorMax(text, 1, 1);
		FrameSlot.SetOffsets(text, left, top, right, bottom);
		text.SetExactFontSize(size);
		text.SetColor(color);
		return text;
	}

	//------------------------------------------------------------------------------------------------
	protected static void HideChild(string name)
	{
		Widget widget = s_wRoot.FindAnyWidget(name);
		if (widget)
			widget.SetVisible(false);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Paint(string name, Color color)
	{
		Widget widget = s_wRoot.FindAnyWidget(name);
		if (widget)
			widget.SetColor(color);
	}

	//------------------------------------------------------------------------------------------------
	//! The title in the client's language: the contract's own row, or the default when it has none.
	protected static string Title(string contractId)
	{
		string key = LF_ContractTracker.TitleKey(contractId);
		string text = WidgetManager.Translate(key);
		if (text == key || text.IsEmpty())
			return WidgetManager.Translate(LF_ContractTracker.KEY_TITLE_DEFAULT);

		return text;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Tick()
	{
		if (!s_wLine || !LF_ContractTracker.ClientHasLine())
		{
			Remove();
			return;
		}

		string contractId = LF_ContractTracker.ClientContractId();
		LF_EContractTimeWord word = LF_ContractTracker.ClientTimeWord(System.GetTickCount());
		if (word == LF_EContractTimeWord.EXPIRED)
		{
			Remove();
			return;
		}

		string text = Title(contractId);
		string timeKey = LF_ContractTracker.TimeKey(word);
		if (!timeKey.IsEmpty())
			text = string.Format(WidgetManager.Translate(LF_ContractTracker.KEY_LINE), text, WidgetManager.Translate(timeKey));

		if (text != s_wLine.GetText())
			s_wLine.SetText(text);

		string cost = string.Empty;
		if (LF_ContractTracker.ClientHasCost())
			cost = WidgetManager.Translate(LF_ContractTracker.KEY_COST);

		if (cost != s_wCost.GetText())
			s_wCost.SetText(cost);
	}
}

//------------------------------------------------------------------------------------------------
//! The wire: the server sends the contract line to this controller's own client only.
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	//! Server: send this player their own contract line ("" clears it). A listen host's own
	//! controller runs the handler inside the Rpc call (docs/architecture/rpc-routing.md).
	void LF_SendContractLine(string payload)
	{
		Rpc(RpcDo_LF_ContractLine, payload);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_ContractLine(string payload)
	{
		LF_ContractTracker.Receive(payload, System.GetTickCount());
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_ContractTracker's statics (LF_StaticReset finds it by reflection).
class LF_ContractTracker_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_ContractTracker";
	}

	override void Apply()
	{
		LF_ContractTrackerHud.Remove();
		LF_ContractTracker.ResetStatics();
	}
}
