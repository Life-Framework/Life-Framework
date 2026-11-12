class LF_GatherAction : ScriptedUserAction
{
	[Attribute("", UIWidgets.EditBox, "Chain node id. When set, tool, yield, and cooldown come from ResourceChains.conf", category: "General")]
	protected string m_sNodeId;

	[Attribute(desc: "Prefab what item is gathered", category: "General")]
	protected ResourceName m_GatherItemPrefab;

	[Attribute(defvalue:"1", desc: "Amount of items to receive", category: "General")]
	protected int m_GatherAmount;

	[Attribute(desc: "Item required for gathering", category: "Requirements")]
	protected ResourceName m_GatherToolRequirement;

	[Attribute(desc: "Check entire inventory for required item too", category: "Requirements")]
	protected bool m_CheckInventoryForToolRequirement;

	[Attribute(desc: "Maximum amount of times to gather before resource has timeout. 0 = unlimited", category: "Limits")]
	protected int m_GatherAmountMax;

	[Attribute(desc: "Cooldown until gathering attempts are restocked, in milliseconds", category: "Limits")]
	protected float m_GatherTimeout;

	[Attribute("#LF-Gather_Action", UIWidgets.EditBox, "Prompt, %1 is the item name. Gather %1 by default; a seed crate says Take %1", category: "General")]
	protected string m_sActionFormat;

	protected int m_iRemainingGathers;
	protected WorldTimestamp m_NextRestock;

	protected string m_sDisplayName;

	//------------------------------------------------------------------------------------------------
	// User has performed the action
	// play a pickup sound and then add the correct amount to the users inventory
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!pOwnerEntity || !LF_NetworkUtils.ShouldApplyAction(pOwnerEntity)) return;
		if (!LF_NetworkUtils.IsOwner(pOwnerEntity))
			LF_Debug.Warn("Gathering", string.Format("gather applied without replication owner on %1", pOwnerEntity.GetName()));
		if (!ApplyNodeFromCatalog())
			return;
		if (!IsGatherRequestConfigured(m_GatherItemPrefab, m_GatherAmount))
		{
			LF_Debug.Error("Gathering", "rejected gather: item prefab or amount is not configured");
			return;
		}

		// Server-side re-validation: the client's CanBePerformedScript is not a gate.
		if (!LF_CanGatherServer(pUserEntity))
		{
			LF_Debug.Warn("Gathering", string.Format("rejected gather (tool/cooldown) prefab=%1", m_GatherItemPrefab));
			return;
		}

		SCR_InventoryStorageManagerComponent inventoryManager = LF_Component<SCR_InventoryStorageManagerComponent>.Find(pUserEntity);
		int added = LF_InventoryUtils.AddAmount(inventoryManager, m_GatherItemPrefab, m_GatherAmount);
		if (added > 0)
		{
			inventoryManager.RpcAsk_PlaySound(LF_NetworkUtils.GetRplId(pUserEntity), "SOUND_PICK_UP");
			LF_Debug.Log("Gathering", string.Format("gathered %1 x%2", m_GatherItemPrefab, added));

			// Notify job manager for reward
			LF_JobManager jobManager = LF_JobManager.GetInstance();
			if (jobManager)
			{
				jobManager.OnGatherCompleted(pUserEntity, m_GatherItemPrefab);
			}
		}
		else
		{
			LF_Debug.Error("Gathering", string.Format("gather grant failed prefab=%1 requested=%2", m_GatherItemPrefab, m_GatherAmount));
		}

		// No limit configured: nothing to consume
		if (m_GatherAmountMax <= 0) return;

		//Replenish gathering count
		if(m_iRemainingGathers <= 0) m_iRemainingGathers = m_GatherAmountMax;

		//Consume one gathering
		m_iRemainingGathers--;

		//Initalize timeout if resource depleted
		if(m_iRemainingGathers <= 0)
		{
			m_NextRestock = pOwnerEntity.GetWorld().GetTimestamp().PlusMilliseconds(m_GatherTimeout);
		}
	}

	//------------------------------------------------------------------------------------------------
	// Formats name for action when hovering
	override bool GetActionNameScript(out string outName)
	{
		if (!m_sDisplayName)
		{
			// UIInfo names are usually #keys; translate before formatting or the raw key shows.
			string itemName = "#LF-Gather_Unknown";
			UIInfo uiInfo = LF_UIInfoUtils.GetInfo(m_GatherItemPrefab);
			if (uiInfo && !uiInfo.GetName().IsEmpty())
				itemName = uiInfo.GetName();
			string format = m_sActionFormat;
			if (format.IsEmpty())
				format = "#LF-Gather_Action";
			m_sDisplayName = WidgetManager.Translate(format, WidgetManager.Translate(itemName));
		}

		outName = m_sDisplayName;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	// Checks if a required item has been set in the Editor
	// If so, check if its in the users inventory/hands depending on settings set
	override bool CanBePerformedScript(IEntity user)
  	{
		if (!ApplyNodeFromCatalog())
			return false;

		if (!IsGatherRequestConfigured(m_GatherItemPrefab, m_GatherAmount))
			return false;

		if (m_GatherAmountMax > 0 && m_iRemainingGathers <= 0 && m_NextRestock.Greater(user.GetWorld().GetTimestamp()))
		{
			int secondsLeft = m_NextRestock.DiffMilliseconds(user.GetWorld().GetTimestamp()) / 1000;
			SetCannotPerformReason(WidgetManager.Translate("#LF-Gather_Wait", secondsLeft + 1)); //+1 to avoid 0 seconds left.
			return false;
		}

		// Crops need no tool: the stage decides. Past the final stage the crop cannot be gathered.
		LF_BaseCrop baseCrop = LF_BaseCrop.Cast(GetOwner());
		if (baseCrop)
			return baseCrop.CanGather();

		if (!LF_HasGatherTool(user))
		{
			if (m_CheckInventoryForToolRequirement)
				SetCannotPerformReason("#LF-Gather_RequiresItem");
			else
				SetCannotPerformReason("#LF-Gather_RequiresItemInHands");
			return false;
		}

		return true;
 	}

	//------------------------------------------------------------------------------------------------
	//! Server-side gate mirroring the client checks: cooldown and remaining gathers first,
	//! then the tool requirement. A forged perform cannot skip these.
	protected bool LF_CanGatherServer(IEntity user)
	{
		if (m_GatherAmountMax > 0 && m_iRemainingGathers <= 0 && m_NextRestock.Greater(user.GetWorld().GetTimestamp()))
			return false;

		LF_BaseCrop baseCrop = LF_BaseCrop.Cast(GetOwner());
		if (baseCrop)
			return baseCrop.CanGather();

		return LF_HasGatherTool(user);
	}

	//------------------------------------------------------------------------------------------------
	//! True if the user holds the configured tool (right hand, left gadget, or
	//! anywhere in inventory when m_CheckInventoryForToolRequirement is set).
	protected bool LF_HasGatherTool(IEntity user)
	{
		return HasToolIn(user, m_GatherToolRequirement, m_CheckInventoryForToolRequirement);
	}

	//------------------------------------------------------------------------------------------------
	//! Tool check behind LF_HasGatherTool. An empty requirement passes.
	static bool HasToolIn(IEntity user, ResourceName tool, bool checkInventory)
	{
		if (!tool) return true;

		CharacterControllerComponent characterController = LF_Component<CharacterControllerComponent>.Find(user);
		if (characterController)
		{
			IEntity rightHandItem = characterController.GetRightHandItem();
			if (LF_Utils.SamePrefab(LF_Utils.GetPrefabName(rightHandItem), tool)) return true;

			IEntity leftHandItem = characterController.GetAttachedGadgetAtLeftHandSlot();
			if (LF_Utils.SamePrefab(LF_Utils.GetPrefabName(leftHandItem), tool)) return true;
		}

		if (checkInventory)
		{
			if (LF_InventoryUtils.GetAmount(user, tool) > 0) return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//------------------------------------------------------------------------------------------------
	//! Copies tool, yield, and cooldown from the chain config when this action
	//! names a node. A missing or broken row logs and leaves the action unable
	//! to grant anything. An empty id keeps the prefab attributes.
	protected bool ApplyNodeFromCatalog()
	{
		if (m_sNodeId.IsEmpty())
			return true;

		LF_ResourceNodeDef node = LF_ResourceChainCatalog.FindNode(m_sNodeId);
		if (!LF_ResourceChainCatalog.IsNodeUsable(node))
		{
			LF_Debug.Error("Gathering", string.Format("rejected gather: node %1 is missing or its item prefab does not resolve", m_sNodeId));
			m_GatherItemPrefab = ResourceName.Empty;
			m_GatherAmount = 0;
			return false;
		}

		m_GatherItemPrefab = node.m_ItemPrefab;
		m_GatherAmount = node.m_iYield;
		m_GatherToolRequirement = node.m_ToolPrefab;
		m_GatherAmountMax = node.m_iMaxGathers;
		m_GatherTimeout = node.m_fCooldownMs;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Reject malformed gather actions before an attempt is consumed.
	static bool IsGatherRequestConfigured(ResourceName itemPrefab, int amount)
	{
		if (itemPrefab.IsEmpty())
			return false;
		return amount > 0;
	}
}
