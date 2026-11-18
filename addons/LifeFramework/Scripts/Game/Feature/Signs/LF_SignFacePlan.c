// LF_SignFacePlan.c: where a styled face puts its title and rows (docs/architecture/sign-system.md).
//
// Pure script: the shown area of the size, the style's margins and fonts, the resolved title and rows in,
// boxes and font sizes out, in layout units from the top-left of the render target. LF_SignStyledFace
// paints the plan; LOGIC tests check it. The carrier is a one-mesh quad with UV 0..1, so nothing turns.

//------------------------------------------------------------------------------------------------
class LF_SignFaceRowPlan
{
	vector m_vPos;
	vector m_vSize;
	int m_iFont;
	string m_sLabel;
	string m_sValue;
	//! The leader between label and value, and where it starts inside the row.
	string m_sLeader;
	float m_fLeaderLeft;
}

//------------------------------------------------------------------------------------------------
class LF_SignFacePlan
{
	//! More rows than this are left off (logged by the painter): the board is for a short list.
	static const int MAX_ROWS = 8;	// lf-fixed: legibility, a price board is a short list
	static const int ROW_FONT_MIN = 28;	// lf-fixed: smaller does not read from 3 m on a 1.8 m board
	static const float ROW_LINE_HEIGHT = 1.3;	// lf-fixed: row pitch per font unit
	//! Mixed-case regular glyphs average about half an em; a period about a third with its spacing.
	static const float ROW_EM_PER_CHAR = 0.52;	// lf-fixed: measured on RobotoCondensed Regular
	static const float LEADER_EM = 0.34;	// lf-fixed: '.' advance in RobotoCondensed Regular
	//! Space kept clear either side of the leader, in ems.
	static const float LEADER_PAD_EM = 0.5;	// lf-fixed: typographic gap
	//! A lifted title (LF_SignStyleDef.m_fTitleLift) sits in a box this many ems tall.
	static const float TITLE_BOX_EM = 2.5;	// lf-fixed: taller than any line of the font, so the engine centres it

	vector m_vTitlePos;
	vector m_vTitleSize;
	int m_iTitleFont;
	string m_sTitle;
	float m_fTitleTilt;

	bool m_bGhost;
	vector m_vGhostPos;

	ref array<ref LF_SignFaceRowPlan> m_aRows = {};
	//! Rows over MAX_ROWS that were left off.
	int m_iDropped;

