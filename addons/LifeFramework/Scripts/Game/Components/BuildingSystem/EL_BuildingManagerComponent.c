[ComponentEditorProps(category: "EveronLife/Game/Building")]
class EL_BuildingManagerComponentClass : ScriptComponentClass
{
}

class EL_BuildingManagerComponent : ScriptComponent
{
	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Preview entity material", "emat", category: "Building Preview")]
	private ResourceName m_PreviewMaterial;

	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Empty Base Preview Prefab", "et", category: "Building Preview")]
	private ResourceName m_BasePreviewPrefab;

	private IEntity m_PreviewEntity;
	private CameraBase m_Cam;
	private vector m_PosInWorld;
	private InputManager m_InputManager;
	private bool m_IsInit;
	private bool m_CanBuild;
	private ref EL_BuildingPerformerComponent m_BuildingPerformer;
	private ref EL_BuildingPerformerComponent m_LastBuildingPerformer;

	//------------------------------------------------------------------------------------------------
	//! Client asks the server to spawn the prefab it holds as a building performer.
	//! Server re-validates the request against the caller's held performer.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void RpcAsk_SpawnEntityPrefab(ResourceName resource, vector pos)
	{
		IEntity character = GetOwner();
		if (!character)
			return;

		EL_BuildingPerformerComponent performer = GetHeldBuildingPerformer(character);
		if (!performer || performer.m_BuildingPrefab != resource)
			return;

		EL_Utils.SpawnEntityPrefab(resource, pos);
	}

	//------------------------------------------------------------------------------------------------
	void OnMouseLeftDown()
	{
		if (m_CanBuild)
			Rpc(RpcAsk_SpawnEntityPrefab, m_BuildingPerformer.m_BuildingPrefab, m_PosInWorld);
	}

	//------------------------------------------------------------------------------------------------
	//! Set preview mesh to new handheld item mesh and change to preview material.
	private void SetNewPreviewMesh()
	{
		m_PreviewEntity.SetObject(Resource.Load(m_BuildingPerformer.m_PreviewMesh).GetResource().ToVObject(), "");
		EL_Utils.ChangeEntityMaterial(m_PreviewEntity, m_PreviewMaterial);
	}

	//------------------------------------------------------------------------------------------------
	//! Get BuildingPerformerComponent from current held item
	private EL_BuildingPerformerComponent GetHeldBuildingPerformer(IEntity owner)
	{
		SCR_GadgetManagerComponent gadgetManager = SCR_GadgetManagerComponent.GetGadgetManager(owner);
		if (!gadgetManager)
			return null;

		return EL_BuildingPerformerComponent.Cast(gadgetManager.GetHeldGadgetComponent());
	}

	//------------------------------------------------------------------------------------------------
	//! Update preview entity origin to mouse pos
	private bool UpdatePreviewOrigin()
	{
		IEntity cursorTarget = m_Cam.GetCursorTargetWithPosition(m_PosInWorld);

		if (m_BuildingPerformer.m_GroundType == EGroundType.TERRAIN && cursorTarget.Type() != GenericTerrainEntity)
			return false;

		m_PreviewEntity.SetOrigin(m_PosInWorld);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Check if buildingPerformer is in hand and update visibility of preview entity
	private bool UpdateHandBuildingPerformer(IEntity owner)
	{
		m_BuildingPerformer = GetHeldBuildingPerformer(owner);

		bool isVisible = (m_PreviewEntity.GetFlags() & EntityFlags.VISIBLE);

		// Added Performer
		if (m_BuildingPerformer && !isVisible)
			m_PreviewEntity.SetFlags(EntityFlags.VISIBLE, true);

		// Removed Performer
		if (!m_BuildingPerformer && isVisible)
			m_PreviewEntity.ClearFlags(EntityFlags.VISIBLE, true);

		return (m_BuildingPerformer != null);
	}

	//------------------------------------------------------------------------------------------------
	void InitComponent(IEntity owner)
	{
		RplComponent rplComponent = RplComponent.Cast(owner.FindComponent(RplComponent));

		// Remove frame mask on other proxies
		if (rplComponent && !rplComponent.IsOwner())
		{
			ClearEventMask(owner, EntityEvent.FRAME);
			return;
		}

		// Authority specific behavior

		m_Cam = GetGame().GetCameraManager().CurrentCamera();
		m_InputManager = GetGame().GetInputManager();

		if (!m_InputManager || !m_Cam)
			return;

		m_InputManager.AddActionListener("MouseLeft", EActionTrigger.DOWN, OnMouseLeftDown);

		// Spawn base preview and set invisible
		m_PreviewEntity = EL_Utils.SpawnEntityPrefab(m_BasePreviewPrefab, owner.GetOrigin());
		EL_Utils.ChangeEntityMaterial(m_PreviewEntity, m_PreviewMaterial);
		m_PreviewEntity.ClearFlags(EntityFlags.VISIBLE, true);

		m_IsInit = true;
	}

	//------------------------------------------------------------------------------------------------
	override void EOnFrame(IEntity owner, float timeSlice)
	{
		if (!m_IsInit)
		{
			InitComponent(owner);
			return;
		}

		// Authority specific behavior
		m_CanBuild = false;

		if (!m_PreviewEntity)
			return;

		if (!UpdateHandBuildingPerformer(owner))
			return;

		// Cache current hand item to not set it every frame.
		if (m_BuildingPerformer != m_LastBuildingPerformer)
		{
			SetNewPreviewMesh();
			m_LastBuildingPerformer = m_BuildingPerformer;
		}

		m_CanBuild = UpdatePreviewOrigin();
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		SetEventMask(owner, EntityEvent.FRAME);
	}

	//------------------------------------------------------------------------------------------------
	//! Destructor -> remove ActionListener
	void ~EL_BuildingManagerComponent()
	{
		if (m_InputManager)
			m_InputManager.RemoveActionListener("MouseLeft", EActionTrigger.DOWN, OnMouseLeftDown);
	}
}