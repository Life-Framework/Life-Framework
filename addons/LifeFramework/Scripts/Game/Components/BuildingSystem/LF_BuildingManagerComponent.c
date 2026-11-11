[ComponentEditorProps(category: "EveronLife/Game/Building")]
class LF_BuildingManagerComponentClass : ScriptComponentClass
{
}

class LF_BuildingManagerComponent : ScriptComponent
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
	private LF_BuildingPerformerComponent m_BuildingPerformer;
	private LF_BuildingPerformerComponent m_LastBuildingPerformer;

	//! Farthest a placement may land from the placing character, in meters.
	static const float MAX_PLACEMENT_DISTANCE = 5.0;

	//------------------------------------------------------------------------------------------------
	//! \return True when pos is within MAX_PLACEMENT_DISTANCE of characterPos.
	static bool IsPlacementInRange(vector characterPos, vector pos)
	{
		return vector.Distance(characterPos, pos) <= MAX_PLACEMENT_DISTANCE;
	}

	//------------------------------------------------------------------------------------------------
	//! Client asks the server to spawn the prefab it holds as a building performer.
	//! Server re-validates the request against the caller's held performer.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void RpcAsk_SpawnEntityPrefab(ResourceName resource, vector pos)
	{
		if (!LF_RateLimit.AllowCharacter(GetOwner(), LF_ERequestKind.SPAWN, "building placement"))
			return;

		PlaceHeldBuilding(resource, pos);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: places the held building item at pos. The caller must hold a performer for that
	//! prefab, pos must be within MAX_PLACEMENT_DISTANCE, and the held item is consumed before
	//! the building spawns: one seed, one crop.
	//! \return The spawned building, or null when refused.
	IEntity PlaceHeldBuilding(ResourceName resource, vector pos)
	{
		IEntity character = GetOwner();
		if (!character)
			return null;

		LF_BuildingPerformerComponent performer = GetHeldBuildingPerformer(character);
		if (!performer || performer.m_BuildingPrefab != resource)
		{
			LF_Debug.Warn("Farming", string.Format("placement refused: resource=%1 (performer mismatch or none)", resource));
			return null;
		}

		if (!IsPlacementInRange(character.GetOrigin(), pos))
		{
			LF_Debug.Warn("Farming", string.Format("placement refused: resource=%1 pos=%2 is more than %3 m from the player", resource, pos, MAX_PLACEMENT_DISTANCE));
			return null;
		}

		if (!ConsumeBuildingItem(character, performer.GetOwner()))
		{
			LF_Debug.Error("Farming", string.Format("placement refused: resource=%1 (held item could not be consumed)", resource));
			return null;
		}

		LF_Debug.Log("Farming", string.Format("placement accepted: resource=%1 pos=%2", resource, pos));
		return LF_Utils.SpawnEntityPrefab(resource, pos);
	}

	//------------------------------------------------------------------------------------------------
	//! Uses up one building item. A quantity stack loses one; a single item leaves the hand and
	//! is deleted.
	//! \return True when one item was consumed.
	static bool ConsumeBuildingItem(IEntity character, IEntity item)
	{
		if (!character || !item)
			return false;

		LF_QuantityComponent quantity = LF_Component<LF_QuantityComponent>.Find(item);
		if (quantity && quantity.GetQuantity() > 1)
		{
			int change;
			quantity.AddQuantity(-1, false, change);
			return change == -1;
		}

		CharacterControllerComponent controller = LF_Component<CharacterControllerComponent>.Find(character);
		if (controller && controller.GetAttachedGadgetAtLeftHandSlot() == item)
			controller.RemoveGadgetFromHand(true);

		SCR_EntityHelper.DeleteEntityAndChildren(item);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	void OnMouseLeftDown()
	{
		if (!m_CanBuild || !m_BuildingPerformer)
			return;

		if (Replication.IsServer())
		{
			PlaceHeldBuilding(m_BuildingPerformer.m_BuildingPrefab, m_PosInWorld);
			return;
		}

		Rpc(RpcAsk_SpawnEntityPrefab, m_BuildingPerformer.m_BuildingPrefab, m_PosInWorld);
	}

	//------------------------------------------------------------------------------------------------
	//! Set preview mesh to new handheld item mesh and change to preview material.
	private void SetNewPreviewMesh()
	{
		if (!m_PreviewEntity || !m_BuildingPerformer)
			return;

		Resource meshResource = Resource.Load(m_BuildingPerformer.m_PreviewMesh);
		if (!meshResource || !meshResource.IsValid())
		{
			LF_Debug.Warn("Farming", "preview mesh skipped: prefab does not resolve");
			return;
		}

		m_PreviewEntity.SetObject(meshResource.GetResource().ToVObject(), "");
		LF_Utils.ChangeEntityMaterial(m_PreviewEntity, m_PreviewMaterial);
	}

	//------------------------------------------------------------------------------------------------
	//! Get BuildingPerformerComponent from current held item
	private LF_BuildingPerformerComponent GetHeldBuildingPerformer(IEntity owner)
	{
		SCR_GadgetManagerComponent gadgetManager = SCR_GadgetManagerComponent.GetGadgetManager(owner);
		if (!gadgetManager)
			return null;

		return LF_BuildingPerformerComponent.Cast(gadgetManager.GetHeldGadgetComponent());
	}

	//------------------------------------------------------------------------------------------------
	//! Update preview entity origin to mouse pos
	private bool UpdatePreviewOrigin()
	{
		IEntity cursorTarget = m_Cam.GetCursorTargetWithPosition(m_PosInWorld);
		if (!cursorTarget)
			return false;

		if (m_BuildingPerformer && m_BuildingPerformer.m_GroundType == EGroundType.TERRAIN && cursorTarget.Type() != GenericTerrainEntity)
			return false;

		if (!m_PreviewEntity)
			return false;

		// The server refuses placements beyond this range; do not offer them.
		if (!IsPlacementInRange(GetOwner().GetOrigin(), m_PosInWorld))
			return false;

		m_PreviewEntity.SetOrigin(m_PosInWorld);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Check if buildingPerformer is in hand and update visibility of preview entity
	private bool UpdateHandBuildingPerformer(IEntity owner)
	{
		m_BuildingPerformer = GetHeldBuildingPerformer(owner);

		if (!m_PreviewEntity)
			return false;

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

		// The camera manager can be null before the first camera exists; retry next frame.
		CameraManager cameraManager = GetGame().GetCameraManager();
		if (!cameraManager)
			return;

		m_Cam = cameraManager.CurrentCamera();
		m_InputManager = GetGame().GetInputManager();

		if (!m_InputManager || !m_Cam)
			return;

		// The preview is a visual for the owning client only: spawn it local (not replicated)
		// so no networked orphan appears on other machines. OnDelete removes it.
		m_PreviewEntity = LF_Utils.SpawnEntityPrefab(m_BasePreviewPrefab, owner.GetOrigin(), "0 0 0", false);
		if (!m_PreviewEntity)
		{
			LF_Debug.Error("Farming", string.Format("building preview failed to spawn: %1", m_BasePreviewPrefab));
			// Degrade: building stays off for this character instead of retrying (and logging) every frame.
			ClearEventMask(owner, EntityEvent.FRAME);
			return;
		}

		m_InputManager.AddActionListener("MouseLeft", EActionTrigger.DOWN, OnMouseLeftDown);

		LF_Utils.ChangeEntityMaterial(m_PreviewEntity, m_PreviewMaterial);
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
	//! Character deleted (death, respawn, disconnect) -> remove the local preview entity.
	override void OnDelete(IEntity owner)
	{
		DeletePreview();
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	private void DeletePreview()
	{
		if (!m_PreviewEntity)
			return;

		SCR_EntityHelper.DeleteEntityAndChildren(m_PreviewEntity);
		m_PreviewEntity = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Destructor -> remove ActionListener, and the preview if OnDelete did not run
	void ~LF_BuildingManagerComponent()
	{
		if (m_InputManager)
			m_InputManager.RemoveActionListener("MouseLeft", EActionTrigger.DOWN, OnMouseLeftDown);

		DeletePreview();
	}
}