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
	static const float LABEL_SIZE = 18;	// lf-fixed: reference pixels, same as the body text
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
			total += Math.Sqrt(Math.Pow(next[0] - screen[i][0], 2) + Math.Pow(next[1] - screen[i][1], 2));
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
			float length = Math.Sqrt(Math.Pow(to[0] - from[0], 2) + Math.Pow(to[1] - from[1], 2));
			if (length <= 0)
				continue;

			float ux = (to[0] - from[0]) / length;
			float uy = (to[1] - from[1]) / length;
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
	static int Build(notnull LF_MapArea area, notnull array<vector> screen, float ppu, float viewW, float viewH, string themeTokens, string labelText, notnull array<ref LF_MapAreaCmd> into)
	{
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
			fill.m_iColor = ColorOf(look.m_sFillToken, look.m_iFillAlpha, themeTokens);
			fill.m_sHatch = look.m_sHatch;
			foreach (vector fillPoint : screen)
			{
				fill.m_aVertices.Insert(fillPoint[0]);
				fill.m_aVertices.Insert(fillPoint[1]);
			}

			into.Insert(fill);
		}

		int lineColor = ColorOf(look.m_sLineToken, 100, themeTokens);
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
			// The middle of the outline's screen box: where the label is centred.
			LF_MapAreaCmd label = new LF_MapAreaCmd();
			label.m_eKind = LF_MapAreaCmdKind.LABEL;
			label.m_sToken = look.m_sLineToken;
			label.m_iColor = lineColor;
			label.m_sText = labelText;
			label.m_fSize = LABEL_SIZE;
			float cx = (minX + maxX) / 2;
			float cy = (minY + maxY) / 2;
			label.m_aVertices.Insert(cx - labelText.Length() * LABEL_SIZE * LABEL_CHAR_WIDTH / 2);
			label.m_aVertices.Insert(cy - LABEL_SIZE / 2);
			if (cx >= 0 && cx <= viewW && cy >= 0 && cy <= viewH)
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
	protected ref array<ref LF_MapArea> m_aAreas = {};
	protected float m_fBuildX;
	protected float m_fBuildY;
	protected bool m_bBuilt;	// false until a build ran on a settled view (the first Update after open)

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
	//! Re-reads the areas and rebuilds every command for the view the map has now.
	protected void Redraw()
	{
		if (!m_wCanvas || !m_MapEntity)
			return;

		FrameSlot.SetOffsets(m_wCanvas, 0, 0, 0, 0);
		OriginOnScreen(m_fBuildX, m_fBuildY);
		LF_MapAreas.Collect(m_aAreas);
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		// Canvas draw commands take screen pixels, as SCR_MapEntity.WorldToScreen gives them (vanilla feeds
		// them unscaled too); DPIUnscale is for FrameSlot positions only. The canvas fills the map root, which
		// is the whole screen, and a canvas made this frame reports no size yet, so the view is the workspace's.
		float viewW = workspace.GetWidth();
		float viewH = workspace.GetHeight();
		float ppu = m_MapEntity.GetCurrentZoom();
		string tokens = ThemeTokens();
		m_aCommands.Clear();
		foreach (LF_MapArea area : m_aAreas)
		{
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
			LF_MapAreasDraw.Build(area, screen, ppu, viewW, viewH, tokens, label, records);
			foreach (LF_MapAreaCmd record : records)
				m_aCommands.Insert(ToCommand(record));
		}

		m_wCanvas.SetDrawCommands(m_aCommands);
		m_bBuilt = true;
		LF_Debug.Info(LF_MapAreas.FEATURE, string.Format("drew %1 area(s) as %2 command(s) at %3 px/m on %4x%5", m_aAreas.Count(), m_aCommands.Count(), ppu, viewW, viewH));
	}

	//------------------------------------------------------------------------------------------------
	//! The map's view is only final a frame after it opens: build again on the first frame.
	override void Update(float timeSlice)
	{
		if (m_wCanvas && !m_bBuilt)
			Redraw();
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
	}

	//------------------------------------------------------------------------------------------------
	protected void OnMapPanEnd(float x, float y)
	{
		Redraw();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnMapZoomEnd(float pixelPerUnit)
	{
		Redraw();
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
		Redraw();
		m_bBuilt = false; // the first Update builds once more on the settled view
	}

	//------------------------------------------------------------------------------------------------
	override void OnMapClose(MapConfiguration config)
	{
		SCR_MapEntity.GetOnMapPan().Remove(OnMapPan);
		SCR_MapEntity.GetOnMapPanEnd().Remove(OnMapPanEnd);
		SCR_MapEntity.GetOnMapZoomEnd().Remove(OnMapZoomEnd);
		if (m_wCanvas)
			m_wCanvas.RemoveFromHierarchy();

		m_wCanvas = null;
		m_aCommands.Clear();
		m_aAreas.Clear();
		super.OnMapClose(config);
	}
}
