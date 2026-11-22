// LF_MapKey.c: the key column's lines, its pointer handler and the badge highlight it drives (map clean-up,
// 2026-11-22). The column itself is LF_MapLegendUI.BuildKey; the line rules are LF_MapLegend.KeyLines.

//------------------------------------------------------------------------------------------------
//! One line of the key column: a group header, or one place type with the place that shows its badge.
class LF_MapKeyLine
{
	//! The player's fold choice for the column, kept in their profile like the help card's.
	static const string PREF_PATH = "$profile:LifeFramework/map-key.txt";
	static const float COLUMN_WIDTH = 210;	// lf-fixed: a badge and a type name at the readable size
	static const float ROW_HEIGHT = 23;	// lf-fixed: compact rows that still read at 1080p
	static const float FONT_SIZE = 17;	// lf-fixed: 13 was too small to read at 1080p (lead, 2026-11-22)
	//! A column holds this many lines; more flow into a second column to its left.
	static const int MAX_PER_COLUMN = 34;	// lf-fixed: from under the tab to above the scale bar at 1080 reference

	bool m_bHeader;
	string m_sCategory;
	//! LF_MapLegend.TypeKey of the row's type; empty on a header.
	string m_sType;
	//! The row's place in the list KeyLines read; -1 on a header.
	int m_iPoi = -1;

	//------------------------------------------------------------------------------------------------
	static LF_MapKeyLine Header(string category)
	{
		LF_MapKeyLine line = new LF_MapKeyLine();
		line.m_bHeader = true;
		line.m_sCategory = category;
		return line;
	}

	//------------------------------------------------------------------------------------------------
	static LF_MapKeyLine Row(string category, string typeKey, int poi)
	{
		LF_MapKeyLine line = new LF_MapKeyLine();
		line.m_sCategory = category;
		line.m_sType = typeKey;
		line.m_iPoi = poi;
		return line;
	}

	//------------------------------------------------------------------------------------------------
	//! How many columns `lines` lines take.
	static int Columns(int lines)
	{
		return LF_Utils.MaxInt(1, (lines + MAX_PER_COLUMN - 1) / MAX_PER_COLUMN);
	}

	//------------------------------------------------------------------------------------------------
	//! How many lines each column holds: the lines spread evenly over the columns.
	static int PerColumn(int lines)
	{
		int columns = Columns(lines);
		return LF_Utils.MaxInt(1, (lines + columns - 1) / columns);
	}

	//------------------------------------------------------------------------------------------------
	//! Where each line goes: column and row, reading down the left column first, then the next. A header never
	//! ends a column: it moves to the top of the next one with its rows. \return how many columns.
	static int Place(notnull array<ref LF_MapKeyLine> lines, int perColumn, notnull array<int> cols, notnull array<int> rows)
	{
		cols.Clear();
		rows.Clear();
		int column;
		int row;
		foreach (LF_MapKeyLine line : lines)
		{
			if (row >= perColumn || (line.m_bHeader && row == perColumn - 1 && row > 0))
			{
				column++;
				row = 0;
			}

			cols.Insert(column);
			rows.Insert(row);
			row++;
		}

		return column + 1;
	}
}

//------------------------------------------------------------------------------------------------
//! A click on the key tab folds or unfolds the column.
class LF_MapKeyTabHandler : ScriptedWidgetEventHandler
{
	LF_MapLegendUI m_Owner;

	//------------------------------------------------------------------------------------------------
	override bool OnClick(Widget w, int x, int y, int button)
	{
		if (m_Owner && button == 0)
			m_Owner.OnKeyTabClicked();

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The pointer on one key line: enter lifts its badges, leave drops them, a click filters.
class LF_MapKeyRowHandler : ScriptedWidgetEventHandler
{
	LF_MapLegendUI m_Owner;
	ref LF_MapKeyLine m_Line;
	Widget m_wRow;

	//------------------------------------------------------------------------------------------------
	override bool OnMouseEnter(Widget w, int x, int y)
	{
		if (m_Owner)
			m_Owner.HighlightRow(m_Line);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool OnMouseLeave(Widget w, Widget enterW, int x, int y)
	{
		if (m_Owner)
			m_Owner.HighlightRow(null);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool OnClick(Widget w, int x, int y, int button)
	{
		if (m_Owner && button == 0)
			m_Owner.ClickRow(m_Line);

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The places layer lifts the badges of one type while the key row for it is under the pointer: those marks
//! grow and pulse, every other mark dims. A crowded mark lifts when any place in it is of that type.
modded class LF_MapPlacesUI
{
	protected static const float LIFT_SCALE = 1.35;	// lf-fixed: a lifted badge is this much bigger
	protected static const float LIFT_PULSE = 0.12;	// lf-fixed: and breathes this much around it
	protected static const float LIFT_HZ = 1.6;	// lf-fixed: a calm pulse
	protected static const float DIM_ALPHA = 0.3;	// lf-fixed: the other marks while one type is lifted

	protected string m_sLifted;
	protected float m_fLiftClock;
	protected ref array<bool> m_aLifted = {};

	//------------------------------------------------------------------------------------------------
	//! Lifts the marks of `typeKey` (LF_MapLegend.TypeKey), or none for "".
	void HighlightType(string typeKey)
	{
		if (typeKey == m_sLifted)
			return;

		m_sLifted = typeKey;
		m_fLiftClock = 0;
		m_aLifted.Clear();
		int lifted;
		foreach (int i, LF_MapClusterGroup group : m_aGroups)
		{
			bool match = false;
			if (!typeKey.IsEmpty())
			{
				foreach (int member : group.m_aMembers)
				{
					if (LF_MapLegend.TypeKey(m_aPois[member]) == typeKey)
					{
						match = true;
						break;
					}
				}
			}

			m_aLifted.Insert(match);
			if (match)
				lifted++;

			if (i >= m_aWidgets.Count() || !m_aWidgets[i])
				continue;

			if (typeKey.IsEmpty() || match)
				m_aWidgets[i].SetOpacity(1);
			else
				m_aWidgets[i].SetOpacity(DIM_ALPHA);

			FrameSlot.SetSize(m_aWidgets[i], m_aSizes[i], m_aSizes[i]);
		}

		if (!typeKey.IsEmpty())
			LF_Debug.Info(FEATURE, string.Format("key hover lifts %1 mark(s)", lifted));
	}

	//------------------------------------------------------------------------------------------------
	//! One frame of the lifted marks' pulse.
	void PulseHighlight(float timeSlice)
	{
		if (m_sLifted.IsEmpty())
			return;

		// A rebuild (zoom end, filter) made new marks: lift them again for the row still under the pointer.
		if (m_aLifted.Count() != m_aGroups.Count())
		{
			string lifted = m_sLifted;
			m_sLifted = "";
			HighlightType(lifted);
		}

		m_fLiftClock += timeSlice;
		float scale = LIFT_SCALE + LIFT_PULSE * Math.Sin(m_fLiftClock * LIFT_HZ * Math.PI2);	// lf-float: an animation phase
		int count = LF_Utils.MinInt(m_aLifted.Count(), m_aWidgets.Count());
		for (int i = 0; i < count; i++)
		{
			if (!m_aLifted[i] || !m_aWidgets[i] || i >= m_aSizes.Count())
				continue;

			float size = m_aSizes[i] * scale;
			FrameSlot.SetSize(m_aWidgets[i], size, size);
		}
	}
}
