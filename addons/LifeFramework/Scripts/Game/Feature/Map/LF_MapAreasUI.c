// LF_MapAreasUI.c: the area layer of the map (#2234, epic #2223, card M6; docs/architecture/everon-map.md
// section 2 "Rendering").
//
// Every area LF_MapAreas.Collect returns (the safe and lawless zones, the town line, gated and conf areas)
// is drawn on one canvas over the map: a wash or a hatch fill, an outline (solid, dashed or heavy dashed)
// and, at the closer zoom bands, a label at the centroid. Colours are the theme's `map-*` tokens
// (LF_UIThemeRecord.m_sMapTokens), so a pack restyles the map by overriding them; the built-in values
// below hold when the theme names none. The style of each kind comes from LF_MapAreas.LookOf, the one
// table the HUD cue, the emulator and this layer share.
//
// The drawing is split so a LOGIC test covers it (LF_Test_MapAreasDraw): LF_MapAreasDraw.Build turns one
// area's screen outline and the zoom into a list of draw records, with no widget and no map. The component
// at the bottom turns the records into canvas commands on map open, pan end and zoom end.

//------------------------------------------------------------------------------------------------
enum LF_MapAreaCmdKind
{
	FILL,
	OUTLINE,
	LABEL
}

//------------------------------------------------------------------------------------------------
//! One thing the canvas draws for an area. FILL: a polygon (vertices x y x y ...), hatched when m_sHatch
//! is not "none". OUTLINE: one polyline of a dash (or the whole closed outline when solid). LABEL: the
//! text's top-left corner (one x y pair) and the key or text to translate.
class LF_MapAreaCmd : Managed
{
	LF_MapAreaCmdKind m_eKind;
	ref array<float> m_aVertices = {};
	//! The theme token the colour came from, so a test and a pack author can name it.
	string m_sToken;
	//! ARGB, alpha already applied.
	int m_iColor;
	string m_sHatch;
	float m_fWidth;
	bool m_bClosed;
	string m_sText;
	float m_fSize;
}

//------------------------------------------------------------------------------------------------
//! The colours of one tessellation pass (#2397): a token's colour is parsed out of the theme string once and
//! reused by every area of that style, instead of splitting the theme string again for each area (about 80
//! parses for 40 areas, a large share of a zoom end's cost). One palette is made per pass, so a theme change
//! is seen on the next pass.
class LF_MapAreaPalette
{
	protected string m_sThemeTokens;
	protected ref map<string, int> m_mColors = new map<string, int>();

	//------------------------------------------------------------------------------------------------
	void LF_MapAreaPalette(string themeTokens)
	{
		m_sThemeTokens = themeTokens;
	}

	//------------------------------------------------------------------------------------------------
	//! Same answer as LF_MapAreasDraw.ColorOf for this palette's theme string.
	int ColorOf(string token, int alphaPercent)
	{
		string key = token + "/" + alphaPercent.ToString();
		int color;
		if (m_mColors.Find(key, color))
			return color;

		color = LF_MapAreasDraw.ColorOf(token, alphaPercent, m_sThemeTokens);
		m_mColors.Set(key, color);
		return color;
	}
}

//------------------------------------------------------------------------------------------------
//! The map area drawing rules, pure. Screen units are the canvas's reference pixels.
class LF_MapAreasDraw
{
	//! Labels show from this many pixels per metre (the emulator's rule: scale 0.3), the closer zoom bands.
	static const float LABEL_MIN_PPU = 0.3;	// lf-fixed: the zoom band where a label reads, same as the emulator
	//! An area smaller than this on screen (both sides) is not drawn at all.
	static const float MIN_SCREEN_SIZE = 4;	// lf-fixed: a speck on screen is noise
	static const float DASH_ON = 8;	// lf-fixed: a thin dash, reference pixels
	static const float DASH_OFF = 5;	// lf-fixed: its gap
	static const float HEAVY_ON = 18;	// lf-fixed: the lawless heavy dash
	static const float HEAVY_OFF = 8;	// lf-fixed: its gap
	//! A dashed outline of an area this long on screen would need more dashes than this: it is drawn solid.
	static const int MAX_DASHES = 400;	// lf-fixed: bounds the canvas commands of one huge area
	static const float LABEL_SIZE = 13;	// lf-fixed: reference pixels, a small edge note, not a headline
	//! The gap between the outline and its label, reference pixels.
	static const float LABEL_GAP = 3;	// lf-fixed
	static const float LABEL_CHAR_WIDTH = 0.5;	// lf-fixed: average glyph width as a fraction of the size, to centre a label
	//! Texture coordinates per reference pixel for the 64 px hatch tile: one tile every 32 px.
	static const float HATCH_UV_SCALE = 0.03125;	// lf-fixed: the 64 px tile (period 8) shows a 4 px stripe period
	static const string TOKEN_PREFIX = "map-";

