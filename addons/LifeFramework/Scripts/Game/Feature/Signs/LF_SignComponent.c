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
	//! The prefab slot (#1128): The face a sign draws its text on.
	static const string SLOT_FACE = "sign_face";
	//! LF_Places kind every sign claims under its sign id, so a reader (the town board) finds the
	//! place a sign stands for by component, never by entity name or coordinates.
	static const string PLACE_KIND = "Sign";
	//! How often a face with live rows checks for a new reply or for a viewer in reach.
	static const int ROW_POLL_MS = 2000;	// lf-fixed: a printed face, not an animation
	//! A local row source checks its rows only while the viewer is this close (metres).
	static const float LOCAL_ROWS_RANGE = 60;	// lf-fixed: beyond this the rows cannot be read anyway

	//! The prefab (slot "sign_face"), or empty when no usable prefab resolves (logged).
	static ResourceName FacePrefab()
	{
		return LF_PrefabSlots.Get(SLOT_FACE);
	}
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
	// lf-fixed: a measured font metric of the sign face, not a balance value
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

	[Attribute("", UIWidgets.EditComboBox, "Sign id in Configs/Signs/LF_Signs.conf (the list is every id the catalog and pack overlays know). Right-click the entity, Edit sign text, to change its words")]
	protected string m_sSignId;

	[Attribute("1", UIWidgets.CheckBox, "Print the title on a paper sheet on the prop face (rendering clients only)")]
	protected bool m_bShowFace;

	[Attribute("", UIWidgets.EditComboBox, "Face style in Configs/Signs/LF_SignStyles.conf (chalk, ...). The sign's own m_sStyle wins. Empty: the legacy face below (docs/architecture/sign-system.md)")]
	protected string m_sStyle;

	[Attribute("", UIWidgets.EditComboBox, "Face size in Configs/Signs/LF_SignStyles.conf: the carrier mesh and its shown area. Needed with a style")]
	protected string m_sSize;

	[Attribute("", UIWidgets.EditBox, "Live rows on a styled face: a row source kind (market). The client asks the server when the viewer is within reach and repaints the rows. Empty: the sign's own config rows (LF_SignRowSource.c)")]
	protected string m_sRowSource;

	[Attribute("0", UIWidgets.CheckBox, "The prop mesh itself samples $rendertarget (an A4 notice with LF_SignFace.emat). No sheet is spawned")]
	protected bool m_bFaceOnOwner;

	[Attribute("0 0 -0.007", UIWidgets.EditBox, "Sheet position relative to the prop. Vanilla boards pin posters on the -Z face")]
	protected vector m_vFaceOffset;

	[Attribute("0 0 0", UIWidgets.EditBox, "Sheet rotation relative to the prop (yaw pitch roll, degrees)")]
	protected vector m_vFaceAngles;

	[Attribute("1", UIWidgets.EditBox, "Sheet scale. 1 is an A4 sheet (0.21 x 0.30 m)")]
	protected float m_fFaceScale;

	[Attribute("0", UIWidgets.CheckBox, "Styled face on an A-frame: print the same face a second time on the far side, at m_vBackFaceOffset and m_vBackFaceAngles, so both slates carry it (#3547)")]
	protected bool m_bBackFace;

	[Attribute("0 0 0", UIWidgets.EditBox, "Back face position relative to the prop (m_bBackFace)")]
	protected vector m_vBackFaceOffset;

	[Attribute("0 0 0", UIWidgets.EditBox, "Back face rotation relative to the prop (yaw pitch roll, degrees; m_bBackFace)")]
	protected vector m_vBackFaceAngles;

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

	[Attribute("", UIWidgets.EditBox, "Price board (#540): shop id of the vehicle lot whose price list this sign reads. Empty: a plain sign. The title is then a template with %1 (car name) and %2 (price)")]
	protected string m_sPriceLotShopId;

	[Attribute(ResourceName.Empty, UIWidgets.ResourcePickerThumbnail, "Price board: the lot offer's vehicle prefab (LF_VehicleOffer.m_sPrefab) whose price is shown", "et")]
	protected ResourceName m_PriceVehicle;

	[Attribute("", UIWidgets.EditComboBox, "Sale signs: the sign id the face and the Read card switch to once the house or plot this prop sells is owned (LF_HouseSaleComponent, LF_PlotComponent on the same prop). Empty: the face never changes. The m_sSignId stays the place and quest id either way")]
	protected string m_sSoldSignId;

	protected ref LF_SignDef m_Sign;
	protected bool m_bResolved;
	//! Whether m_Sign was resolved for the sold state; a change of state resolves again.
	protected bool m_bResolvedSold;

	protected ref LF_SignStyledFace m_StyledFace;
	protected ref LF_SignStyledFace m_BackFace;
	protected int m_iRowVersion;
	protected int m_iRowAskMs;
	//! A local row source: the rows last printed, so the poll repaints only when they change.
	protected string m_sLocalRows;
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
	//! A painted face (plate, fascia, pub sign, chalkboard) has an area; the paper board and notice do not.
	bool HasPaintedFace()
	{
		return m_vFaceArea[0] > 0 && m_vFaceArea[1] > 0;
	}

	//------------------------------------------------------------------------------------------------
	//! The sheet the Read card is printed on: framed and accented in this sign's colours when painted.
	LF_UISkin CardSkin()
	{
		return LF_SignCatalog.CardSkin(HasPaintedFace(), m_vRimColor, m_vPaintColor, m_vTextColor);
	}

	//------------------------------------------------------------------------------------------------
	//! The configured sign, or null. Resolved once; a miss logs once and the Read action hides.
	LF_SignDef GetSign()
	{
		bool sold = IsSold();
		if (m_bResolved && sold == m_bResolvedSold)
			return m_Sign;

		m_bResolved = true;
		m_bResolvedSold = sold;
		m_Sign = null;
		LF_SignDef sign;
		if (sold)
		{
			sign = LF_SignCatalog.Find(m_sSoldSignId);
			if (!LF_SignCatalog.IsUsable(sign))
				LF_Debug.Error("Signs", string.Format("sold sign id '%1' has no titled entry in LF_Signs.conf; the for-sale face stays on %2", m_sSoldSignId, GetOwner().GetOrigin()));
		}

		if (!LF_SignCatalog.IsUsable(sign))
			sign = LF_SignCatalog.Find(m_sSignId);

		if (!LF_SignCatalog.IsUsable(sign))
		{
			LF_Debug.Error("Signs", string.Format("sign id '%1' has no titled entry in LF_Signs.conf; Read hidden at %2", m_sSignId, GetOwner().GetOrigin()));
			return null;
		}

		if (!m_sPriceLotShopId.IsEmpty())
		{
			sign = PricedSign(sign);
			if (!sign)
				return null;
		}

		m_Sign = sign;
		return m_Sign;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the house or plot a sale component on this prop sells has an owner (the
	//! replicated flag, so a client reads it too). False for a prop with no sale component.
	static bool OwnerSold(IEntity owner)
	{
		if (!owner)
			return false;

		LF_HouseSaleComponent house = LF_Component<LF_HouseSaleComponent>.Find(owner);
		if (house)
			return house.IsOwned();

		LF_PlotComponent plot = LF_Component<LF_PlotComponent>.Find(owner);
		return plot && plot.IsOwned();
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when this sign is a sale sign (m_sSoldSignId) whose house or plot is owned: it then
	//! prints and reads as the sold sign.
	bool IsSold()
	{
		return !m_sSoldSignId.IsEmpty() && OwnerSold(GetOwner());
	}

	//------------------------------------------------------------------------------------------------
	//! The sale component's owned flag changed (the server's sale, a client's replicated copy): the face
	//! prints again with the sign it is now. Before the first print nothing is drawn, and the print that
	//! follows reads the new state.
	void OnSaleChanged()
	{
		if (m_sSoldSignId.IsEmpty())
			return;

		bool sold = IsSold();
		if (m_bResolved && sold != m_bResolvedSold)
			m_bResolved = false;

		if (!m_wFaceRoot && !m_StyledFace)
			return;

		ClearFace();
		BuildFace();
	}

	//------------------------------------------------------------------------------------------------
	//! A price board's sign: the config sign with its title filled from the lot's price list, so a
	//! changed price in the lot's config changes the board (#540). Null, with an error, when the lot
	//! or the offer is missing; the board then shows nothing rather than a stale price.
	protected LF_SignDef PricedSign(LF_SignDef sign)
	{
		LF_BoardAnswer answer = LF_PricedSignProvider.AnswerFor(m_sPriceLotShopId, m_PriceVehicle);
		if (!answer)
		{
			LF_Debug.Error("Signs", string.Format("price board '%1': no price for %2 at lot '%3'", m_sSignId, m_PriceVehicle, m_sPriceLotShopId));
			return null;
		}

		return PricedFrom(sign, answer);
	}

	//------------------------------------------------------------------------------------------------
	//! The config sign with its title filled from a priced-sign answer (the lot's name and price).
	static LF_SignDef PricedFrom(LF_SignDef sign, notnull LF_BoardAnswer answer)
	{
		return LF_SignCatalog.WithTitle(sign, LF_PricedSignProvider.Title(sign.m_sTitle, answer));
	}

	//------------------------------------------------------------------------------------------------
	//! The printed text on the sheet: the resolved title in capitals.
	static string FaceText(LF_SignDef sign)
	{
		if (!sign)
			return string.Empty;

		string text = LF_SignCatalog.ResolveTitle(sign.m_sTitle);
		text.ToUpper();
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! The style a face draws in: the sign row's, else the prop's. Empty: the legacy face.
	static string StyleIdFor(LF_SignDef sign, string propStyle)
	{
		if (sign && !sign.m_sStyle.IsEmpty())
			return sign.m_sStyle;

		return propStyle;
	}

	//------------------------------------------------------------------------------------------------
	string GetStyleId()
	{
		return StyleIdFor(GetSign(), m_sStyle);
	}

	//------------------------------------------------------------------------------------------------
	string GetSizeId()
	{
		return m_sSize;
	}

	//------------------------------------------------------------------------------------------------
	//! True when a styled face is printed on the far side too (an A-frame, #3547).
	bool HasBackFace()
	{
		return m_bBackFace;
	}

	//------------------------------------------------------------------------------------------------
	//! A styled face (docs/architecture/sign-system.md). False when the sign has no style, or its style
	//! or size does not resolve (logged): the legacy face is drawn instead.
	protected bool BuildStyledFace(IEntity owner, LF_SignDef sign)
	{
		string styleId = StyleIdFor(sign, m_sStyle);
		if (styleId.IsEmpty())
			return false;

		LF_SignStyleDef style = LF_SignStyles.FindStyle(styleId);
		LF_SignSizeDef size = LF_SignStyles.FindSize(m_sSize);
		if (!style || !size)
		{
			LF_Debug.Error("Signs", string.Format("sign %1: style '%2' or size '%3' is not in LF_SignStyles.conf; legacy face drawn", m_sSignId, styleId, m_sSize));
			return false;
		}

		m_StyledFace = new LF_SignStyledFace();
		LF_SignDef printed = LiveSign(sign);
		if (m_StyledFace.Build(owner, printed, style, size, m_vFaceOffset, m_vFaceAngles, m_fFaceScale, HasPaintedFace(), m_vPaintColor, m_vTextColor))
		{
			if (m_bBackFace)
				BuildBackFace(owner, printed, style, size);

			StartRowPoll();
			return true;
		}

		m_StyledFace.Release();
		m_StyledFace = null;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! The far slate of an A-frame: the same face again on its own carrier and render target. A failure
	//! is logged by the face and leaves the front printed.
	protected void BuildBackFace(IEntity owner, LF_SignDef printed, LF_SignStyleDef style, LF_SignSizeDef size)
	{
		m_BackFace = new LF_SignStyledFace();
		if (m_BackFace.Build(owner, printed, style, size, m_vBackFaceOffset, m_vBackFaceAngles, m_fFaceScale, HasPaintedFace(), m_vPaintColor, m_vTextColor))
			return;

		m_BackFace.Release();
		m_BackFace = null;
	}

	//------------------------------------------------------------------------------------------------
	protected void ReleaseStyledFaces()
	{
		if (m_StyledFace)
			m_StyledFace.Release();

		m_StyledFace = null;
		if (m_BackFace)
			m_BackFace.Release();

		m_BackFace = null;
	}

	//------------------------------------------------------------------------------------------------
	//! True when this sign prints rows from a live source: its Read card is the source's own card.
	bool HasRowSource()
	{
		return !m_sRowSource.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! The sign with the newest live rows, else the config sign. The title and style stay the config's.
	protected LF_SignDef LiveSign(LF_SignDef sign)
	{
		if (m_sRowSource.IsEmpty())
			return sign;

		LF_SignRowSource source = LF_SignRowSources.Find(m_sRowSource);
		if (source && source.IsLocal())
		{
			array<ref LF_SignRow> local = {};
			source.LocalRows(GetOwner(), local);
			m_sLocalRows = LF_SignRowSource.Signature(local);
			LF_SignDef printed = LF_SignCatalog.WithTitle(sign, sign.m_sTitle);
			printed.m_aRows = local;
			return printed;
		}

		m_iRowVersion = LF_SignRowSources.VersionFor(m_sRowSource);
		array<ref LF_SignRow> rows = LF_SignRowSources.RowsFor(m_sRowSource);
		if (!rows)
			return sign;

		LF_SignDef copy = LF_SignCatalog.WithTitle(sign, sign.m_sTitle);
		copy.m_aRows = rows;
		return copy;
	}

	//------------------------------------------------------------------------------------------------
	protected void StartRowPoll()
	{
		if (m_sRowSource.IsEmpty() || !GetGame() || !GetGame().GetCallqueue())
			return;

		if (!LF_SignRowSources.Find(m_sRowSource))
		{
			LF_Debug.Error("Signs", string.Format("sign %1: row source '%2' is not registered; the face prints its config rows", m_sSignId, m_sRowSource));
			return;
		}

		GetGame().GetCallqueue().Remove(PollRows);
		GetGame().GetCallqueue().CallLater(PollRows, ROW_POLL_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	//! Every couple of seconds: repaint when a newer reply is held; else ask the server once when the
	//! viewer is within reach (and again after the refresh window).
	protected void PollRows()
	{
		IEntity owner = GetOwner();
		if (!owner || owner.IsDeleted() || !m_StyledFace)
			return;

		LF_SignRowSource source = LF_SignRowSources.Find(m_sRowSource);
		if (!source)
			return;

		if (source.IsLocal())
		{
			PollLocalRows(owner, source);
			return;
		}

		if (LF_SignRowSources.VersionFor(m_sRowSource) != m_iRowVersion)
		{
			Repaint(owner);
			return;
		}

		IEntity viewer = LF_Utils.GetLocalCharacter();
		if (!viewer || !LF_SignRowSources.InAskRange(vector.Distance(viewer.GetOrigin(), owner.GetOrigin()), LF_PoliceConfig.BoardReadDistance()))
			return;

		int now = System.GetTickCount();
		if (!LF_SignRowSources.ShouldAsk(m_iRowAskMs, now))
			return;

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller)
			return;

		m_iRowAskMs = now;
		controller.LF_AskBoard(source.GetBoardKind(), true);
	}

	//------------------------------------------------------------------------------------------------
	//! A local source: within sight, work the rows out again and repaint when they changed (a stop that
	//! registered after this face was printed). Nothing is sent.
	protected void PollLocalRows(IEntity owner, LF_SignRowSource source)
	{
		IEntity viewer = LF_Utils.GetLocalCharacter();
		if (!viewer || vector.Distance(viewer.GetOrigin(), owner.GetOrigin()) > LOCAL_ROWS_RANGE)
			return;

		array<ref LF_SignRow> rows = {};
		source.LocalRows(owner, rows);
		if (LF_SignRowSource.Signature(rows) != m_sLocalRows)
			Repaint(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! Print the face again with the newest rows.
	protected void Repaint(IEntity owner)
	{
		LF_SignDef sign = GetSign();
		if (!sign)
			return;

		ReleaseStyledFaces();
		BuildStyledFace(owner, sign);
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
		// The first prop of an id stands for the place; a second (two safe zone signs) is skipped.
		if (!m_sSignId.IsEmpty())
			LF_Places.Claim(PLACE_KIND, m_sSignId, owner);

		// An empty id still reaches EOnInit: BuildFace prints a blank painted face for it (BuildBlankFace),
		// so a mesh that samples $rendertarget never stays black.
		if (!m_bShowFace)
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
		if (!owner || owner.IsDeleted() || m_wFaceRoot || m_StyledFace)
			return;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		LF_SignDef sign = GetSign();
		if (!sign)
		{
			BuildBlankFace(owner, workspace);
			return;
		}

		if (BuildStyledFace(owner, sign))
			return;

		if (m_bFaceOnChild)
		{
			IEntity plate = owner.GetChildren();
			if (!plate)
			{
				LF_Debug.Error("Signs", "sign " + m_sSignId + " paints its child but has none");
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

		Resource resource = Resource.Load(FacePrefab());
		if (!resource || !resource.IsValid())
		{
			LF_Debug.Error("Signs", "sign face prefab does not load: " + FacePrefab());
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
			LF_Debug.Error("Signs", "sign face failed to spawn for " + m_sSignId);
			return;
		}

		PrintOn(m_Face, workspace, sign);
	}

	//------------------------------------------------------------------------------------------------
	//! A sign whose id is empty or has no titled entry (GetSign logged it) still prints its painted
	//! board, with no text. A prefab whose own mesh (the owner or its plate child) samples $rendertarget
	//! draws solid black until something paints that target, so the unprinted sign was a black slab
	//! (the bakery and butcher fascias and the taxi plate had no id, 2026-11-22).
	protected void BuildBlankFace(IEntity owner, WorkspaceWidget workspace)
	{
		if (m_bFaceOnChild)
		{
			IEntity plate = owner.GetChildren();
			if (plate)
				PrintOn(plate, workspace, null);

			return;
		}

		if (m_bFaceOnOwner)
			PrintOn(owner, workspace, null);
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
	//! \return Font size: 96, or less so the longest word fits the shown width.
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
			LF_Debug.Error("Signs", "painted face layout is missing Rim or Paint: " + m_sSignId);
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
			LF_Debug.Error("Signs", "sign face layout failed to load: " + layout);
			ClearFace();
			return;
		}

		m_wFaceTexture = RTTextureWidget.Cast(m_wFaceRoot.FindAnyWidget("RTTexture0"));
		TextWidget title = TextWidget.Cast(m_wFaceRoot.FindAnyWidget("Title"));
		if (!m_wFaceTexture || !title)
		{
			LF_Debug.Error("Signs", "sign face layout is missing RTTexture0 or Title");
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
		LF_Debug.Info("Signs", "face printed: " + m_sSignId);
	}

	//------------------------------------------------------------------------------------------------
	protected void ClearFace()
	{
		if (GetGame() && GetGame().GetCallqueue())
		{
			GetGame().GetCallqueue().Remove(BuildFace);
			GetGame().GetCallqueue().Remove(PollRows);
		}

		ReleaseStyledFaces();

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

#ifdef WORKBENCH
	// World Editor preview (docs/tooling/sign-editing.md). The game prints the face in play mode only,
	// so in edit mode a sign whose mesh samples $rendertarget shows nothing. BuildFace does run in
	// edit mode (lf-cloud 2026-11-16: "face printed" for every sign), but the World Editor viewport
	// never draws the render target, so the editor writes the sign's text as world-space debug text
	// on the face instead: the title, or the whole card while the sign is selected. The text comes
	// from the same catalog and the same FaceText as the game. None of this is compiled into the game.
	protected static const int WB_MENU_EDIT = 1;
	protected static const int WB_MENU_RELOAD = 2;
	protected static int s_iWbGeneration;
	protected int m_iWbGeneration = -1;

	//------------------------------------------------------------------------------------------------
	override int _WB_GetAfterWorldUpdateSpecs(IEntity owner, IEntitySource src)
	{
		return EEntityFrameUpdateSpecs.CALL_WHEN_ENTITY_VISIBLE;
	}

	//------------------------------------------------------------------------------------------------
	override void _WB_AfterWorldUpdate(IEntity owner, float timeSlice)
	{
		if (!owner || !GetGame() || GetGame().InPlayMode())
			return;

		if (m_iWbGeneration != s_iWbGeneration)
		{
			m_iWbGeneration = s_iWbGeneration;
			m_Sign = null;
			m_bResolved = false;
		}

		if (m_sSignId.IsEmpty())
			return;

		WbDrawText(owner, WbIsSelected(owner));
	}

	//------------------------------------------------------------------------------------------------
	protected bool WbIsSelected(IEntity owner)
	{
		GenericEntity entity = GenericEntity.Cast(owner);
		if (!entity)
			return false;

		WorldEditorAPI api = entity._WB_GetEditorAPI();
		if (!api)
			return false;

		IEntitySource src = api.EntityToSource(owner);
		return src && api.IsEntitySelected(src);
	}

	//------------------------------------------------------------------------------------------------
	//! The title on the sign's face, or (selected) the whole card as plain lines. A sign whose id
	//! does not resolve says so in red, where the game would only log it.
	protected void WbDrawText(IEntity owner, bool full)
	{
		LF_SignDef sign;
		if (!m_sSignId.IsEmpty())
			sign = GetSign();

		string text;
		int color = ARGBF(1, m_vTextColor[0], m_vTextColor[1], m_vTextColor[2]);
		int background = ARGBF(0.85, m_vPaintColor[0], m_vPaintColor[1], m_vPaintColor[2]);
		if (!HasPaintedFace())
		{
			color = ARGBF(1, 0.08, 0.08, 0.08);
			background = ARGBF(0.85, 0.95, 0.93, 0.86);
		}

		if (!sign)
		{
			text = "sign id '" + m_sSignId + "' has no titled entry in LF_Signs.conf";
			color = ARGBF(1, 1, 1, 1);
			background = ARGBF(0.85, 0.7, 0.05, 0.05);
		}
		else if (full)
		{
			text = m_sSignId + "\n" + LF_SignCatalog.PlainText(sign);
			color = ARGBF(1, 1, 1, 1);
			background = ARGBF(0.8, 0.05, 0.05, 0.05);
		}
		else
		{
			text = FaceText(sign);
		}

		// The text goes on the entity that carries the face: the plate, for a plate on a pole.
		IEntity face = owner;
		if (m_bFaceOnChild && owner.GetChildren())
			face = owner.GetChildren();

		vector mins;
		vector maxs;
		face.GetBounds(mins, maxs);
		vector mat[4];
		face.GetWorldTransform(mat);

		// A painted face's mesh samples $rendertarget, which the edit view never fills, so it draws
		// black. The face area's aspect is the board's (115 x 67 plate: 1024 x 596), so the face is
		// that tall from the top of the bounds; the legs of a board on posts are not face.
		bool panel = HasPaintedFace() && (m_bFaceOnOwner || m_bFaceOnChild);
		if (panel)
		{
			float faceHeight = (maxs[0] - mins[0]) * m_vFaceArea[1] / m_vFaceArea[0];
			if (faceHeight < maxs[1] - mins[1])
				mins[1] = maxs[1] - faceHeight;
		}

		vector world[4];
		world[0] = mat[0];
		world[1] = mat[1];
		world[2] = mat[2];
		world[3] = WbToWorld(mat, Vector((mins[0] + maxs[0]) * 0.5, (mins[1] + maxs[1]) * 0.5, maxs[2] + 0.02));

		vector camera[4];
		owner.GetWorld().GetCurrentCamera(camera);
		float distance = vector.Distance(camera[3], world[3]);

		float scaleX = mat[0].Length();
		float scaleY = mat[1].Length();
		if (scaleX <= 0)
			scaleX = 1;
		if (scaleY <= 0)
			scaleY = 1;

		float width = (maxs[0] - mins[0]) * scaleX;
		float size = 0.08;
		if (!full)
			size = Math.Clamp(width * 1.3 / Math.Max(text.Length(), 6), 0.04, 0.35);

		// Shapes draw after debug text whatever their flags (lf-cloud 2026-11-16), so the panel leaves
		// a hole a little smaller than the title's paint-coloured background instead of covering it.
		// A selected sign shows its whole card over the bare face.
		if (panel && !full && sign)
		{
			float holeX = Math.Min(text.Length() * size * 0.33 / scaleX, (maxs[0] - mins[0]) * 0.45);
			float holeY = Math.Min(size * 0.4 / scaleY, (maxs[1] - mins[1]) * 0.45);
			WbDrawPanel(mat, mins, maxs, holeX, holeY);
		}

		if (distance > 0 && size / distance < 0.004)
			return;

		int flags = DebugTextFlags.CENTER | DebugTextFlags.ONCE | DebugTextFlags.FACE_CAMERA;
		DebugTextWorldSpace.CreateInWorld(owner.GetWorld(), text, flags, world, size, color, background);
	}

	//------------------------------------------------------------------------------------------------
	//! A point in the entity's local space, in world space (mat: the entity's world transform).
	protected static vector WbToWorld(vector mat[4], vector local)
	{
		return mat[0] * local[0] + mat[1] * local[1] + mat[2] * local[2] + mat[3];
	}

	//------------------------------------------------------------------------------------------------
	//! The face's front (its local +Z side) in the sign's colours, for this editor frame only: a rim
	//! ring m_fRimWidth wide (layout units over the face area's width), then paint inside it around
	//! a hole of half-size holeX x holeY at the centre where the title sits. No two quads overlap, so
	//! draw order does not matter.
	protected void WbDrawPanel(vector mat[4], vector mins, vector maxs, float holeX, float holeY)
	{
		float rim = 0;
		if (m_vFaceArea[0] > 0)
			rim = (maxs[0] - mins[0]) * m_fRimWidth / m_vFaceArea[0];

		float z = maxs[2] + 0.004;
		float midX = (mins[0] + maxs[0]) * 0.5;
		float midY = (mins[1] + maxs[1]) * 0.5;
		int rimColor = ARGBF(1, m_vRimColor[0], m_vRimColor[1], m_vRimColor[2]);
		int paintColor = ARGBF(1, m_vPaintColor[0], m_vPaintColor[1], m_vPaintColor[2]);
		WbDrawFrame(mat, mins[0], mins[1], maxs[0], maxs[1], mins[0] + rim, mins[1] + rim, maxs[0] - rim, maxs[1] - rim, z, rimColor);
		WbDrawFrame(mat, mins[0] + rim, mins[1] + rim, maxs[0] - rim, maxs[1] - rim, midX - holeX, midY - holeY, midX + holeX, midY + holeY, z, paintColor);
	}

	//------------------------------------------------------------------------------------------------
	//! The ring between an outer and an inner rectangle, as four quads.
	protected void WbDrawFrame(vector mat[4], float l, float b, float r, float t, float il, float ib, float ir, float it, float z, int color)
	{
		WbDrawQuad(mat, l, b, r, ib, z, color);
		WbDrawQuad(mat, l, it, r, t, z, color);
		WbDrawQuad(mat, l, ib, il, it, z, color);
		WbDrawQuad(mat, ir, ib, r, it, z, color);
	}

	//------------------------------------------------------------------------------------------------
	protected void WbDrawQuad(vector mat[4], float left, float bottom, float right, float top, float z, int color)
	{
		vector a = WbToWorld(mat, Vector(left, bottom, z));
		vector b = WbToWorld(mat, Vector(right, bottom, z));
		vector c = WbToWorld(mat, Vector(right, top, z));
		vector d = WbToWorld(mat, Vector(left, top, z));
		vector tris[6];
		tris[0] = a;
		tris[1] = b;
		tris[2] = c;
		tris[3] = a;
		tris[4] = c;
		tris[5] = d;
		Shape.CreateTris(color, ShapeFlags.ONCE | ShapeFlags.NOOUTLINE | ShapeFlags.DOUBLESIDE, tris, 2);
	}

	//------------------------------------------------------------------------------------------------
	//! A changed attribute reprints the face with the new value (the editor passes the component source).
	override bool _WB_OnKeyChanged(IEntity owner, BaseContainer src, string key, BaseContainerList ownerContainers, IEntity parent)
	{
		if (src)
			WbReadAttributes(src);

		m_iWbGeneration = -1;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void WbReadAttributes(notnull BaseContainer src)
	{
		src.Get("m_sSignId", m_sSignId);
		src.Get("m_bShowFace", m_bShowFace);
		src.Get("m_sStyle", m_sStyle);
		src.Get("m_sSize", m_sSize);
		src.Get("m_sRowSource", m_sRowSource);
		src.Get("m_bFaceOnOwner", m_bFaceOnOwner);
		src.Get("m_bFaceOnChild", m_bFaceOnChild);
		src.Get("m_vFaceOffset", m_vFaceOffset);
		src.Get("m_vFaceAngles", m_vFaceAngles);
		src.Get("m_fFaceScale", m_fFaceScale);
		src.Get("m_bBackFace", m_bBackFace);
		src.Get("m_vBackFaceOffset", m_vBackFaceOffset);
		src.Get("m_vBackFaceAngles", m_vBackFaceAngles);
		src.Get("m_sFaceLayout", m_sFaceLayout);
		src.Get("m_fTitleRotation", m_fTitleRotation);
		src.Get("m_vTitlePos", m_vTitlePos);
		src.Get("m_vTitleSize", m_vTitleSize);
		src.Get("m_iTitleFontMax", m_iTitleFontMax);
		src.Get("m_vFaceArea", m_vFaceArea);
		src.Get("m_fRimWidth", m_fRimWidth);
		src.Get("m_vRimColor", m_vRimColor);
		src.Get("m_vPaintColor", m_vPaintColor);
		src.Get("m_vTextColor", m_vTextColor);
	}

	//------------------------------------------------------------------------------------------------
	//! The sign id picker: every id the catalog and its pack overlays know.
	override array<ref ParamEnum> _WB_GetUserEnums(string varName, IEntity owner, IEntityComponentSource src)
	{
		array<string> ids = {};
		if (varName == "m_sSignId")
			LF_SignCatalog.WbIds(ids);
		else if (varName == "m_sStyle")
			LF_SignStyles.StyleIds(ids);
		else if (varName == "m_sSize")
			LF_SignStyles.SizeIds(ids);
		else
			return null;

		ids.Sort();
		array<ref ParamEnum> result = {};
		foreach (string id : ids)
		{
			result.Insert(new ParamEnum(id, id, ""));
		}

		return result;
	}

	//------------------------------------------------------------------------------------------------
	override array<ref WB_UIMenuItem> _WB_GetContextMenuItems(IEntity owner)
	{
		array<ref WB_UIMenuItem> items = {};
		items.Insert(new WB_UIMenuItem("Edit sign text (LF_Signs.conf)", WB_MENU_EDIT));
		items.Insert(new WB_UIMenuItem("Reload sign text", WB_MENU_RELOAD));
		return items;
	}

	//------------------------------------------------------------------------------------------------
	//! Edit opens the sign config in its editor (the entry is the row whose m_sId is this sign's id);
	//! Reload reads the config and pack overlays again and reprints every sign in the world.
	override void _WB_OnContextMenu(IEntity owner, int id)
	{
		if (id == WB_MENU_EDIT)
		{
			string path = LF_SignCatalog.CONFIG_PATH.GetPath();
			LF_Debug.Info("Signs", string.Format("edit sign '%1': the m_sId '%1' row in %2; a #key title is a string row (tools\\cli regen-localization after)", m_sSignId, path));
			if (!Workbench.OpenResource(path))
				LF_Debug.Error("Signs", "could not open " + path);

			return;
		}

		if (id == WB_MENU_RELOAD)
		{
			LF_SignCatalog.WbReload();
			LF_SignStyles.ResetStatics();
			s_iWbGeneration++;
			LF_Debug.Info("Signs", "sign text reloaded");
		}
	}
#endif

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		ClearFace();
		super.OnDelete(owner);
	}
}

//------------------------------------------------------------------------------------------------
//! The priced sign as a board kind (#1946, chapter #1014): the read of a vehicle lot's price list
//! goes through the board provider, so the record board and stall boards can follow. The answer is
//! built per read and not cached, so a changed price in the lot's config is a changed board. The
//! face is painted on each client from the lot's own config, so nothing crosses the network and the
//! kind names no read action: LF_BoardProviders.ServerRead refuses a client that asks for it.
class LF_PricedSignProvider : LF_BoardProvider
{
	static const string KIND = "priced";

	override string GetKind()
	{
		return KIND;
	}

	//------------------------------------------------------------------------------------------------
	//! The answer for one sign: the offer's name key, then its price. Null, with an error, when the lot
	//! or the offer is missing (the sign adds which board it was), so the board shows nothing
	//! rather than a stale price.
	static LF_BoardAnswer AnswerFor(string lotShopId, ResourceName vehicle)
	{
		LF_VehicleLotComponent lot = LF_VehicleLotComponent.FindLot(lotShopId);
		if (!lot)
		{
			LF_Debug.Error("Signs", string.Format("price board finds no vehicle lot '%1'", lotShopId));
			return null;
		}

		LF_VehicleOffer offer = lot.GetOffer(vehicle);
		if (!offer)
		{
			LF_Debug.Error("Signs", string.Format("price board: lot '%1' has no offer for %2", lotShopId, vehicle));
			return null;
		}

		LF_BoardAnswer answer = new LF_BoardAnswer();
		answer.m_aTexts.Insert(offer.GetNameKey());
		answer.m_aNumbers.Insert(offer.GetPrice());
		return answer;
	}

	//------------------------------------------------------------------------------------------------
	//! The sign's title: its title format filled from an answer (%1 the car name, %2 the grouped price).
	static string Title(string format, notnull LF_BoardAnswer answer)
	{
		if (answer.m_aTexts.IsEmpty() || answer.m_aNumbers.IsEmpty())
			return format;

		return LF_SignCatalog.PricedTitle(format, answer.m_aTexts[0], answer.m_aNumbers[0]);
	}

	//------------------------------------------------------------------------------------------------
	override array<ref LF_UIPart> Paint(notnull LF_BoardAnswer answer)
	{
		array<ref LF_UIPart> parts = {};
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "Price", Title("%1 %2", answer)));
		return parts;
	}
}

//------------------------------------------------------------------------------------------------
modded class LF_BoardProviderSet
{
	override void Collect(notnull array<ref LF_BoardProvider> providers)
	{
		super.Collect(providers);
		providers.Insert(new LF_PricedSignProvider());
	}
}
