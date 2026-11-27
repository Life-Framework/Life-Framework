// LF_ChargeSheetScreen.c: the charge sheet paper (#4426, police L4). Client only; the server's half is
// LF_ChargeSheetScreenDesk.c. Built from UI kit parts on a paper skin (docs/style-guide.md "Screens"):
// no new .layout, no colours or fonts here. The one new full screen the 0.3.0 plan allows.

//------------------------------------------------------------------------------------------------
//! How a row of the sheet stands.
enum LF_EChargeRow
{
	OPEN,		//!< not ticked: Tick is offered
	TICKED,		//!< ticked and counted: Untick is offered
	STACKED,	//!< ticked but a higher code of its group counts: greyed
	OVER_CAP	//!< ticked and counted, but the rank cap is already reached: greyed
}

//------------------------------------------------------------------------------------------------
//! One row of the sheet as the screen shows it.
class LF_ChargeSheetRow : Managed
{
	string m_sCodeId;
	int m_iFine;
	int m_iSeconds;
	bool m_bJailOnly;
	LF_EChargeRow m_eState;

	//------------------------------------------------------------------------------------------------
	//! A greyed row has no button: the officer cannot act on it.
	bool IsGreyed()
	{
		return m_eState == LF_EChargeRow.STACKED || m_eState == LF_EChargeRow.OVER_CAP;
	}
}

//------------------------------------------------------------------------------------------------
//! What the sheet shows: its rows, greyed rows and the total at the foot. Pure: it asks
//! `LF_ChargeSheetRules` for every number, so the paper and the server's offer cannot disagree. The
//! server still recomputes from the codes the officer signs; this model is only the preview.
class LF_ChargeSheetModel : Managed
{
	ref array<ref LF_ChargeSheetRow> m_aRows = {};
	//! The total the rules came to for the ticked rows (0 when nothing is ticked).
	int m_iFine;
	int m_iSeconds;
	//! The rank cap the sheet was built with.
	int m_iCap;
	bool m_bCapped;
	bool m_bPayable;
	bool m_bJailOnly;
	//! True when at least one ticked row counts: Sign has something to send.
	bool m_bSignable;

	//------------------------------------------------------------------------------------------------
	//! \param[in] open The suspect's open codes, in the order the rows show.
	//! \param[in] tickedIds The ids the officer ticked.
	//! \param[in] fineCap The officer's rank cap, as the server sent it.
	static LF_ChargeSheetModel Build(notnull array<LF_CrimeEntry> open, notnull array<string> tickedIds, int fineCap)
	{
		LF_ChargeSheetModel model = new LF_ChargeSheetModel();
		model.m_iCap = fineCap;
		array<LF_CrimeEntry> ticked = {};
		foreach (LF_CrimeEntry code : open)
		{
			if (code && tickedIds.Contains(code.m_sCrimeId))
				ticked.Insert(code);
		}

		array<string> counted = {};
		if (!ticked.IsEmpty())
		{
			array<string> noRepeats = {};
			LF_ChargeSheetResult result = LF_ChargeSheetRules.EvaluateWith(ticked, noRepeats, fineCap, true, false, false);
			if (result && result.IsSigned())
			{
				model.m_iFine = result.m_iFine;
				model.m_iSeconds = result.m_iSentenceSeconds;
				model.m_bCapped = result.m_bCapped;
				model.m_bPayable = result.m_bFineOffered;
				model.m_bJailOnly = result.m_bJailOnly;
				model.m_bSignable = true;
				foreach (LF_CrimeEntry winner : result.m_aCounted)
				{
					counted.Insert(winner.m_sCrimeId);
				}

				// A jail-only code is left out of the fine count but serves its time: it is not "stacked".
				array<LF_CrimeEntry> bySentence = {};
				LF_LawCodeRules.Counted(ticked, false, bySentence);
				foreach (LF_CrimeEntry served : bySentence)
				{
					if (served.m_bJailOnly && !counted.Contains(served.m_sCrimeId))
						counted.Insert(served.m_sCrimeId);
				}
			}
		}

		// Counted rows add up in the order shown; the row that carries the running fine past the cap is greyed.
		int running;
		foreach (LF_CrimeEntry entry : open)
		{
			if (!entry)
				continue;

			LF_ChargeSheetRow row = new LF_ChargeSheetRow();
			row.m_sCodeId = entry.m_sCrimeId;
			row.m_iFine = entry.m_iFine;
			row.m_iSeconds = entry.m_iSentenceSeconds;
			row.m_bJailOnly = entry.m_bJailOnly;
			if (row.m_bJailOnly)
				row.m_iFine = 0;

			if (!tickedIds.Contains(entry.m_sCrimeId))
			{
				row.m_eState = LF_EChargeRow.OPEN;
			}
			else if (!counted.Contains(entry.m_sCrimeId))
			{
				row.m_eState = LF_EChargeRow.STACKED;
			}
			else
			{
				row.m_eState = LF_EChargeRow.TICKED;
				running += row.m_iFine;
				if (model.m_bCapped && running > fineCap && row.m_iFine > 0)
					row.m_eState = LF_EChargeRow.OVER_CAP;
			}

			model.m_aRows.Insert(row);
		}

		return model;
	}

