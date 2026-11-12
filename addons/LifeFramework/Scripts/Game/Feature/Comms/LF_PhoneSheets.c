// LF_PhoneSheets.c: the two paper forms of the call-back requests (#147). Typing a message and
// picking a line off a pad are correct UI (decision "In-world first"); everything else happens at
// the telephone. Client-only, on LF_UIScreen with the paper skin. The server opened each sheet and
// checks again whatever it sends back. Both close on Close, MenuBack, death, losing the character,
// or walking away from the telephone.

//------------------------------------------------------------------------------------------------
//! The caller's message slip: an optional short message, then "leave it".
class LF_PhoneSlipSheet
{
	static const string PART_MESSAGE = "PhoneSlipMessage";
	static const string PART_ERROR = "PhoneSlipError";
	static const string PART_LEAVE = "PhoneSlipLeaveButton";

	protected static ref LF_PhoneSlipSheet s_Sheet;

	protected ref LF_UIScreen m_Screen;
	protected SCR_PlayerController m_Controller;
	protected string m_sPhoneKey;
	protected int m_iMaxChars;

	//------------------------------------------------------------------------------------------------
	static array<ref LF_UIPart> BuildParts(int maxChars)
	{
		array<ref LF_UIPart> parts = {};
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.TITLE, "PhoneSlipTitle", "#LF-Phone_SlipTitle"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "PhoneSlipHint", "#LF-Phone_SlipHint"));
		if (maxChars > 0)
		{
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.FIELD, PART_MESSAGE, WidgetManager.Translate("#LF-Phone_SlipField", maxChars)));
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, PART_ERROR, ""));
		}
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.BUTTON, PART_LEAVE, "#LF-Phone_SlipLeave"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.CLOSE, "PhoneSlipClose", "#LF-Phone_SlipCancel"));
		return parts;
	}

	//------------------------------------------------------------------------------------------------
	static LF_UIScreen Open(SCR_PlayerController controller, string phoneKey, int maxChars)
	{
		Hide();
		if (!controller)
		{
			LF_Debug.Error(LF_PhoneCalls.FEATURE, "message slip refused: no player controller");
			return null;
		}

		LF_PhoneSlipSheet sheet = new LF_PhoneSlipSheet();
		sheet.m_Controller = controller;
		sheet.m_sPhoneKey = phoneKey;
		sheet.m_iMaxChars = maxChars;
		sheet.m_Screen = LF_UIScreen.Open(BuildParts(maxChars), LF_UISkin.Paper());
		if (!sheet.m_Screen)
		{
			LF_Debug.Error(LF_PhoneCalls.FEATURE, "message slip failed to open");
			return null;
		}

		sheet.m_Screen.GetOnButton().Insert(sheet.OnButton);
		sheet.m_Screen.CloseOnDeath();
		sheet.m_Screen.CloseWhenUncontrolled();
		LF_PhoneComponent phone = LF_PhoneComponent.ByKey(phoneKey);
		if (phone)
			sheet.m_Screen.CloseBeyond(LF_PhoneConfig.CordMeters(), phone.GetOwner().GetID());
		else
			LF_Debug.Warn(LF_PhoneCalls.FEATURE, "message slip open without its telephone on this client; no distance close");

		s_Sheet = sheet;
		LF_Debug.Info(LF_PhoneCalls.FEATURE, string.Format("message slip open at %1, up to %2 characters", phoneKey, maxChars));
		return sheet.m_Screen;
	}

	//------------------------------------------------------------------------------------------------
	static LF_UIScreen Current()
	{
		if (!s_Sheet)
			return null;

		return s_Sheet.m_Screen;
	}

	//------------------------------------------------------------------------------------------------
	static void Hide()
	{
		if (!s_Sheet)
			return;

		LF_PhoneSlipSheet sheet = s_Sheet;
		s_Sheet = null;
		if (sheet.m_Screen)
			sheet.m_Screen.Close();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnButton(string name)
	{
		if (name != PART_LEAVE || !m_Screen)
			return;

		string message;
		if (m_iMaxChars > 0)
			message = LF_PhoneCallBackBook.Clean(m_Screen.GetFieldText(PART_MESSAGE));

		if (message.Length() > m_iMaxChars)
		{
			m_Screen.SetLine(PART_ERROR, WidgetManager.Translate("#LF-Phone_RequestTooLongBy", message.Length() - m_iMaxChars));
			return;
		}

		SCR_PlayerController controller = m_Controller;
		string phoneKey = m_sPhoneKey;
		Hide();
		if (!controller)
		{
			LF_Debug.Error(LF_PhoneCalls.FEATURE, "call-back request not sent: no player controller");
			return;
		}

		LF_Debug.Info(LF_PhoneCalls.FEATURE, string.Format("call-back request sent from %1 (%2 characters)", phoneKey, message.Length()));
		controller.LF_PhoneAskLeave(phoneKey, message);
	}
}

//------------------------------------------------------------------------------------------------
//! The station's message pad, "While you were out": one line per request with a ring-back button.
class LF_PhonePadSheet
{
	static const string LIST = "PhonePad";
	static const string ROW_PREFIX = "PhoneRequest";
	static const float LIST_HEIGHT = 300;
	static const float PAD_WIDTH = 680;

	protected static ref LF_PhonePadSheet s_Sheet;

	protected ref LF_UIScreen m_Screen;
	protected SCR_PlayerController m_Controller;
	protected string m_sPhoneKey;

