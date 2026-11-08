// LF_SignComponent.c — names the LF_Signs.conf entry a board, poster, or notice shows.
//
// On a rendering client the title is printed on a paper sheet on the prop's
// face: a local LF_SignFace child (vanilla A4 poster mesh) whose material
// samples $rendertarget, fed by an RTTextureWidget (the vanilla ballistic
// table pattern). A dedicated server skips the face entirely.

[ComponentEditorProps(category: "LifeFramework/Signs", description: "Names the LF_Signs.conf entry this prop shows on Read")]
class LF_SignComponentClass : ScriptComponentClass
{
}

class LF_SignComponent : ScriptComponent
{
	static const ResourceName FACE_PREFAB = "{B7765F4913B42527}Prefabs/Props/Signs/LF_SignFace.et";
	static const ResourceName FACE_LAYOUT = "{ED05AC4807160345}UI/Layouts/Signs/LF_SignFace.layout";
	// LF_SignFace.layout: RTTexture0 size, Title margin, Title font size.
	static const float FACE_WIDTH = 512;
	static const float FACE_HEIGHT = 724;
	static const float FACE_MARGIN = 40;
	static const float FACE_FONT_SIZE = 96;
	// A game client shows only the left part of the sheet's render target: at
	// 640x360 and at 1920x1080 alike, a title centred at x 256 printed 78% of the
	// way across the paper, so about 328 of the 512 units are on the paper. The
	// full height shows. The title box sits inside the shown width.
	static const float CLIENT_TITLE_LEFT = 20;
	static const float CLIENT_TITLE_WIDTH = 290;
	// RobotoCondensed Bold capitals measured about 0.6 em each ("WELCOME" was
	// 406 units at 96). A long word does not wrap, so the font shrinks to fit it.
	static const float CLIENT_EM_PER_CHAR = 0.62;

	// LF_SignBoardFace.layout, the painted board face. The A4 sheet's UVs cover
	// u 0..0.667 and v 0..1 (PosterSingle_A4.xob), so a 768 x 724 target puts
	// 512 x 724 units on the sheet: the same scale both ways, 0.21 x 0.297 m
	// times the sheet scale. The board turns the sheet on its side (roll 90) to
	// fill its landscape face, and the title turns with it, so the title box is
	// laid out along the sheet's 724-unit side and then rotated about its centre.
	static const float BOARD_SHOWN_WIDTH = 512;
	static const float BOARD_SHOWN_HEIGHT = 724;
	static const float BOARD_TEXT_WIDTH = 644;
	static const float BOARD_TEXT_HEIGHT = 420;
	static const int BOARD_FONT_MAX = 180;
	static const int BOARD_FONT_MIN = 24;
	static const float BOARD_LINE_HEIGHT = 1.15;

	[Attribute("", UIWidgets.EditBox, "Sign id in Configs/Signs/LF_Signs.conf")]
	protected string m_sSignId;

	[Attribute("1", UIWidgets.CheckBox, "Print the title on a paper sheet on the prop face (rendering clients only)")]
	protected bool m_bShowFace;

	[Attribute("0", UIWidgets.CheckBox, "The prop mesh itself samples $rendertarget (an A4 notice with LF_SignFace.emat). No sheet is spawned")]
	protected bool m_bFaceOnOwner;

	[Attribute("0 0 -0.007", UIWidgets.EditBox, "Sheet position relative to the prop. Vanilla boards pin posters on the -Z face")]
	protected vector m_vFaceOffset;

	[Attribute("0 0 0", UIWidgets.EditBox, "Sheet rotation relative to the prop (yaw pitch roll, degrees)")]
	protected vector m_vFaceAngles;

	[Attribute("1", UIWidgets.EditBox, "Sheet scale. 1 is an A4 sheet (0.21 x 0.30 m)")]
	protected float m_fFaceScale;

	[Attribute("", UIWidgets.ResourceNamePicker, "Painted face layout (RTTexture0 with a Title text). Empty: the printed paper sheet (LF_SignFace.layout)", "layout")]
	protected ResourceName m_sFaceLayout;