	//------------------------------------------------------------------------------------------------
	//! The ids of the rows that count, in row order: what Sign sends.
	void TickedIds(notnull array<string> outIds)
	{
		outIds.Clear();
		foreach (LF_ChargeSheetRow row : m_aRows)
		{
			if (row.m_eState != LF_EChargeRow.OPEN)
				outIds.Insert(row.m_sCodeId);
		}
	}
}

//------------------------------------------------------------------------------------------------
//! The paper on screen: one row per open code with a Tick button, the fine and time at the foot, and
//! Sign. Ticking is local; Sign sends the ticked ids to the server (LF_AskSignChargeSheet) and the
//! server answers with the booking offer.
class LF_ChargeSheetScreen
{
	static const string FEATURE = "Law";
	static const string PART_TITLE = "ChargeSheetTitle";
	static const string PART_PICK = "ChargeSheetPick";
	static const string PART_LIST = "ChargeSheetRows";
	static const string PART_FINE = "ChargeSheetFine";
	static const string PART_TIME = "ChargeSheetTime";
	static const string PART_NOTE = "ChargeSheetNote";
	static const string PART_SIGN = "ChargeSheetSign";
	static const string PART_CLOSE = "ChargeSheetClose";
	static const string ROW_PREFIX = "ChargeSheetRow";
	static const int MAX_ROWS = 24;
	static const float LIST_HEIGHT = 230;
	static const float SHEET_WIDTH = 640;

	protected static ref LF_ChargeSheetScreen s_Sheet;

	protected ref LF_UIScreen m_Screen;
	protected ref array<ref LF_CrimeEntry> m_aCodes = {};
	protected ref array<string> m_aTicked = {};
	protected ref LF_ChargeSheetModel m_Model;
	protected int m_iCap;
	protected bool m_bDone;
	//! The harness fixture's sheet: Sign closes it without asking a server that is not there.
	protected bool m_bFixture;

