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
	protected void FitFaceTitle(TextWidget title, float dpiScale)
	{
		vector pos;
		vector size;
		int font = FaceTitleRect(dpiScale, pos, size);
		FrameSlot.SetAnchorMin(title, 0, 0);
		FrameSlot.SetAnchorMax(title, 0, 0);
		FrameSlot.SetPos(title, pos[0], pos[1]);
		FrameSlot.SetSize(title, size[0], size[1]);
		title.SetExactFontSize(font);
	}

	//------------------------------------------------------------------------------------------------
	protected void PrintOn(IEntity target, WorkspaceWidget workspace, LF_SignDef sign)
	{
		m_wFaceRoot = workspace.CreateWidgets(FACE_LAYOUT);
		if (!m_wFaceRoot)
		{
			EL_Debug.Error("Signs", "sign face layout failed to load: " + FACE_LAYOUT);
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

		FitFaceTitle(title, workspace.DPIScale(1));
		title.SetTextWrapping(true);
		title.SetText(FaceText(sign));
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