	//------------------------------------------------------------------------------------------------
	//! The plan for one face. \p labels and \p values are resolved text, one pair per row (a missing
	//! value is empty). \p shown is the size's shown area (width, height).
	static LF_SignFacePlan Compose(vector shown, notnull LF_SignStyleDef style, string title, notnull array<string> labels, notnull array<string> values)
	{
		LF_SignFacePlan plan = new LF_SignFacePlan();
		float width = shown[0];
		float height = shown[1];
		float shorter = Math.Min(width, height);
		float margin = shorter * Math.Clamp(style.m_fMargin, 0, 0.3);
		float innerW = width - 2 * margin;
		float innerH = height - 2 * margin;

		int count = labels.Count();
		if (count > MAX_ROWS)
		{
			plan.m_iDropped = count - MAX_ROWS;
			count = MAX_ROWS;
		}

		plan.m_sTitle = title;
		if (style.m_bUpperTitle)
			plan.m_sTitle.ToUpper();

		float titleH = innerH;
		if (count > 0)
			titleH = innerH * Math.Clamp(style.m_fTitleShare, 0.15, 0.6);

		string lines;
		plan.m_iTitleFont = LF_SignComponent.PaintedTitleFit(plan.m_sTitle, innerW, titleH, style.m_iTitleFontMax, lines);
		plan.m_sTitle = lines;
		plan.m_vTitlePos = Vector(margin, margin, 0);
		plan.m_vTitleSize = Vector(innerW, titleH, 0);
		float lift = 0;
		if (style.m_fTitleLift != 0)
		{
			// A client draws a title about a quarter of its font below the middle of its box (measured on a
			// 1280x720 client: bank and gendarmerie at 129 sat 29 to 32 units low, the hospital at 67 sat 20),
			// and a box shorter than its line is anchored at the top instead. So the box is tall enough for
			// the line to centre in, and raised by the seat. (m_fTitleLift)
			lift = plan.m_iTitleFont * Math.Clamp(style.m_fTitleLift, -1, 1);
			float boxH = Math.Max(titleH, plan.m_iTitleFont * TITLE_BOX_EM);
			plan.m_vTitleSize = Vector(innerW, boxH, 0);
			plan.m_vTitlePos = Vector(margin, margin + titleH * 0.5 - boxH * 0.5 - lift, 0);
		}

		plan.m_fTitleTilt = style.m_fTilt;

		if (style.m_fGhostAlpha > 0)
		{
			plan.m_bGhost = true;
			plan.m_vGhostPos = Vector(plan.m_vTitlePos[0] + style.m_vGhostShift[0], plan.m_vTitlePos[1] + style.m_vGhostShift[1], 0);
		}

		if (count == 0)
			return plan;

		// One font for every row, so the prices line up: the pitch the area allows, the style's cap, and
		// narrow enough that the longest label and value still leave room for a short leader.
		float rowsTop = margin + titleH;
		float rowsH = innerH - titleH;
		int font = Math.Floor(rowsH / (ROW_LINE_HEIGHT * count));
		font = LF_Utils.MinInt(font, style.m_iRowFontMax);

		int widest = 0;
		for (int i = 0; i < count; i++)
		{
			int chars = labels[i].Length() + ValueAt(values, i).Length();
			if (chars > widest)
				widest = chars;
		}

		float emsNeeded = widest * ROW_EM_PER_CHAR + 2 * LEADER_PAD_EM + 3 * LEADER_EM;
		if (emsNeeded > 0)
		{
			int fit = Math.Floor(innerW / emsNeeded);
			font = LF_Utils.MinInt(font, fit);
		}
		font = LF_Utils.MaxInt(font, ROW_FONT_MIN);

		float pitch = font * ROW_LINE_HEIGHT;
		for (int r = 0; r < count; r++)
		{
			LF_SignFaceRowPlan row = new LF_SignFaceRowPlan();
			row.m_iFont = font;
			row.m_vPos = Vector(margin, rowsTop + r * pitch, 0);
			row.m_vSize = Vector(innerW, pitch, 0);
			row.m_sLabel = labels[r];
			row.m_sValue = ValueAt(values, r);
			FillLeader(row, style.m_sLeader, innerW);
			plan.m_aRows.Insert(row);
		}

		return plan;
	}

	//------------------------------------------------------------------------------------------------
	protected static string ValueAt(array<string> values, int index)
	{
		if (index < values.Count())
			return values[index];

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! The leader fills the gap between the label's end and the value's start, less a pad each side.
	static void FillLeader(notnull LF_SignFaceRowPlan row, string leader, float rowWidth)
	{
		row.m_sLeader = string.Empty;
		if (leader.IsEmpty() || row.m_sValue.IsEmpty())
			return;

		float em = row.m_iFont;
		float labelW = row.m_sLabel.Length() * ROW_EM_PER_CHAR * em;
		float valueW = row.m_sValue.Length() * ROW_EM_PER_CHAR * em;
		float gap = rowWidth - labelW - valueW - 2 * LEADER_PAD_EM * em;
		int dots = Math.Floor(gap / (LEADER_EM * em * leader.Length()));
		if (dots < 2)
			return;

		row.m_fLeaderLeft = labelW + LEADER_PAD_EM * em;
		for (int i = 0; i < dots; i++)
		{
			row.m_sLeader += leader;
		}
	}
}