	//------------------------------------------------------------------------------------------------
	static LF_UIScreen Current()
	{
		if (!s_Sheet)
			return null;

		return s_Sheet.m_Screen;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsOpen()
	{
		LF_UIScreen screen = Current();
		return screen && screen.IsOpen();
	}

	//------------------------------------------------------------------------------------------------
	static LF_UISkin Skin()
	{
		LF_UISkin skin = LF_UISkin.Paper();
		skin.m_fWidth = SHEET_WIDTH;
		return skin;
	}

	//------------------------------------------------------------------------------------------------
	//! The row name at this place in the list.
	static string RowName(int index)
	{
		return ROW_PREFIX + index.ToString();
	}

	//------------------------------------------------------------------------------------------------
	//! \return The list place a row name stands for, or -1 for any other part name.
	static int RowIndex(string name)
	{
		if (!name.StartsWith(ROW_PREFIX))
			return -1;

		string number = name.Substring(ROW_PREFIX.Length(), name.Length() - ROW_PREFIX.Length());
		if (number.IsEmpty())
			return -1;

		for (int i = 0; i < number.Length(); i++)
		{
			string ch = number.Substring(i, 1);
			if (ch < "0" || ch > "9")
				return -1;
		}

		return number.ToInt();
	}

	//------------------------------------------------------------------------------------------------
	//! What the server sent: the open code ids and the officer's rank cap. Every id is looked up in the
	//! catalog on this machine; one it does not list is left off the paper (and logged by the catalog).
	static LF_UIScreen OpenFromServer(string idsCsv, int fineCap)
	{
		array<string> ids = {};
		idsCsv.Split(",", ids, true);
		array<LF_CrimeEntry> codes = {};
		LF_LawCode.Resolve(ids, codes);
		return Open(codes, fineCap, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Opens the sheet on these codes, all unticked. A sheet already on screen is replaced.
	//! \return The screen, or null when it failed to open (logged).
	static LF_UIScreen Open(notnull array<LF_CrimeEntry> codes, int fineCap, bool fixture, bool tickAll = false)
	{
		Hide();
		LF_ChargeSheetScreen sheet = new LF_ChargeSheetScreen();
		sheet.m_iCap = fineCap;
		sheet.m_bFixture = fixture;
		foreach (LF_CrimeEntry code : codes)
		{
			if (!code)
				continue;

			if (sheet.m_aCodes.Count() >= MAX_ROWS)
			{
				LF_Debug.Warn(FEATURE, string.Format("charge sheet lists %1 codes and no more, the rest are left off the paper", MAX_ROWS));
				break;
			}

			sheet.m_aCodes.Insert(code);
			if (tickAll)
				sheet.m_aTicked.Insert(code.m_sCrimeId);
		}

		if (sheet.m_aCodes.IsEmpty())
		{
			LF_Debug.Warn(FEATURE, "charge sheet has no open code to show, not opened");
			return null;
		}

		s_Sheet = sheet;
		sheet.Show();
		if (!sheet.m_Screen)
		{
			s_Sheet = null;
			return null;
		}

		LF_Debug.Info(FEATURE, string.Format("charge sheet open, %1 code(s), cap $%2", sheet.m_aCodes.Count(), fineCap));
		return sheet.m_Screen;
	}

	//------------------------------------------------------------------------------------------------
	//! Takes the sheet down.
	static void Hide()
	{
		if (!s_Sheet)
			return;

		LF_ChargeSheetScreen sheet = s_Sheet;
		s_Sheet = null;
		sheet.m_bDone = true;
		sheet.Release();
	}

	//------------------------------------------------------------------------------------------------
	//! The parts of the sheet: heading, one line, the list, the two foot lines, a note, Sign and Close.
	static array<ref LF_UIPart> BuildParts()
	{
		ref array<ref LF_UIPart> parts = {};
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.TITLE, PART_TITLE, "#LF-ChargeSheet_Title"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, PART_PICK, "#LF-ChargeSheet_Pick"));
		parts.Insert(LF_UIPart.CreateList(PART_LIST));
		parts.Insert(LF_UIPart.CreatePair(PART_FINE, "#LF-ChargeSheet_TotalFine", "-"));
		parts.Insert(LF_UIPart.CreatePair(PART_TIME, "#LF-ChargeSheet_TotalTime", "-"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, PART_NOTE, "#LF-ChargeSheet_NothingYet"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.BUTTON, PART_SIGN, "#LF-ChargeSheet_Sign"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.CLOSE, PART_CLOSE, "#LF-ChargeSheet_Close"));
		return parts;
	}

	//------------------------------------------------------------------------------------------------
	//! The row's right-hand text: its fine and time, or what keeps it off the total.
	static string RowStatus(notnull LF_ChargeSheetRow row, int fineCap)
	{
		if (row.m_eState == LF_EChargeRow.STACKED)
			return LF_Terms.Text("#LF-ChargeSheet_Stacked");

		if (row.m_eState == LF_EChargeRow.OVER_CAP)
			return LF_Terms.Text("#LF-ChargeSheet_OverCap", LF_Terms.Text("#LF-ChargeSheet_Money", row.m_iFine.ToString()));

		int minutes = LF_BookingDesk.Minutes(row.m_iSeconds);
		if (row.m_bJailOnly)
			return LF_Terms.Text("#LF-ChargeSheet_RowJail", minutes.ToString());

		if (minutes <= 0)
			return LF_Terms.Text("#LF-ChargeSheet_RowFine", row.m_iFine.ToString());

		return LF_Terms.Text("#LF-ChargeSheet_RowBoth", row.m_iFine.ToString(), minutes.ToString());
	}

	//------------------------------------------------------------------------------------------------
	//! The button a row carries: Tick, Untick, or none for a greyed row.
	static string RowAction(notnull LF_ChargeSheetRow row)
	{
		if (row.m_eState == LF_EChargeRow.OPEN)
			return LF_Terms.Text("#LF-ChargeSheet_Tick");

		if (row.m_eState == LF_EChargeRow.TICKED)
			return LF_Terms.Text("#LF-ChargeSheet_Untick");

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	protected void Release()
	{
		if (!m_Screen)
			return;

		m_Screen.GetOnButton().Remove(OnButton);
		m_Screen.GetOnClosed().Remove(OnClosed);
		m_Screen.Close();
		m_Screen = null;
	}

	//------------------------------------------------------------------------------------------------
	protected void Show()
	{
		m_Screen = LF_UIScreen.Open(BuildParts(), Skin());
		if (!m_Screen)
		{
			LF_Debug.Error(FEATURE, "charge sheet failed to open");
			return;
		}

		m_Screen.GetOnButton().Insert(OnButton);
		m_Screen.GetOnClosed().Insert(OnClosed);
		m_Screen.CloseOnDeath();
		m_Screen.SetListHeight(PART_LIST, LIST_HEIGHT);
		m_Model = LF_ChargeSheetModel.Build(Codes(), m_aTicked, m_iCap);
		foreach (int i, LF_ChargeSheetRow row : m_Model.m_aRows)
		{
			m_Screen.AddListRow(PART_LIST, RowName(i), LF_Terms.Shown(row.m_sCodeId), RowStatus(row, m_iCap), RowAction(row));
		}

		Paint();
	}

	//------------------------------------------------------------------------------------------------
	protected array<LF_CrimeEntry> Codes()
	{
		array<LF_CrimeEntry> codes = {};
		foreach (LF_CrimeEntry code : m_aCodes)
		{
			codes.Insert(code);
		}

		return codes;
	}

	//------------------------------------------------------------------------------------------------
	//! Rebuilds the model for the ticks and puts every row, the foot and the note back on the paper.
	protected void Paint()
	{
		if (!m_Screen)
			return;

		m_Model = LF_ChargeSheetModel.Build(Codes(), m_aTicked, m_iCap);
		foreach (int i, LF_ChargeSheetRow row : m_Model.m_aRows)
		{
			m_Screen.SetStatusRow(RowName(i), LF_Terms.Shown(row.m_sCodeId), RowStatus(row, m_iCap), RowAction(row));
		}

		string fine = "-";
		string time = "-";
		string note = LF_Terms.Text("#LF-ChargeSheet_NothingYet");
		if (m_Model.m_bSignable)
		{
			fine = LF_Terms.Text("#LF-ChargeSheet_Money", m_Model.m_iFine.ToString());
			if (m_Model.m_bJailOnly)
				fine = LF_Terms.Text("#LF-ChargeSheet_None");

			time = LF_Terms.Text("#LF-ChargeSheet_Minutes", LF_BookingDesk.Minutes(m_Model.m_iSeconds).ToString());
			if (m_Model.m_iSeconds <= 0)
				time = LF_Terms.Text("#LF-ChargeSheet_None");

			note = LF_Terms.Text("#LF-ChargeSheet_ReadyNote");
			if (m_Model.m_bCapped)
				note = LF_Terms.Text("#LF-ChargeSheet_Capped", m_iCap.ToString());
		}

		m_Screen.SetPairValue(PART_FINE, fine);
		m_Screen.SetPairValue(PART_TIME, time);
		m_Screen.SetLine(PART_NOTE, note);
	}

	//------------------------------------------------------------------------------------------------
	//! The payload Sign sends: the ticked ids, comma-joined. Never a total.
	string TickedCsv()
	{
		string csv;
		array<string> ids = {};
		if (m_Model)
			m_Model.TickedIds(ids);

		foreach (string id : ids)
		{
			if (!csv.IsEmpty())
				csv += ",";

			csv += id;
		}

		return csv;
	}

	//------------------------------------------------------------------------------------------------
	protected void OnButton(string name)
	{
		if (m_bDone || !m_Screen)
			return;

		if (name == PART_SIGN)
		{
			Sign();
			return;
		}

		int index = RowIndex(name);
		if (index < 0 || !m_Model || index >= m_Model.m_aRows.Count())
			return;

		LF_ChargeSheetRow row = m_Model.m_aRows[index];
		if (row.IsGreyed())
			return;

		int at = m_aTicked.Find(row.m_sCodeId);
		if (at >= 0)
			m_aTicked.Remove(at);
		else
			m_aTicked.Insert(row.m_sCodeId);

		Paint();
	}

	//------------------------------------------------------------------------------------------------
	//! Sends the ticked ids and the total the paper showed (the server logs a mismatch and ignores it).
	protected void Sign()
	{
		if (!m_Model || !m_Model.m_bSignable)
		{
			LF_Debug.Info(FEATURE, "charge sheet: sign pressed with nothing ticked");
			m_Screen.SetLine(PART_NOTE, LF_Terms.Text("#LF-ChargeSheet_NoneTicked"));
			return;
		}

		string csv = TickedCsv();
		int shownFine = m_Model.m_iFine;
		LF_Debug.Info(FEATURE, string.Format("charge sheet signed: %1 (paper showed $%2)", csv, shownFine));
		if (!m_bFixture)
		{
			SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
			if (controller)
				controller.LF_AskSignChargeSheet(csv, shownFine);
			else
				LF_Debug.Error(FEATURE, "charge sheet: no local player controller to send the signature");
		}

		Hide();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnClosed()
	{
		if (m_bDone || s_Sheet != this)
			return;

		m_bDone = true;
		s_Sheet = null;
		m_Screen = null;
		LF_Debug.Info(FEATURE, "charge sheet closed");
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_Sheet = null;
	}
}

//------------------------------------------------------------------------------------------------
//! Harness `chargesheet fixture|close`: a fixture sheet for the session picture (four open codes, all
//! ticked, and a long-named unlicensed-item row; two share a stacking group, so one greys). `fixture-cap` adds a low cap so a row greys by cap.
modded class LF_AgentHarnessVerbs
{
	override bool Handle(LF_AgentCommand cmd)
	{
		if (cmd.Verb != "chargesheet")
			return super.Handle(cmd);

		if (cmd.Arg1 == "close")
		{
			LF_ChargeSheetScreen.Hide();
			LF_AgentHarness.Answer(cmd.Id, "ok", "closed");
			return true;
		}

		array<ref LF_CrimeEntry> keep = {};
		LF_CrimeEntry robbery = LF_CrimeEntry.Make("#LF-Offence_Robbery", 0, 300);
		robbery.m_iSentenceSeconds = 600;
		robbery.m_sStackGroup = "fixture-theft";
		LF_CrimeEntry carjacking = LF_CrimeEntry.Make("#LF-Offence_Shoplifting", 0, 100);
		carjacking.m_iSentenceSeconds = 120;
		carjacking.m_sStackGroup = "fixture-theft";
		LF_CrimeEntry vehicleTheft = LF_CrimeEntry.Make("#LF-Offence_Lockpick", 0, 150);
		vehicleTheft.m_iSentenceSeconds = 180;
		// A long name beside a status (#5147): "Carrying without a license" was cut at "licen". First, so it shows above the fold.
		LF_CrimeEntry unlicensed = LF_CrimeEntry.Make("#LF-Offence_UnlicensedItem", 0, 400);
		keep.Insert(unlicensed);
		keep.Insert(robbery);
		keep.Insert(carjacking);
		keep.Insert(vehicleTheft);
		array<LF_CrimeEntry> codes = {};
		foreach (LF_CrimeEntry code : keep)
		{
			codes.Insert(code);
		}

		int cap = 500;
		if (cmd.Arg1 == "fixture-cap")
			cap = 400;

		if (!LF_ChargeSheetScreen.Open(codes, cap, true, true))
		{
			LF_AgentHarness.Answer(cmd.Id, "error", "not_opened");
			return true;
		}

		LF_AgentHarness.Answer(cmd.Id, "ok", "opened");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
class LF_ChargeSheetScreen_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_ChargeSheetScreen";
	}

	override void Apply()
	{
		LF_ChargeSheetScreen.ResetStatics();
	}
}
