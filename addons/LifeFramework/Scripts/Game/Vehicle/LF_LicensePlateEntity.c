[EntityEditorProps(category: "EL/LicensePlateEntity", description:"The License Plate Entity")]
class LF_LicensePlateEntityClass : GenericEntityClass
{
};

//! One plate on a vehicle. The plate mesh samples `$rendertarget` (LicensePlateMaterial.emat on the PIPMaterial
//! slot); on a client this entity builds LicensePlate.layout once and feeds the layout's RTTexture0 to the mesh,
//! so the manager's `m_TextWidget.SetText(registration)` shows on the plate. A dedicated server has no renderer
//! and builds nothing: `m_TextWidget` stays null there (the manager keeps the text per plate instead).
class LF_LicensePlateEntity : GenericEntity
{
	[Attribute("{A4CADC2CF8CBAB17}UI/Layouts/LicensePlate/LicensePlate.layout", UIWidgets.ResourcePickerThumbnail, "The render-target layout the plate text is drawn in", params: "layout")]
	protected ResourceName m_Layout;

	protected ref Widget m_wRoot;
	protected RTTextureWidget m_wRenderTargetTextureWidget;

	ref TextWidget m_TextWidget;
	LF_LicensePlateManagerComponent m_LicensePlateManager;

	//------------------------------------------------------------------------------------------------
	void LF_LicensePlateEntity(IEntitySource src, IEntity parent)
	{
		// The World Editor also constructs this entity; only a running game draws plates.
		if (!GetGame() || !GetGame().InPlayMode())
			return;

		// The mesh object exists once the entity has initialised: bind the render target then.
		SetEventMask(EntityEvent.INIT);
	}

	//------------------------------------------------------------------------------------------------
	void ~LF_LicensePlateEntity()
	{
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(BuildText);

		m_TextWidget = null;
		m_wRenderTargetTextureWidget = null;
		if (m_wRoot)
		{
			m_wRoot.RemoveFromHierarchy();
			m_wRoot = null;
		}
	}

	//------------------------------------------------------------------------------------------------
	ResourceName GetLayout()
	{
		return m_Layout;
	}

	//------------------------------------------------------------------------------------------------
	protected override void EOnInit(IEntity owner)
	{
		ClearEventMask(EntityEvent.INIT);
		if (RplSession.Mode() == RplMode.Dedicated)
			return;

		// After the vehicle finishes spawning, as the sign faces do (LF_SignComponent.EOnInit).
		if (GetGame().GetCallqueue())
			GetGame().GetCallqueue().CallLater(BuildText, 0, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Create the layout widget once and bind it to this plate's mesh. Client only.
	protected void BuildText()
	{
		if (m_wRoot || IsDeleted())
			return;

		if (RplSession.Mode() == RplMode.Dedicated)
			return;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		m_wRoot = workspace.CreateWidgets(m_Layout);
		if (!m_wRoot)
		{
			LF_Debug.Error("LicensePlate", "plate layout failed to load: " + m_Layout);
			return;
		}

		m_wRenderTargetTextureWidget = RTTextureWidget.Cast(m_wRoot.FindAnyWidget("RTTexture0"));
		m_TextWidget = TextWidget.Cast(m_wRoot.FindAnyWidget("Text0"));
		if (!m_wRenderTargetTextureWidget || !m_TextWidget)
		{
			LF_Debug.Error("LicensePlate", "plate entity or text widget missing: layout has no RTTexture0 or Text0");
			m_TextWidget = null;
			m_wRenderTargetTextureWidget = null;
			m_wRoot.RemoveFromHierarchy();
			m_wRoot = null;
			return;
		}

		m_wRenderTargetTextureWidget.SetRenderTarget(this);

		// The manager applied the registration before this plate initialised, when it was already known.
		if (m_LicensePlateManager && !m_LicensePlateManager.m_Registration.IsEmpty())
			m_TextWidget.SetText(m_LicensePlateManager.m_Registration);

		LF_Debug.Info("LicensePlate", "plate render target bound");
	}
};