	[Attribute("0", UIWidgets.EditBox, "Painted face: title rotation in degrees, so it reads level on a sheet turned on its side")]
	protected float m_fTitleRotation;

	[Attribute("-66 152 0", UIWidgets.EditBox, "Painted face: title box top-left before rotation, layout units")]
	protected vector m_vTitlePos;

	[Attribute("644 420 0", UIWidgets.EditBox, "Painted face: title box size before rotation, layout units")]
	protected vector m_vTitleSize;

	[Attribute("180", UIWidgets.EditBox, "Painted face: largest title font, layout units")]
	protected int m_iTitleFontMax;

	[Attribute("0 0 0", UIWidgets.EditBox, "Painted face: the part of the target the mesh shows (width height, layout units from the top-left). Set, it sizes the Rim and Paint images of the layout and colours them; zero leaves the layout as authored")]
	protected vector m_vFaceArea;

	[Attribute("24", UIWidgets.EditBox, "Painted face: rim width, layout units")]
	protected float m_fRimWidth;

	[Attribute("1 1 1", UIWidgets.ColorPicker, "Painted face: rim colour (RGB 0..1)")]
	protected vector m_vRimColor;

	[Attribute("0.1 0.2 0.5", UIWidgets.ColorPicker, "Painted face: paint colour (RGB 0..1)")]
	protected vector m_vPaintColor;

	[Attribute("1 1 1", UIWidgets.ColorPicker, "Painted face: lettering colour (RGB 0..1)")]
	protected vector m_vTextColor;

	[Attribute("0", UIWidgets.CheckBox, "The face is the first child entity's mesh (a plate on a pole), not the owner's")]
	protected bool m_bFaceOnChild;

	protected ref LF_SignDef m_Sign;
	protected bool m_bResolved;

	protected IEntity m_Face;
	protected IEntity m_FaceTarget;
	protected Widget m_wFaceRoot;
	protected RTTextureWidget m_wFaceTexture;

	//------------------------------------------------------------------------------------------------
	string GetSignId()
	{
		return m_sSignId;
	}

	//------------------------------------------------------------------------------------------------
	//! The configured sign, or null. Resolved once; a miss logs once and the Read action hides.
	LF_SignDef GetSign()
	{
		if (m_bResolved)
			return m_Sign;

		m_bResolved = true;
		LF_SignDef sign = LF_SignCatalog.Find(m_sSignId);
		if (!LF_SignCatalog.IsUsable(sign))
		{
			EL_Debug.Error("Signs", string.Format("sign id '%1' has no titled entry in LF_Signs.conf; Read hidden at %2", m_sSignId, GetOwner().GetOrigin()));
			return null;
		}

		m_Sign = sign;
		return m_Sign;
	}