	//------------------------------------------------------------------------------------------------
	//! The built-in colour of a token, "#RRGGBB". Empty for a name that is not an area token.
	static string BuiltInHex(string token)
	{
		if (token == "map-area-safe-fill")
			return "#468250";
		if (token == "map-area-safe-line")
			return "#326E3E";
		if (token == "map-area-lawless-fill")
			return "#B03428";
		if (token == "map-area-lawless-line")
			return "#A02820";
		if (token == "map-area-town-fill")
			return "#5A5046";
		if (token == "map-area-town-line")
			return "#1C1A16";
		if (token == "map-area-gated-fill")
			return "#555555";
		if (token == "map-area-gated-line")
			return "#1C1A16";
		if (token == "map-area-event-fill")
			return "#D9A030";
		if (token == "map-area-event-line")
			return "#C48A1E";
		if (token == "map-area-custom-fill")
			return "#5A5046";
		if (token == "map-area-custom-line")
			return "#3E403A";

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! The colour of a token: the theme string's "name=#hex" pair when it names it, else the built-in.
	//! `themeTokens` is LF_UIThemeRecord.m_sMapTokens ("name=#RRGGBB;name=#RRGGBB").
	static string HexOf(string token, string themeTokens)
	{
		array<string> pairs = {};
		themeTokens.Split(";", pairs, true);
		foreach (string pair : pairs)
		{
			array<string> parts = {};
			pair.Split("=", parts, true);
			if (parts.Count() != 2)
				continue;

			string name = parts[0].Trim();
			if (name != token)
				continue;

			string value = parts[1].Trim();
			int r, g, b, a;
			if (LF_UIThemeRecord.ParseHex(value, r, g, b, a))
				return value;
		}

		return BuiltInHex(token);
	}

	//------------------------------------------------------------------------------------------------
	//! ARGB of a token's colour with the alpha percent applied (the hex's own alpha counts, then the percent).
	static int ColorOf(string token, int alphaPercent, string themeTokens)
	{
		int r, g, b, a;
		if (!LF_UIThemeRecord.ParseHex(HexOf(token, themeTokens), r, g, b, a))
		{
			r = 128;
			g = 128;
			b = 128;
			a = 255;
		}

		int percent = Math.ClampInt(alphaPercent, 0, 100);
		int alpha = Math.Round(a * percent / 100.0);
		return (alpha << 24) | (r << 16) | (g << 8) | b;
	}

	//------------------------------------------------------------------------------------------------
	//! The alpha byte of an ARGB colour.
	static int AlphaOf(int argb)
	{
		return (argb >> 24) & 0xFF;
	}

	//------------------------------------------------------------------------------------------------
	//! The core hatch tile a style's hatch word names, empty for "none". The tile is white ink: the
	//! polygon's colour tints it, so one tile serves every hue.
	static ResourceName HatchTexture(string hatch)
	{
		if (hatch == "diagonal")
			return "{256E55302F1E2204}UI/Textures/Map/LF_MapHatch_Diagonal.edds";
		if (hatch == "cross")
			return "{BF263B38BFBB5166}UI/Textures/Map/LF_MapHatch_Cross.edds";

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! Dash and gap lengths of a style's dash word. False for "solid".
	static bool DashLengths(string dash, out float on, out float off)
	{
		if (dash == "dash")
		{
			on = DASH_ON;
			off = DASH_OFF;
			return true;
		}

		if (dash == "heavy-dash")
		{
			on = HEAVY_ON;
			off = HEAVY_OFF;
			return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Closed length of the screen outline.
	static float Perimeter(notnull array<vector> screen)
	{
		float total;
		for (int i = 0; i < screen.Count(); i++)
		{
			vector next = screen[(i + 1) % screen.Count()];
			float edgeX = next[0] - screen[i][0];
			float edgeY = next[1] - screen[i][1];
			total += Math.Sqrt(edgeX * edgeX + edgeY * edgeY);
		}

		return total;
	}

	//------------------------------------------------------------------------------------------------
	//! The outline cut into dashes: each is one polyline (corners kept) of `on` pixels, then `off` pixels
	//! of gap, walking the closed outline from its first vertex.
	static void BuildDashes(notnull array<vector> screen, float on, float off, float width, int color, string token, notnull array<ref LF_MapAreaCmd> into)
	{
		LF_MapAreaCmd dash;
		float left = on;
		bool drawing = true;
		for (int i = 0; i < screen.Count(); i++)
		{
			vector from = screen[i];
			vector to = screen[(i + 1) % screen.Count()];
			float deltaX = to[0] - from[0];
			float deltaY = to[1] - from[1];
			float length = Math.Sqrt(deltaX * deltaX + deltaY * deltaY);
			if (length <= 0)
				continue;

			float ux = deltaX / length;
			float uy = deltaY / length;
			float at;
			if (drawing && !dash)
			{
				dash = NewOutline(width, color, token, false);
				dash.m_aVertices.Insert(from[0]);
				dash.m_aVertices.Insert(from[1]);
			}

			while (at < length)
			{
				float step = Math.Min(left, length - at);
				at += step;
				left -= step;
				if (drawing)
				{
					dash.m_aVertices.Insert(from[0] + ux * at);
					dash.m_aVertices.Insert(from[1] + uy * at);
				}

				if (left > 0.0001)
					continue;

				if (drawing)
				{
					into.Insert(dash);
					dash = null;
					drawing = false;
					left = off;
				}
				else
				{
					drawing = true;
					left = on;
					dash = NewOutline(width, color, token, false);
					dash.m_aVertices.Insert(from[0] + ux * at);
					dash.m_aVertices.Insert(from[1] + uy * at);
				}
			}
		}

		if (dash && dash.m_aVertices.Count() >= 4)
			into.Insert(dash);
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_MapAreaCmd NewOutline(float width, int color, string token, bool closed)
	{
		LF_MapAreaCmd cmd = new LF_MapAreaCmd();
		cmd.m_eKind = LF_MapAreaCmdKind.OUTLINE;
		cmd.m_fWidth = width;
		cmd.m_iColor = color;
		cmd.m_sToken = token;
		cmd.m_bClosed = closed;
		return cmd;
	}

	//------------------------------------------------------------------------------------------------
	//! The records that draw one area. `screen` is its outline in canvas pixels (x, y in [0] and [1]),
	//! `ppu` the map's pixels per metre (the zoom), `viewW` and `viewH` the canvas size, `themeTokens`
	//! the theme's map colours, `labelText` the translated label (empty: none). Draws nothing for an
	//! outline off the canvas or too small to see. Order: fill, outline, label.
	//! \return The number of records added.
	static int Build(notnull LF_MapArea area, notnull array<vector> screen, float ppu, float viewW, float viewH, string themeTokens, string labelText, notnull array<ref LF_MapAreaCmd> into, LF_MapAreaPalette palette = null)
	{
		LF_MapAreaPalette colors = palette;
		if (!colors)
			colors = new LF_MapAreaPalette(themeTokens);

		if (screen.Count() < 3)
			return 0;

		float minX = screen[0][0];
		float maxX = minX;
		float minY = screen[0][1];
		float maxY = minY;
		foreach (vector point : screen)
		{
			minX = Math.Min(minX, point[0]);
			maxX = Math.Max(maxX, point[0]);
			minY = Math.Min(minY, point[1]);
			maxY = Math.Max(maxY, point[1]);
		}

		if (maxX < 0 || minX > viewW || maxY < 0 || minY > viewH)
			return 0;

		if (maxX - minX < MIN_SCREEN_SIZE && maxY - minY < MIN_SCREEN_SIZE)
			return 0;

		int start = into.Count();
		LF_MapAreaLook look = LF_MapAreas.LookOf(area.m_eStyle);
		if (look.m_iFillAlpha > 0)
		{
			LF_MapAreaCmd fill = new LF_MapAreaCmd();
			fill.m_eKind = LF_MapAreaCmdKind.FILL;
			fill.m_sToken = look.m_sFillToken;
			fill.m_iColor = colors.ColorOf(look.m_sFillToken, look.m_iFillAlpha);
			fill.m_sHatch = look.m_sHatch;
			foreach (vector fillPoint : screen)
			{
				fill.m_aVertices.Insert(fillPoint[0]);
				fill.m_aVertices.Insert(fillPoint[1]);
			}

			into.Insert(fill);
		}

		int lineColor = colors.ColorOf(look.m_sLineToken, 100);
		float on;
		float off;
		bool dashed = DashLengths(look.m_sDash, on, off);
		if (dashed && Perimeter(screen) / (on + off) > MAX_DASHES)
			dashed = false;

		if (dashed)
		{
			BuildDashes(screen, on, off, look.m_iLineWidth, lineColor, look.m_sLineToken, into);
		}
		else
		{
			LF_MapAreaCmd line = NewOutline(look.m_iLineWidth, lineColor, look.m_sLineToken, true);
			foreach (vector linePoint : screen)
			{
				line.m_aVertices.Insert(linePoint[0]);
				line.m_aVertices.Insert(linePoint[1]);
			}

			into.Insert(line);
		}

		if (ppu >= LABEL_MIN_PPU && !labelText.IsEmpty())
		{
			// Small type just under the outline, centred on it (above it when the bottom edge is off the
			// canvas): a label over the middle sat on the badges inside the zone (map clean-up, 2026-11-22).
			LF_MapAreaCmd label = new LF_MapAreaCmd();
			label.m_eKind = LF_MapAreaCmdKind.LABEL;
			label.m_sToken = look.m_sLineToken;
			label.m_iColor = lineColor;
			label.m_sText = labelText;
			label.m_fSize = LABEL_SIZE;
			float cx = (minX + maxX) / 2;
			float top = maxY + LABEL_GAP;
			if (top + LABEL_SIZE > viewH)
				top = minY - LABEL_GAP - LABEL_SIZE;

			label.m_aVertices.Insert(cx - labelText.Length() * LABEL_SIZE * LABEL_CHAR_WIDTH / 2);
			label.m_aVertices.Insert(top);
			if (cx >= 0 && cx <= viewW && top >= 0 && top + LABEL_SIZE <= viewH)
				into.Insert(label);
		}

		return into.Count() - start;
	}
}

//------------------------------------------------------------------------------------------------
//! The map UI component: one canvas over the map, re-tessellated on open, pan end and zoom end (not per
//! frame during a drag: the canvas is moved by the pan since the last build). Listed in
//! `Configs/Map/LF_MapFullscreen.conf`. Client only (the map is).
class LF_MapAreasUI : SCR_MapUIBaseComponent
{
	protected CanvasWidget m_wCanvas;
	protected ref array<ref CanvasWidgetCommand> m_aCommands = {};
	//! Records one frame's slice of a rebuild may add (about 1 ms of script).
	protected static const int SLICE_COMMANDS = 120;	// lf-fixed: bounds the work of one frame; 40 areas take some five frames
	protected float m_fBuildX;
	protected float m_fBuildY;
	protected bool m_bBuilt;	// false until a build ran on a settled view (the first Update after open)
	// The rebuild in progress (#2397): its areas, the commands so far, the theme it reads and where it is.
	protected ref array<ref LF_MapArea> m_aJobAreas = {};
	protected ref array<ref CanvasWidgetCommand> m_aJobCommands = {};
	protected ref LF_MapAreaPalette m_JobPalette;
	protected string m_sJobTokens;
	protected int m_iJobNext = -1;	// the next area to build, -1 when no rebuild is running
	protected float m_fJobX;	// where the world origin was on screen when the job began
	protected float m_fJobY;

	//------------------------------------------------------------------------------------------------
	protected static string ThemeTokens()
	{
		return LF_UITheme.Base().m_sMapTokens;
	}

	//------------------------------------------------------------------------------------------------
	//! Where the world origin is on screen now, in canvas units.
	protected void OriginOnScreen(out float x, out float y)
	{
		float screenX;
		float screenY;
		m_MapEntity.WorldToScreen(0, 0, screenX, screenY, true);
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		x = workspace.DPIUnscale(screenX);
		y = workspace.DPIUnscale(screenY);
	}

	//------------------------------------------------------------------------------------------------
	//! Why an area made no commands: its point count, its first world point and its screen box.
	protected void LogNotDrawn(notnull LF_MapArea area, notnull array<vector> screen)
	{
		vector first;
		if (!area.m_aOutline.IsEmpty())
			first = area.m_aOutline[0];

		vector lo;
		vector hi;
		if (!screen.IsEmpty())
		{
			lo = screen[0];
			hi = screen[0];
		}

		foreach (vector point : screen)
		{
			lo[0] = Math.Min(lo[0], point[0]);
			lo[1] = Math.Min(lo[1], point[1]);
			hi[0] = Math.Max(hi[0], point[0]);
			hi[1] = Math.Max(hi[1], point[1]);
		}

		LF_Debug.Info(LF_MapAreas.FEATURE, string.Format("area %1 not drawn: %2 point(s), first world %3, screen box %4..%5", area.m_sName, area.m_aOutline.Count(), first, lo, hi));
	}

	//------------------------------------------------------------------------------------------------
	//! Re-reads the areas and rebuilds every command for the view the map has now, all in this call (the map
	//! opening: nothing is drawn yet, so there is nothing to hold on screen meanwhile).
	protected void Redraw()
	{
		if (!m_wCanvas || !m_MapEntity)
			return;

		StartJob();
		while (m_iJobNext >= 0)
			StepJob();
	}

	//------------------------------------------------------------------------------------------------
	//! Starts a rebuild for the view the map has now, to be run a slice a frame by Update (#2397): 40 areas
	//! take about 4 ms of script (the dashes alone are some 500 small records), which in one frame was a hitch
	//! on top of the places layer's rebuild. The canvas keeps showing the last commit until the new one is done.
	protected void StartJob()
	{
		if (!m_wCanvas || !m_MapEntity)
			return;

		OriginOnScreen(m_fJobX, m_fJobY);
		LF_MapAreas.Collect(m_aJobAreas);
		BeginJob();
	}

	//------------------------------------------------------------------------------------------------
	//! Starts the job on the areas already in m_aJobAreas (the probe fills them with its own).
	protected void BeginJob()
	{
		m_aJobCommands.Clear();
		m_sJobTokens = ThemeTokens();
		m_JobPalette = new LF_MapAreaPalette(m_sJobTokens);
		m_iJobNext = 0;
		if (m_aJobAreas.IsEmpty())
			Commit();
	}

	//------------------------------------------------------------------------------------------------
	//! One frame's slice of the job: areas until SLICE_COMMANDS records were added. The last slice commits.
	protected void StepJob()
	{
		if (m_iJobNext < 0)
			return;

		if (!m_MapEntity)
		{
			m_iJobNext = -1;
			return;
		}

		m_iJobNext = BuildSlice(m_aJobAreas, m_iJobNext, SLICE_COMMANDS, m_sJobTokens, m_JobPalette, m_aJobCommands, true);
		if (m_iJobNext >= m_aJobAreas.Count())
			Commit();
	}

	//------------------------------------------------------------------------------------------------
	//! Puts the finished job on the canvas in one call. The map may have panned since the job began (a pan
	//! restarts the job, but one that ended this frame did not see it), so the canvas is offset by the difference.
	protected void Commit()
	{
		m_iJobNext = -1;
		array<ref CanvasWidgetCommand> done = m_aJobCommands;
		m_aJobCommands = m_aCommands;
		m_aCommands = done;
		m_fBuildX = m_fJobX;
		m_fBuildY = m_fJobY;
		float nowX;
		float nowY;
		OriginOnScreen(nowX, nowY);
		float dx = nowX - m_fBuildX;
		float dy = nowY - m_fBuildY;
		FrameSlot.SetOffsets(m_wCanvas, dx, dy, dx, dy);
		m_wCanvas.SetDrawCommands(m_aCommands);
		m_bBuilt = true;
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (workspace)
			LF_Debug.Info(LF_MapAreas.FEATURE, string.Format("drew %1 area(s) as %2 command(s) at %3 px/m on %4x%5", m_aJobAreas.Count(), m_aCommands.Count(), m_MapEntity.GetCurrentZoom(), workspace.GetWidth(), workspace.GetHeight()));
	}

	//------------------------------------------------------------------------------------------------
	//! Tessellates areas[from..] for the view the map has now, stopping once `budget` records were added (at
	//! least one area is always done). With `convert` the canvas commands are added to `commands`.
	//! \return the index of the next area, the count when all are done.
	protected int BuildSlice(notnull array<ref LF_MapArea> areas, int from, int budget, string tokens, notnull LF_MapAreaPalette palette, notnull array<ref CanvasWidgetCommand> commands, bool convert)
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		// Canvas draw commands take screen pixels, as SCR_MapEntity.WorldToScreen gives them (vanilla feeds
		// them unscaled too); DPIUnscale is for FrameSlot positions only. The canvas fills the map root, which
		// is the whole screen, and a canvas made this frame reports no size yet, so the view is the workspace's.
		float viewW = workspace.GetWidth();
		float viewH = workspace.GetHeight();
		float ppu = m_MapEntity.GetCurrentZoom();
		int added;
		for (int at = from; at < areas.Count(); at++)
		{
			LF_MapArea area = areas[at];
			array<vector> screen = {};
			foreach (vector world : area.m_aOutline)
			{
				float sx;
				float sy;
				m_MapEntity.WorldToScreen(world[0], world[2], sx, sy, true);
				screen.Insert(Vector(sx, sy, 0));
			}

			string label = LF_MapAreas.LabelKeyOf(area);
			if (label.StartsWith("#"))
				label = WidgetManager.Translate(label);

			array<ref LF_MapAreaCmd> records = {};
			if (LF_MapAreasDraw.Build(area, screen, ppu, viewW, viewH, tokens, label, records, palette) == 0)
				LogNotDrawn(area, screen);

			added += records.Count();
			if (convert)
			{
				foreach (LF_MapAreaCmd record : records)
					commands.Insert(ToCommand(record));
			}

			if (added >= budget)
				return at + 1;
		}

		return areas.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! HARNESS ONLY (`map demo`): puts the 40 stand-in areas on the canvas through the same frame slices a real
	//! zoom end uses (they stay until the next pan or zoom end re-reads the real ones). \return false with no map.
	bool Demo()
	{
		array<ref LF_MapArea> synthetic = {};
		if (!StandIns(synthetic))
			return false;

		m_aJobAreas.Clear();
		foreach (LF_MapArea fake : synthetic)
		{
			m_aJobAreas.Insert(fake);
		}

		OriginOnScreen(m_fJobX, m_fJobY);
		BeginJob();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! 40 circle areas of the six styles in an 8 by 5 grid over the ground the map shows now.
	protected bool StandIns(notnull array<ref LF_MapArea> into)
	{
		if (!m_wCanvas || !m_MapEntity)
			return false;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		float ppu = m_MapEntity.GetCurrentZoom();
		if (!workspace || ppu <= 0)
			return false;

		float centreX;
		float centreZ;
		m_MapEntity.GetMapCenterWorldPosition(centreX, centreZ);
		float viewM = workspace.GetWidth() / ppu;
		float viewH = workspace.GetHeight() / ppu;
		float cell = viewM / 8;
		for (int a = 0; a < 40; a++)
		{
			LF_MapArea area = new LF_MapArea();
			area.m_sName = "probe-area-" + a.ToString();
			area.m_eStyle = a % 6;
			area.m_sLabel = "Probe area " + a.ToString();
			int col = a % 8;
			int row = a / 8;
			float x = centreX - viewM / 2 + cell * (col + 0.5);
			float z = centreZ - viewH / 2 + cell * (row + 0.5);
			LF_MapAreas.CircleOutline(Vector(x, 0, z), cell * 0.4, LF_MapAreas.DEFAULT_SEGMENTS, area.m_aOutline);
			into.Insert(area);
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The `map probe` measurement: 40 synthetic circle areas in the current view, projected and rebuilt as
	//! frame slices over many passes (a zoom end's area cost). \return the log line.
	string Probe()
	{
		array<ref LF_MapArea> synthetic = {};
		if (!StandIns(synthetic))
			return "probe areas: map not ready";

		int passes = 50;
		int start = System.GetTickCount();
		for (int j = 0; j < passes; j++)
		{
			foreach (LF_MapArea projected : synthetic)
			{
				foreach (vector world : projected.m_aOutline)
				{
					float sx;
					float sy;
					m_MapEntity.WorldToScreen(world[0], world[2], sx, sy, true);
				}
			}
		}

		int projectMs = System.GetTickCount() - start;

		// The whole rebuild as frame slices: the time of all of them, how many frames it takes, and the
		// dearest slice (a slice is one frame's cost; whole milliseconds, so a fast slice reads 0).
		int steps = 0;
		int worstMs = 0;
		int slicesMs = 0;
		for (int p = 0; p < passes; p++)
		{
			m_aJobAreas.Clear();
			foreach (LF_MapArea fake : synthetic)
			{
				m_aJobAreas.Insert(fake);
			}

			BeginJob();
			while (m_iJobNext >= 0)
			{
				int stepStart = System.GetTickCount();
				StepJob();
				int stepMs = System.GetTickCount() - stepStart;
				slicesMs += stepMs;
				worstMs = LF_Utils.MaxInt(worstMs, stepMs);
				steps++;
			}
		}

		string line = string.Format("probe areas: 40 areas, %1 commands: projection %2 us; rebuild as slices: %3 us per pass in %4 frame(s), %5 us per frame, dearest frame %6 ms", m_aCommands.Count(), projectMs * 1000 / passes, slicesMs * 1000 / passes, steps / passes, slicesMs * 1000 / steps, worstMs);
		LF_Debug.Info(LF_MapAreas.FEATURE, line);
		Redraw();
		return line;
	}

	//------------------------------------------------------------------------------------------------
	//! The map's view is only final a frame after it opens: build again on the first frame.
	override void Update(float timeSlice)
	{
		if (m_wCanvas && !m_bBuilt)
		{
			Redraw();
			return;
		}

		if (m_iJobNext >= 0)
			StepJob();
	}

	//------------------------------------------------------------------------------------------------
	protected CanvasWidgetCommand ToCommand(notnull LF_MapAreaCmd record)
	{
		if (record.m_eKind == LF_MapAreaCmdKind.FILL)
		{
			PolygonDrawCommand polygon = new PolygonDrawCommand();
			polygon.m_iColor = record.m_iColor;
			polygon.m_Vertices = new array<float>();
			polygon.m_Vertices.Copy(record.m_aVertices);
			ResourceName texture = LF_MapAreasDraw.HatchTexture(record.m_sHatch);
			if (!texture.IsEmpty())
			{
				polygon.m_pTexture = CanvasWidget.LoadTexture(texture);
				polygon.m_fUVScale = LF_MapAreasDraw.HATCH_UV_SCALE;
			}

			return polygon;
		}

		if (record.m_eKind == LF_MapAreaCmdKind.LABEL)
		{
			TextDrawCommand text = new TextDrawCommand();
			text.m_iColor = record.m_iColor;
			text.m_sText = record.m_sText;
			text.m_fSize = record.m_fSize;
			text.m_Position = Vector(record.m_aVertices[0], record.m_aVertices[1], 0);
			return text;
		}

		LineDrawCommand line = new LineDrawCommand();
		line.m_iColor = record.m_iColor;
		line.m_fWidth = record.m_fWidth;
		line.m_bShouldEnclose = record.m_bClosed;
		line.m_Vertices = new array<float>();
		line.m_Vertices.Copy(record.m_aVertices);
		return line;
	}

	//------------------------------------------------------------------------------------------------
	protected void OnMapPan(float x, float y, bool adjustedPan)
	{
		if (!m_wCanvas)
			return;

		float nowX;
		float nowY;
		OriginOnScreen(nowX, nowY);
		float dx = nowX - m_fBuildX;
		float dy = nowY - m_fBuildY;
		FrameSlot.SetOffsets(m_wCanvas, dx, dy, dx, dy);
		// The slices already built were projected from where the map was: start again from where it is now.
		if (m_iJobNext >= 0)
			StartJob();
	}

	//------------------------------------------------------------------------------------------------
	//! The zoom is moving: a rebuild begun for the old scale is wasted, the zoom end starts the right one.
	protected void OnMapZoom(float pixelPerUnit)
	{
		if (m_iJobNext >= 0)
			StartJob();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnMapPanEnd(float x, float y)
	{
		StartJob();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnMapZoomEnd(float pixelPerUnit)
	{
		StartJob();
	}

	//------------------------------------------------------------------------------------------------
	override void OnMapOpen(MapConfiguration config)
	{
		super.OnMapOpen(config);

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace || !m_RootWidget)
		{
			LF_Debug.Error(LF_MapAreas.FEATURE, "the map has no root widget; the areas are not drawn");
			return;
		}

		m_wCanvas = CanvasWidget.Cast(workspace.CreateWidget(WidgetType.CanvasWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR | WidgetFlags.NOFOCUS, Color.FromInt(Color.WHITE), 0, m_RootWidget));
		if (!m_wCanvas)
		{
			LF_Debug.Error(LF_MapAreas.FEATURE, "the area canvas was not created; the areas are not drawn");
			return;
		}

		FrameSlot.SetAnchorMin(m_wCanvas, 0, 0);
		FrameSlot.SetAnchorMax(m_wCanvas, 1, 1);
		FrameSlot.SetOffsets(m_wCanvas, 0, 0, 0, 0);
		// First in the root's order would put it under the map; the root's last child is on top, so it
		// sits over the vanilla layers. Later components (the POIs, the waypoint) create theirs after.
		SCR_MapEntity.GetOnMapPan().Insert(OnMapPan);
		SCR_MapEntity.GetOnMapPanEnd().Insert(OnMapPanEnd);
		SCR_MapEntity.GetOnMapZoomEnd().Insert(OnMapZoomEnd);
		SCR_MapEntity.GetOnMapZoom().Insert(OnMapZoom);
		Redraw();
		m_bBuilt = false; // the first Update builds once more on the settled view
	}

	//------------------------------------------------------------------------------------------------
	override void OnMapClose(MapConfiguration config)
	{
		SCR_MapEntity.GetOnMapPan().Remove(OnMapPan);
		SCR_MapEntity.GetOnMapPanEnd().Remove(OnMapPanEnd);
		SCR_MapEntity.GetOnMapZoomEnd().Remove(OnMapZoomEnd);
		SCR_MapEntity.GetOnMapZoom().Remove(OnMapZoom);
		if (m_wCanvas)
			m_wCanvas.RemoveFromHierarchy();

		m_wCanvas = null;
		m_iJobNext = -1;
		m_aCommands.Clear();
		m_aJobCommands.Clear();
		m_aJobAreas.Clear();
		m_JobPalette = null;
		super.OnMapClose(config);
	}
}