	//------------------------------------------------------------------------------------------------
	//! Pure: one pad entry ("id|time|rang|calling|place|message") as its parts. False when malformed.
	static bool ParseEntry(string entry, out int id, out string label, out string status, out bool calling)
	{
		array<string> fields = {};
		entry.Split(LF_PhoneCallBackBook.FIELD_SEPARATOR, fields, false);
		if (fields.Count() < 6 || fields[0].ToInt() <= 0)
			return false;

		id = fields[0].ToInt();
		calling = fields[3] == "1";
		string place = fields[4];
		if (place.IsEmpty())
			place = WidgetManager.Translate("#LF-Dispatch_UnknownPlace");
		else
			place = LF_WantedBoard.ResolvePlace(place);

		label = WidgetManager.Translate("#LF-Phone_PadLine", fields[1], place, fields[2]);
		status = fields[5];
		for (int i = 6; i < fields.Count(); i++)
		{
			status += LF_PhoneCallBackBook.FIELD_SEPARATOR + fields[i];
		}

		if (status.IsEmpty())
			status = WidgetManager.Translate("#LF-Phone_PadNoMessage");
		if (calling)
			status = WidgetManager.Translate("#LF-Phone_PadCalling", status);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	static LF_UIScreen Open(SCR_PlayerController controller, string phoneKey, array<string> entries)
	{
		Hide();
		if (!controller)
		{
			LF_Debug.Error(LF_PhoneCalls.FEATURE, "station pad refused: no player controller");
			return null;
		}

		array<ref LF_UIPart> parts = {};
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.TITLE, "PhonePadTitle", "#LF-Phone_PadTitle"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "PhonePadHint", "#LF-Phone_PadHint"));
		if (entries.IsEmpty())
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "PhonePadNone", "#LF-Phone_PadNone"));
		else
			parts.Insert(LF_UIPart.CreateList(LIST));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.CLOSE, "PhonePadClose", "#LF-Phone_PadClose"));

		LF_UISkin skin = LF_UISkin.Paper();
		skin.m_fWidth = PAD_WIDTH;
		LF_PhonePadSheet sheet = new LF_PhonePadSheet();
		sheet.m_Controller = controller;
		sheet.m_sPhoneKey = phoneKey;
		sheet.m_Screen = LF_UIScreen.Open(parts, skin);
		if (!sheet.m_Screen)
		{
			LF_Debug.Error(LF_PhoneCalls.FEATURE, "station pad failed to open");
			return null;
		}

		if (!entries.IsEmpty())
			sheet.m_Screen.SetListHeight(LIST, LIST_HEIGHT);

		int shown;
		foreach (string entry : entries)
		{
			int id;
			string label;
			string status;
			bool calling;
			if (!ParseEntry(entry, id, label, status, calling))
			{
				LF_Debug.Error(LF_PhoneCalls.FEATURE, string.Format("pad entry dropped: '%1'", entry));
				continue;
			}

			string action = "#LF-Phone_PadCallBack";
			if (calling)
				action = string.Empty;

			sheet.m_Screen.AddListRow(LIST, ROW_PREFIX + id.ToString(), label, status, action);
			shown++;
		}

		sheet.m_Screen.GetOnButton().Insert(sheet.OnButton);
		sheet.m_Screen.CloseOnDeath();
		sheet.m_Screen.CloseWhenUncontrolled();
		LF_PhoneComponent phone = LF_PhoneComponent.ByKey(phoneKey);
		if (phone)
			sheet.m_Screen.CloseBeyond(LF_PhoneConfig.CordMeters(), phone.GetOwner().GetID());

		s_Sheet = sheet;
		LF_Debug.Info(LF_PhoneCalls.FEATURE, string.Format("station pad open at %1: %2 request(s)", phoneKey, shown));
		return sheet.m_Screen;
	}

	//------------------------------------------------------------------------------------------------
	static LF_UIScreen Current()
	{
		if (!s_Sheet)
			return null;

		return s_Sheet.m_Screen;
	}

	//------------------------------------------------------------------------------------------------
	static void Hide()
	{
		if (!s_Sheet)
			return;

		LF_PhonePadSheet sheet = s_Sheet;
		s_Sheet = null;
		if (sheet.m_Screen)
			sheet.m_Screen.Close();
	}

	//------------------------------------------------------------------------------------------------
	//! Request id from a row name ("PhoneRequest12" -> 12), or 0.
	static int RowId(string rowName)
	{
		int prefix = ROW_PREFIX.Length();
		if (rowName.Length() <= prefix || rowName.Substring(0, prefix) != ROW_PREFIX)
			return 0;

		string digits = rowName.Substring(prefix, rowName.Length() - prefix);
		if (digits.ToInt().ToString() != digits)
			return 0;

		return digits.ToInt();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnButton(string name)
	{
		int id = RowId(name);
		if (id <= 0)
			return;

		SCR_PlayerController controller = m_Controller;
		string phoneKey = m_sPhoneKey;
		Hide();
		if (!controller)
		{
			LF_Debug.Error(LF_PhoneCalls.FEATURE, "ring-back not sent: no player controller");
			return;
		}

		LF_Debug.Info(LF_PhoneCalls.FEATURE, string.Format("ring-back asked for request %1 at %2", id, phoneKey));
		controller.LF_PhoneAskCallBack(phoneKey, id);
	}
}