	//------------------------------------------------------------------------------------------------
	//! The printed text on the sheet: the resolved title in capitals.
	static string FaceText(LF_SignDef sign)
	{
		if (!sign)
			return string.Empty;

		string text = LF_SignCatalog.Resolve(sign.m_sTitle);
		text.ToUpper();
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! The face is drawn only where something renders and a sheet is wanted.
	static bool WantsFace(bool showFace, bool dedicated, bool hasWorkspace)
	{
		return showFace && !dedicated && hasWorkspace;
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		if (!m_bShowFace || m_sSignId.IsEmpty())
			return;

		SetEventMask(owner, EntityEvent.INIT);
	}

	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		if (!GetGame() || !GetGame().InPlayMode())
			return;

		bool dedicated = RplSession.Mode() == RplMode.Dedicated;
		if (!WantsFace(m_bShowFace, dedicated, GetGame().GetWorkspace() != null))
			return;

		// After the world finishes loading, so the parent is placed.
		if (GetGame().GetCallqueue())
			GetGame().GetCallqueue().CallLater(BuildFace, 0, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void BuildFace()
	{
		IEntity owner = GetOwner();
		if (!owner || owner.IsDeleted() || m_wFaceRoot)
			return;

		LF_SignDef sign = GetSign();
		if (!sign)
			return;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		if (m_bFaceOnChild)
		{
			IEntity plate = owner.GetChildren();
			if (!plate)
			{
				EL_Debug.Error("Signs", "sign " + m_sSignId + " paints its child but has none");
				return;
			}

			PrintOn(plate, workspace, sign);
			return;
		}

		if (m_bFaceOnOwner)
		{
			PrintOn(owner, workspace, sign);
			return;
		}

		Resource resource = Resource.Load(FACE_PREFAB);
		if (!resource || !resource.IsValid())
		{
			EL_Debug.Error("Signs", "sign face prefab does not load: " + FACE_PREFAB);
			return;
		}

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.LOCAL;
		params.Parent = owner;
		Math3D.AnglesToMatrix(m_vFaceAngles, params.Transform);
		params.Transform[3] = m_vFaceOffset;
		params.Scale = m_fFaceScale;

		m_Face = GetGame().SpawnEntityPrefabLocal(resource, owner.GetWorld(), params);
		if (!m_Face)
		{
			EL_Debug.Error("Signs", "sign face failed to spawn for " + m_sSignId);
			return;
		}

		PrintOn(m_Face, workspace, sign);
	}

	//------------------------------------------------------------------------------------------------
	//! The render target is FACE_WIDTH x FACE_HEIGHT reference units times the
	//! screen DPI scale in pixels, but the widgets inside it lay out one
	//! reference unit per pixel. Anchored to the full sheet, the title centred on
	//! a 512-wide box while a 720p window only showed the first 340: the sheet
	//! read "MINI / ARE". The title box is placed in the part that is shown.
	//! \param dpiScale WorkspaceWidget.DPIScale(1): pixels per reference unit.
	//! \param[out] pos Title position, reference units from the sheet's top-left.
	//! \param[out] size Title size, reference units.
	//! \return Font size for the title.
	static int FaceTitleRect(float dpiScale, out vector pos, out vector size)
	{
		float scale = dpiScale;
		if (scale <= 0)
			scale = 1;

		pos = Vector(FACE_MARGIN * scale, FACE_MARGIN * scale, 0);
		size = Vector((FACE_WIDTH - 2 * FACE_MARGIN) * scale, (FACE_HEIGHT - 2 * FACE_MARGIN) * scale, 0);
		return Math.Round(FACE_FONT_SIZE * scale);
	}

	//------------------------------------------------------------------------------------------------
	//! The title box and font on a game client, in layout units (a game client
	//! scales the widgets inside the target like any widget).
	//! \param text The printed title, capitals.
	//! eturn Font size: 96, or less so the longest word fits the shown width.
	static int ClientTitleRect(string text, out vector pos, out vector size)
	{
		pos = Vector(CLIENT_TITLE_LEFT, FACE_MARGIN, 0);
		size = Vector(CLIENT_TITLE_WIDTH, FACE_HEIGHT - 2 * FACE_MARGIN, 0);

		int longest = 0;
		array<string> words = {};
		text.Split(" ", words, true);
		foreach (string word : words)
		{
			if (word.Length() > longest)
				longest = word.Length();
		}

		if (longest <= 0)
			return FACE_FONT_SIZE;

		int font = Math.Floor(CLIENT_TITLE_WIDTH / (CLIENT_EM_PER_CHAR * longest));
		if (font > FACE_FONT_SIZE)
			return FACE_FONT_SIZE;

		return font;
	}

	//------------------------------------------------------------------------------------------------
	//! A painted title: the biggest font, up to fontMax, at which the title, broken
	//! into lines at spaces, fits a box of width x height layout units (before
	//! any rotation).
	//! \param text The printed title, capitals.
	//! \param[out] lines The title with a line break where it wraps.
	//! \return Font size.
	static int PaintedTitleFit(string text, float width, float height, int fontMax, out string lines)
	{
		array<string> words = {};
		text.Split(" ", words, true);
		lines = text;
		if (words.IsEmpty())
			return fontMax;

		for (int font = fontMax; font > BOARD_FONT_MIN; font--)
		{
			int maxChars = Math.Floor(width / (CLIENT_EM_PER_CHAR * font));
			int count = WrapWords(words, maxChars, lines);
			if (count > 0 && count * font * BOARD_LINE_HEIGHT <= height)
				return font;
		}

		int minChars = Math.Floor(width / (CLIENT_EM_PER_CHAR * BOARD_FONT_MIN));
		int last = WrapWords(words, minChars, lines);
		if (last <= 0)
			lines = text;

		return BOARD_FONT_MIN;
	}

	//------------------------------------------------------------------------------------------------
	//! The cork board's painted title (LF_Sign_Board): its box is centred on the
	//! shown part of the turned sheet.
	static int BoardTitleFit(string text, out string lines, out vector pos, out vector size)
	{
		pos = Vector(BOARD_SHOWN_WIDTH * 0.5 - BOARD_TEXT_WIDTH * 0.5, BOARD_SHOWN_HEIGHT * 0.5 - BOARD_TEXT_HEIGHT * 0.5, 0);
		size = Vector(BOARD_TEXT_WIDTH, BOARD_TEXT_HEIGHT, 0);
		return PaintedTitleFit(text, BOARD_TEXT_WIDTH, BOARD_TEXT_HEIGHT, BOARD_FONT_MAX, lines);
	}

	//------------------------------------------------------------------------------------------------
	//! Greedy wrap at spaces into lines of at most maxChars characters.
	//! \return Line count, or 0 when one word is longer than maxChars.
	static int WrapWords(array<string> words, int maxChars, out string lines)
	{
		lines = string.Empty;
		string line;
		int count = 0;
		foreach (string word : words)
		{
			if (word.Length() > maxChars)
				return 0;

			if (line.IsEmpty())
			{
				line = word;
				continue;
			}

			if (line.Length() + 1 + word.Length() <= maxChars)
			{
				line = line + " " + word;
				continue;
			}

			if (count > 0)
				lines += "\n";
			lines += line;
			count++;
			line = word;
		}

		if (!line.IsEmpty())
		{
			if (count > 0)
				lines += "\n";
			lines += line;
			count++;
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! Size and colour the painted face: Rim fills the part of the target the mesh shows,
	//! Paint sits inside it by the rim width, and the title takes the lettering colour.
	protected void PaintFace(TextWidget title)
	{
		title.SetColor(Color.FromVector(m_vTextColor));
		if (m_vFaceArea[0] <= 0 || m_vFaceArea[1] <= 0)
			return;

		Widget rim = m_wFaceRoot.FindAnyWidget("Rim");
		Widget paint = m_wFaceRoot.FindAnyWidget("Paint");
		if (!rim || !paint)
		{
			EL_Debug.Error("Signs", "painted face layout is missing Rim or Paint: " + m_sSignId);
			return;
		}

		FrameSlot.SetAnchorMin(rim, 0, 0);
		FrameSlot.SetAnchorMax(rim, 0, 0);
		FrameSlot.SetPos(rim, 0, 0);
		FrameSlot.SetSize(rim, m_vFaceArea[0], m_vFaceArea[1]);
		rim.SetColor(Color.FromVector(m_vRimColor));

		FrameSlot.SetAnchorMin(paint, 0, 0);
		FrameSlot.SetAnchorMax(paint, 0, 0);
		FrameSlot.SetPos(paint, m_fRimWidth, m_fRimWidth);
		FrameSlot.SetSize(paint, m_vFaceArea[0] - 2 * m_fRimWidth, m_vFaceArea[1] - 2 * m_fRimWidth);
		paint.SetColor(Color.FromVector(m_vPaintColor));
	}

	//------------------------------------------------------------------------------------------------
	//! Lay out the painted board title, turned to read level on the turned sheet.
	protected void FitBoardTitle(TextWidget title, string text, float dpiScale, bool workbench)
	{
		string lines;
		vector pos = m_vTitlePos;
		vector size = m_vTitleSize;
		int font = PaintedTitleFit(text, size[0], size[1], m_iTitleFontMax, lines);

		// Workbench renders the target at the layout size times the DPI scale
		// while the widgets inside lay out one unit per pixel (FaceTitleRect).
		float scale = 1;
		if (workbench && dpiScale > 0)
			scale = dpiScale;

		// A turned text is still clipped to the widget's unturned rectangle, so
		// the widget is a square around the box: the lines fit the box, the clip
		// covers them at any turn.
		float side = Math.Max(size[0], size[1]);
		float left = pos[0] + size[0] * 0.5 - side * 0.5;
		float top = pos[1] + size[1] * 0.5 - side * 0.5;

		FrameSlot.SetAnchorMin(title, 0, 0);
		FrameSlot.SetAnchorMax(title, 0, 0);
		FrameSlot.SetPos(title, left * scale, top * scale);
		FrameSlot.SetSize(title, side * scale, side * scale);
		title.SetExactFontSize(Math.Round(font * scale));
		title.SetTextWrapping(false);
		title.SetPivot(0.5, 0.5);
		title.SetRotation(m_fTitleRotation);
		title.SetText(lines);
	}

	//------------------------------------------------------------------------------------------------
	protected void FitFaceTitle(TextWidget title, string text, float dpiScale, bool workbench)
	{
		vector pos;
		vector size;
		int font;
		if (workbench)
			font = FaceTitleRect(dpiScale, pos, size);
		else
			font = ClientTitleRect(text, pos, size);

		FrameSlot.SetAnchorMin(title, 0, 0);
		FrameSlot.SetAnchorMax(title, 0, 0);
		FrameSlot.SetPos(title, pos[0], pos[1]);
		FrameSlot.SetSize(title, size[0], size[1]);
		title.SetExactFontSize(font);
	}

	//------------------------------------------------------------------------------------------------
	protected void PrintOn(IEntity target, WorkspaceWidget workspace, LF_SignDef sign)
	{
		ResourceName layout = FACE_LAYOUT;
		if (!m_sFaceLayout.IsEmpty())
			layout = m_sFaceLayout;

		m_wFaceRoot = workspace.CreateWidgets(layout);
		if (!m_wFaceRoot)
		{
			EL_Debug.Error("Signs", "sign face layout failed to load: " + layout);
			ClearFace();
			return;
		}

		m_wFaceTexture = RTTextureWidget.Cast(m_wFaceRoot.FindAnyWidget("RTTexture0"));
		TextWidget title = TextWidget.Cast(m_wFaceRoot.FindAnyWidget("Title"));
		if (!m_wFaceTexture || !title)
		{
			EL_Debug.Error("Signs", "sign face layout is missing RTTexture0 or Title");
			ClearFace();
			return;
		}

		bool workbench = false;
		#ifdef WORKBENCH
		workbench = true;
		#endif
		string text = FaceText(sign);
		if (!m_sFaceLayout.IsEmpty())
		{
			PaintFace(title);
			FitBoardTitle(title, text, workspace.DPIScale(1), workbench);
		}
		else
		{
			FitFaceTitle(title, text, workspace.DPIScale(1), workbench);
			title.SetTextWrapping(true);
			title.SetText(text);
		}
		m_wFaceTexture.SetRenderTarget(target);
		m_FaceTarget = target;
		EL_Debug.Info("Signs", "face printed: " + m_sSignId);
	}

	//------------------------------------------------------------------------------------------------
	protected void ClearFace()
	{
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(BuildFace);

		if (m_wFaceTexture && m_FaceTarget && !m_FaceTarget.IsDeleted())
			m_wFaceTexture.RemoveRenderTarget(m_FaceTarget);

		m_wFaceTexture = null;
		m_FaceTarget = null;

		if (m_wFaceRoot)
			m_wFaceRoot.RemoveFromHierarchy();

		m_wFaceRoot = null;

		if (m_Face && !m_Face.IsDeleted())
			SCR_EntityHelper.DeleteEntityAndChildren(m_Face);

		m_Face = null;
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		ClearFace();
		super.OnDelete(owner);
	}
}
