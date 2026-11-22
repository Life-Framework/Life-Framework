// LF_BusStopSheet.c - the timetable at a bus stop. Client only, on the UI kit (LF_UIScreen), so it
// takes the 1989 theme from LF_UITheme: no new .layout, no colours or fonts here. One row per other
// stop (name, fare, wait); its button asks the server for a ticket (SCR_PlayerController.LF_AskBusTrip)
// and the answer is written on the sheet. The fare shown is priced from the same config the server
// charges by; the server's price is the one paid.

class LF_BusStopSheet
{
	static const float CLOSE_DISTANCE = 5;	// lf-fixed: the stop reach; walking away closes the timetable
	static const string PART_LIST = "BusStops";
	static const string PART_STATUS = "BusStatus";
	static const string ROW_PREFIX = "BusTo_";
	static const float LIST_HEIGHT = 320;	// lf-fixed: about eight rows before the list scrolls
	static const float SHEET_WIDTH = 760;	// lf-fixed: a stop name, the fare and wait, and the button fit on one row

	protected static ref LF_BusStopSheet s_Sheet;

	protected ref LF_UIScreen m_Screen;
	protected string m_sFromId;
	protected ref map<string, string> m_mRowStops = new map<string, string>();
	protected bool m_bAsked;

	//------------------------------------------------------------------------------------------------
	static array<ref LF_UIPart> BuildParts(string fromLabel)
	{
		ref array<ref LF_UIPart> parts = {};
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.TITLE, "BusTitle", "#LF-BusStop_Title"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "BusFrom", LF_Terms.Text("#LF-BusStop_From", WidgetManager.Translate(fromLabel))));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "BusIntro", "#LF-BusStop_Intro"));
		parts.Insert(LF_UIPart.CreateList(PART_LIST));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, PART_STATUS, ""));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.CLOSE, "BusClose", "#LF-BusStop_Close"));
		return parts;
	}

	//------------------------------------------------------------------------------------------------
	//! The other stops, nearest first, as (id, entity) pairs. Pure over the given list.
	static void OtherStops(notnull IEntity from, notnull array<IEntity> stops, notnull array<IEntity> into)
	{
		vector origin = from.GetOrigin();
		foreach (IEntity stop : stops)
		{
			if (!stop || stop == from)
				continue;

			float d = vector.DistanceXZ(origin, stop.GetOrigin());
			int at = into.Count();
			for (int i = 0; i < into.Count(); i++)
			{
				if (d < vector.DistanceXZ(origin, into[i].GetOrigin()))
				{
					at = i;
					break;
				}
			}

			into.InsertAt(stop, at);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! A printed timetable: the notice paper, wide enough for a stop name, its fare and wait, and the button on one row.
	static LF_UISkin Skin()
	{
		LF_UISkin skin = LF_UISkin.Notice(null, 34, 64, 120);
		skin.m_fWidth = SHEET_WIDTH;
		return skin;
	}

	//------------------------------------------------------------------------------------------------
	static LF_UIScreen Open(notnull IEntity stopEntity)
	{
		Hide();
		LF_BusStopComponent stop = LF_BusStopComponent.Of(stopEntity);
		if (!stop)
		{
			LF_Debug.Error(LF_BusStop.FEATURE, "bus timetable not opened: the sign has no LF_BusStopComponent");
			return null;
		}

		LF_BusStopSheet sheet = new LF_BusStopSheet();
		sheet.m_sFromId = stop.GetStopId();
		sheet.m_Screen = LF_UIScreen.Open(BuildParts(stop.GetLabel()), Skin());
		if (!sheet.m_Screen)
		{
			LF_Debug.Error(LF_BusStop.FEATURE, "bus timetable failed to open");
			return null;
		}

		sheet.m_Screen.GetOnButton().Insert(sheet.OnButton);
		sheet.m_Screen.GetOnClosed().Insert(sheet.OnClosed);
		sheet.m_Screen.CloseOnDeath();
		sheet.m_Screen.CloseWhenUncontrolled();
		sheet.m_Screen.CloseBeyond(CLOSE_DISTANCE, stopEntity.GetID());
		sheet.m_Screen.SetListHeight(PART_LIST, LIST_HEIGHT);
		s_Sheet = sheet;
		int rows = sheet.FillRows(stopEntity);
		LF_Debug.Info(LF_BusStop.FEATURE, string.Format("bus timetable open at %1: %2 destinations", sheet.m_sFromId, rows));
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

		LF_BusStopSheet sheet = s_Sheet;
		s_Sheet = null;
		if (sheet.m_Screen)
			sheet.m_Screen.Close();
	}

	//------------------------------------------------------------------------------------------------
	//! Client, from the server's answer.
	static void OnResult(int refusal, int fare, int waitSeconds)
	{
		if (refusal == LF_EBusRefusal.NONE)
		{
			LF_Debug.Info(LF_BusStop.FEATURE, string.Format("bus ticket bought: fare %1, wait %2 s", fare, waitSeconds));
			if (s_Sheet && s_Sheet.m_Screen)
				s_Sheet.m_Screen.SetLine(PART_STATUS, LF_Terms.Text("#LF-BusStop_Boarded", fare.ToString(), waitSeconds.ToString()));

			// The wait is the player's own to see: the hint stays while the sheet closes.
			SCR_HintManagerComponent.ShowCustomHint(LF_Terms.Text("#LF-BusStop_Boarded", fare.ToString(), waitSeconds.ToString()), WidgetManager.Translate("#LF-BusStop_Title"), waitSeconds);
			Hide();
			return;
		}

		if (!s_Sheet)
			return;

		s_Sheet.m_bAsked = false;
		if (s_Sheet.m_Screen)
			s_Sheet.m_Screen.SetLine(PART_STATUS, LF_BusStop.RefusalKey(refusal));
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		Hide();
		s_Sheet = null;
	}

	//------------------------------------------------------------------------------------------------
	protected int FillRows(IEntity from)
	{
		array<IEntity> stops = {};
		LF_Places.FindAll(LF_BusStop.PLACE_KIND, stops);
		array<IEntity> others = {};
		OtherStops(from, stops, others);
		LF_BusStopConfig config = LF_BusStopConfig.Get();
		int rows = 0;
		foreach (IEntity stop : others)
		{
			LF_BusStopComponent component = LF_BusStopComponent.Of(stop);
			if (!component)
				continue;

			float metres = vector.Distance(from.GetOrigin(), stop.GetOrigin());
			int fare = LF_BusStop.Fare(config, metres);
			int wait = Math.Round(LF_BusStop.WaitSeconds(config, metres));
			string rowName = ROW_PREFIX + rows.ToString();
			string status = LF_Terms.Text("#LF-BusStop_Row", fare.ToString(), wait.ToString());
			if (m_Screen.AddListRow(PART_LIST, rowName, component.GetLabel(), status, "#LF-BusStop_Board"))
			{
				m_mRowStops.Set(rowName, component.GetStopId());
				rows++;
			}
		}

		if (rows == 0)
			m_Screen.SetLine(PART_STATUS, "#LF-BusStop_NoRoutes");

		return rows;
	}

	//------------------------------------------------------------------------------------------------
	protected void OnButton(string name)
	{
		if (!m_Screen || !m_mRowStops.Contains(name))
			return;

		if (m_bAsked)
			return;

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller)
		{
			LF_Debug.Error(LF_BusStop.FEATURE, "bus ticket not asked: no player controller");
			return;
		}

		string toId = m_mRowStops.Get(name);
		m_bAsked = true;
		m_Screen.SetLine(PART_STATUS, "#LF-BusStop_Asking");
		LF_Debug.Info(LF_BusStop.FEATURE, string.Format("bus ticket asked: %1 to %2", m_sFromId, toId));
		controller.LF_AskBusTrip(m_sFromId, toId);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnClosed()
	{
		if (s_Sheet != this)
			return;

		s_Sheet = null;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_BusStopSheet's statics (LF_StaticReset finds it by reflection).
class LF_BusStopSheet_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_BusStopSheet";
	}

	override void Apply()
	{
		LF_BusStopSheet.ResetStatics();
	}
}
