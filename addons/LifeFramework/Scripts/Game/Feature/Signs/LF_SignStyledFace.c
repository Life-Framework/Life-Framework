// LF_SignStyledFace.c: paints one sign in its style on its size's carrier (docs/architecture/sign-system.md).
//
// Client only. The carrier is a local child (a one-mesh quad whose material samples $rendertarget) or,
// with no carrier on the size, the prop's own mesh. The style layout is filled from LF_SignFacePlan and
// bound with SetRenderTarget; a sign does not move, so the target draws one frame a second.

class LF_SignStyledFace
{
	static const int MAX_FPS = 1;	// lf-fixed: a printed face never moves

	protected IEntity m_Carrier;
	protected bool m_bOwnCarrier;
	protected Widget m_wRoot;
	protected RTTextureWidget m_wTexture;
	protected ref array<Widget> m_aRowWidgets = {};

	//------------------------------------------------------------------------------------------------
	//! The sign's rows as resolved text, in order.
	static void ResolveRows(LF_SignDef sign, notnull array<string> labels, notnull array<string> values)
	{
		if (!sign || !sign.m_aRows)
			return;

		foreach (LF_SignRow row : sign.m_aRows)
		{
			if (!row)
				continue;

			labels.Insert(LF_SignCatalog.Resolve(row.m_sLabel));
			values.Insert(LF_SignCatalog.Resolve(row.m_sValue));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Spawn the carrier and print the sign. False, logged, when a resource is missing; the caller
	//! then draws nothing (never a half-built face).
	bool Build(notnull IEntity owner, notnull LF_SignDef sign, notnull LF_SignStyleDef style, notnull LF_SignSizeDef size, vector offset, vector angles, float scale, bool painted = false, vector paint = vector.Zero, vector text = vector.Zero)
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return false;

		if (!SpawnCarrier(owner, size, offset, angles, scale))
			return false;

		m_wRoot = workspace.CreateWidgets(style.m_sLayout);
		if (!m_wRoot)
		{
			LF_Debug.Error("Signs", string.Format("sign %1: style %2 layout does not load: %3", sign.m_sId, style.m_sId, style.m_sLayout));
			Release();
			return false;
		}

		m_wTexture = RTTextureWidget.Cast(m_wRoot.FindAnyWidget("RTTexture0"));
		// A carrier quad samples its whole render target (UV 0..1), so the target takes the size row's shape: the
		// layout authors a 1920 x 1080 one, and a portrait quad on it showed only the strip the style paints (#3547).
		if (m_wTexture && m_bOwnCarrier && size.m_vTarget[0] > 0 && size.m_vTarget[1] > 0)
			FrameSlot.SetSize(m_wTexture, size.m_vTarget[0], size.m_vTarget[1]);

		TextWidget title = TextWidget.Cast(m_wRoot.FindAnyWidget("Title"));
		ImageWidget ground = ImageWidget.Cast(m_wRoot.FindAnyWidget("Ground"));
		Widget rows = m_wRoot.FindAnyWidget("Rows");
		if (!m_wTexture || !title || !ground || !rows)
		{
			LF_Debug.Error("Signs", string.Format("sign %1: style %2 layout lacks RTTexture0, Ground, Title or Rows", sign.m_sId, style.m_sId));
			Release();
			return false;
		}

		array<string> labels = {};
		array<string> values = {};
		ResolveRows(sign, labels, values);
		LF_SignFacePlan plan = LF_SignFacePlan.Compose(size.m_vShown, style, LF_SignCatalog.ResolveTitle(sign.m_sTitle), labels, values);
		if (plan.m_iDropped > 0)
			LF_Debug.Warn("Signs", string.Format("sign %1: %2 rows over the %3 a face holds are left off", sign.m_sId, plan.m_iDropped, LF_SignFacePlan.MaxRowsOf(style)));

		vector groundColor;
		vector inkColor;
		FaceColors(style, painted, paint, text, groundColor, inkColor);
		PaintGround(ground, style, size, groundColor);
		PaintFinish(style, size);
		Color ink = new Color(inkColor[0], inkColor[1], inkColor[2], style.m_fInkAlpha);
		PlaceText(title, plan.m_sTitle, plan.m_vTitlePos, plan.m_vTitleSize, plan.m_iTitleFont, plan.m_fTitleTilt, ink);

		TextWidget ghost = TextWidget.Cast(m_wRoot.FindAnyWidget("Ghost"));
		if (ghost && plan.m_bGhost)
		{
			vector shadow = ShadowColor(style, inkColor);
			Color faint = new Color(shadow[0], shadow[1], shadow[2], style.m_fGhostAlpha);
			PlaceText(ghost, plan.m_sTitle, plan.m_vGhostPos, plan.m_vTitleSize, plan.m_iTitleFont, plan.m_fTitleTilt * -1.5, faint);
		}
		else if (ghost)
		{
			ghost.SetVisible(false);
		}

		foreach (LF_SignFaceRowPlan row : plan.m_aRows)
		{
			if (!PaintRow(workspace, rows, style, row, ink))
			{
				LF_Debug.Error("Signs", string.Format("sign %1: style %2 row layout does not load or lacks Label, Leader, Value: %3", sign.m_sId, style.m_sId, style.m_sRowLayout));
				break;
			}
		}

		m_wTexture.SetMaxFPS(MAX_FPS);
		m_wTexture.SetRenderTarget(m_Carrier);
		LF_Debug.Info("Signs", string.Format("styled face printed: %1 style=%2 size=%3 rows=%4", sign.m_sId, style.m_sId, size.m_sId, plan.m_aRows.Count()));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Ground and ink: the style's, or the placed prop's paint and lettering colours when the style
	//! takes placement colours and the prop has a painted face (a shop fascia in the shop's colours).
	static void FaceColors(notnull LF_SignStyleDef style, bool painted, vector paint, vector text, out vector ground, out vector ink)
	{
		ground = style.m_vGround;
		ink = style.m_vInk;
		if (!style.m_bPlacementColors || !painted)
			return;

		ground = paint;
		ink = text;
	}

	//------------------------------------------------------------------------------------------------
	//! The ghost or shadow colour: the style's own, or the ink when it sets none (all zero).
	static vector ShadowColor(notnull LF_SignStyleDef style, vector ink)
	{
		if (style.m_vGhostColor == vector.Zero)
			return ink;

		return style.m_vGhostColor;
	}

	//------------------------------------------------------------------------------------------------
	protected bool SpawnCarrier(IEntity owner, LF_SignSizeDef size, vector offset, vector angles, float scale)
	{
		if (size.m_sCarrier.IsEmpty())
		{
			m_Carrier = owner;
			m_bOwnCarrier = false;
			return true;
		}

		Resource resource = Resource.Load(size.m_sCarrier);
		if (!resource || !resource.IsValid())
		{
			LF_Debug.Error("Signs", string.Format("size %1: carrier prefab does not load: %2", size.m_sId, size.m_sCarrier));
			return false;
		}

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.LOCAL;
		params.Parent = owner;
		Math3D.AnglesToMatrix(angles, params.Transform);
		params.Transform[3] = offset;
		params.Scale = scale;
		m_Carrier = GetGame().SpawnEntityPrefabLocal(resource, owner.GetWorld(), params);
		m_bOwnCarrier = true;
		if (!m_Carrier)
		{
			LF_Debug.Error("Signs", "size " + size.m_sId + ": carrier failed to spawn");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The ground fills the shown area in the style colour, under the style texture when it has one.
	protected void PaintGround(ImageWidget ground, LF_SignStyleDef style, LF_SignSizeDef size, vector color)
	{
		FrameSlot.SetAnchorMin(ground, 0, 0);
		FrameSlot.SetAnchorMax(ground, 0, 0);
		FrameSlot.SetPos(ground, 0, 0);
		FrameSlot.SetSize(ground, size.m_vShown[0], size.m_vShown[1]);
		ground.SetColor(Color.FromVector(color));
		if (!style.m_sGroundTexture.IsEmpty() && !ground.LoadImageTexture(0, style.m_sGroundTexture, true))
			LF_Debug.Warn("Signs", "style " + style.m_sId + ": ground texture does not load, the ground is plain: " + style.m_sGroundTexture);
	}

	//------------------------------------------------------------------------------------------------
	//! The style's finish (worn edges, tape): the layout's Finish image over the shown area. A style with
	//! no finish hides the widget; a layout without one, or an imageset that does not load, logs and
	//! prints the face without wear.
	protected void PaintFinish(LF_SignStyleDef style, LF_SignSizeDef size)
	{
		ImageWidget finish = ImageWidget.Cast(m_wRoot.FindAnyWidget("Finish"));
		if (style.m_sFinishImage.IsEmpty())
		{
			if (finish)
				finish.SetVisible(false);

			return;
		}

		if (!finish)
		{
			LF_Debug.Warn("Signs", "style " + style.m_sId + ": its layout has no Finish image widget, printed without wear");
			return;
		}

		FrameSlot.SetAnchorMin(finish, 0, 0);
		FrameSlot.SetAnchorMax(finish, 0, 0);
		FrameSlot.SetPos(finish, 0, 0);
		FrameSlot.SetSize(finish, size.m_vShown[0], size.m_vShown[1]);
		string name = style.m_sFinishName;
		if (name.IsEmpty())
			name = "full";

		if (!finish.LoadImageFromSet(0, style.m_sFinishImage, name))
		{
			LF_Debug.Warn("Signs", string.Format("style %1: finish image '%2' does not load from %3, printed without wear", style.m_sId, name, style.m_sFinishImage));
			finish.SetVisible(false);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void PlaceText(TextWidget text, string value, vector pos, vector size, int font, float tilt, Color color)
	{
		FrameSlot.SetAnchorMin(text, 0, 0);
		FrameSlot.SetAnchorMax(text, 0, 0);
		FrameSlot.SetPos(text, pos[0], pos[1]);
		FrameSlot.SetSize(text, size[0], size[1]);
		text.SetExactFontSize(font);
		text.SetTextWrapping(false);
		text.SetColor(color);
		if (tilt != 0)
		{
			text.SetPivot(0.5, 0.5);
			text.SetRotation(tilt);
		}

		text.SetText(value);
	}

	//------------------------------------------------------------------------------------------------
	protected bool PaintRow(WorkspaceWidget workspace, Widget rows, LF_SignStyleDef style, LF_SignFaceRowPlan row, Color ink)
	{
		Widget widget = workspace.CreateWidgets(style.m_sRowLayout, rows);
		if (!widget)
			return false;

		m_aRowWidgets.Insert(widget);
		TextWidget label = TextWidget.Cast(widget.FindAnyWidget("Label"));
		TextWidget leader = TextWidget.Cast(widget.FindAnyWidget("Leader"));
		TextWidget value = TextWidget.Cast(widget.FindAnyWidget("Value"));
		if (!label || !leader || !value)
			return false;

		FrameSlot.SetAnchorMin(widget, 0, 0);
		FrameSlot.SetAnchorMax(widget, 0, 0);
		FrameSlot.SetPos(widget, row.m_vPos[0], row.m_vPos[1]);
		FrameSlot.SetSize(widget, row.m_vSize[0], row.m_vSize[1]);

		PlaceText(label, row.m_sLabel, vector.Zero, row.m_vSize, row.m_iFont, 0, ink);
		PlaceText(value, row.m_sValue, vector.Zero, row.m_vSize, row.m_iFont, 0, ink);
		PlaceText(leader, row.m_sLeader, Vector(row.m_fLeaderLeft, 0, 0), Vector(row.m_vSize[0] - row.m_fLeaderLeft, row.m_vSize[1], 0), row.m_iFont, 0, ink);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	void Release()
	{
		if (m_wTexture && m_Carrier && !m_Carrier.IsDeleted())
			m_wTexture.RemoveRenderTarget(m_Carrier);

		m_wTexture = null;
		m_aRowWidgets.Clear();
		if (m_wRoot)
			m_wRoot.RemoveFromHierarchy();

		m_wRoot = null;
		if (m_bOwnCarrier && m_Carrier && !m_Carrier.IsDeleted())
			SCR_EntityHelper.DeleteEntityAndChildren(m_Carrier);

		m_Carrier = null;
		m_bOwnCarrier = false;
	}
}
