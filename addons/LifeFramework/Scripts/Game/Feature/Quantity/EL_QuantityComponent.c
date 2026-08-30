//------------------------------------------------------------------------------------------------
//! One visual tier for a quantity item. When the stack quantity reaches m_iQuantityAmount,
//! the owner swaps to m_StackModel; below the lowest tier it returns to its base model.
[BaseContainerProps(), SCR_BaseContainerCustomTitleField("m_iQuantityAmount", "%1")]
class EL_QuantityStack
{
	[Attribute("0", UIWidgets.Range, "Quantity at and above which this stack model is shown")]
	int m_iQuantityAmount;

	[Attribute(ResourceName.Empty, UIWidgets.ResourcePickerThumbnail, "Model to show for this quantity tier", "xob")]
	ResourceName m_StackModel;
}

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "EveronLife/Feature/Quantity", description: "Virtual quantities for inventory items.")]
class EL_QuantityComponentClass : ScriptComponentClass
{
	[Attribute(defvalue: "0", desc: "Inclusive maximal quantity this item can stack to. 0 means unlimited stack size.", params: "0 2000000000 1")]
	int m_iMaxQuantity;

	//------------------------------------------------------------------------------------------------
	override static array<typename> CannotCombine(IEntityComponentSource src)
	{
		return {EL_QuantityComponent}; // Prevent user from adding multiple quantity components
	}
}

class EL_QuantityComponent : ScriptComponent
{
	protected static ref map<IEntity, bool> s_mQuantityTransferIntents;

	[RplProp(onRplName: "OnQuantityChanged")]
	protected int m_iQuantity = 1;

	[Attribute(defvalue: "1", desc: "Quantity this item spawns with. Must be 1 or higher; a value below 1 is ignored.", params: "1 2000000000 1")]
	int m_iInitQuantity;

	[Attribute("", UIWidgets.Object, "Quantity tiers whose models replace the item mesh at and above m_iQuantityAmount", category: "Quantity Visuals")]
	ref array<ref EL_QuantityStack> m_aQuantityStacks;

	protected ref ScriptInvoker m_pOnQuantityChanged;

	protected VObject m_InitalModel;

	//! Last stack-tier model applied to the owner; empty when the base model is shown.
	protected ResourceName m_sAppliedStackModel;

	//------------------------------------------------------------------------------------------------
	int GetMaxQuantity()
	{
		int result = EL_QuantityComponentClass.Cast(GetComponentData(GetOwner())).m_iMaxQuantity;
		if (result == 0) result = int.MAX;
		return result;
	}

	//------------------------------------------------------------------------------------------------
	int GetRemainingCapacity()
	{
		return GetMaxQuantity() - m_iQuantity;
	}

	//------------------------------------------------------------------------------------------------
	int GetQuantity()
	{
		return m_iQuantity;
	}

	//------------------------------------------------------------------------------------------------
	//! Returns new quantity after operation
	int AddQuantity(int quantity, bool allowPartial = true, out int change = 0)
	{
		if (quantity == 0) return 0;

		if (quantity > 0)
		{
			int capacity = GetRemainingCapacity();
			if (quantity > capacity)
			{
				if (!allowPartial) return 0;
				quantity = capacity;
			}
		}
		else if (quantity < -m_iQuantity)
		{
			if (!allowPartial) return 0;
			quantity = -m_iQuantity;
		}

		SetQuantity(m_iQuantity + quantity);
		change = quantity;
		return m_iQuantity;
	}

	//------------------------------------------------------------------------------------------------
	//! Returns new quantity after operation
	int RemoveQuantity(int quantity, bool allowPartial = true, out int change = 0)
	{
		return AddQuantity(-quantity, allowPartial, change);
	}

	//------------------------------------------------------------------------------------------------
	bool SetQuantity(int quantity)
	{
		RplComponent rpl = EL_Component<RplComponent>.Find(GetOwner());
		if (rpl && !rpl.IsMaster()) return false;
		if ((quantity < 0) || (quantity > GetMaxQuantity())) return false;

		m_iQuantity = quantity;

		if (quantity == 0)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(GetOwner());
		}
		else
		{
			Replication.BumpMe();
		}

		OnQuantityChanged(); // Call on authority

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void OnQuantityChanged()
	{
		UpdateStackModel();
		RefreshInventory(GetOwner());
		if (m_pOnQuantityChanged) m_pOnQuantityChanged.Invoke(this);
	}

	//------------------------------------------------------------------------------------------------
	//! The stack-tier model currently applied to the owner, or an empty ResourceName when the
	//! base model is shown. Observable so tests can assert the visual without a renderer.
	ResourceName GetActiveStackModel()
	{
		return m_sAppliedStackModel;
	}

	//------------------------------------------------------------------------------------------------
	//! Picks the model of the highest tier whose threshold the quantity clears. Empty when no
	//! tier applies, so the caller falls back to the base model. Independent of list order.
	//! \param quantity Current stack quantity.
	//! \param stacks Authored tiers, may be null or contain empty model entries.
	//! \return The winning model, or an empty ResourceName when none applies.
	static ResourceName GetStackModelForQuantity(int quantity, array<ref EL_QuantityStack> stacks)
	{
		ResourceName model;
		if (!stacks)
			return model;

		int bestThreshold = -1;
		foreach (EL_QuantityStack stack : stacks)
		{
			if (!stack || !stack.m_StackModel)
				continue;

			if (quantity >= stack.m_iQuantityAmount && stack.m_iQuantityAmount > bestThreshold)
			{
				bestThreshold = stack.m_iQuantityAmount;
				model = stack.m_StackModel;
			}
		}

		return model;
	}

	//------------------------------------------------------------------------------------------------
	//! Swaps the owner's mesh to the tier matching the current quantity, restoring the base
	//! model below the lowest tier. Runs on the authority after every quantity change and on
	//! proxies through the replicated property callback. Fail-safe: a missing list, unresolvable
	//! model or missing base mesh logs an error and leaves the current model untouched.
	void UpdateStackModel()
	{
		if (!m_aQuantityStacks)
			return;

		IEntity owner = GetOwner();
		if (!owner)
			return;

		ResourceName stackModelName = GetStackModelForQuantity(m_iQuantity, m_aQuantityStacks);
		VObject targetModel;

		if (stackModelName)
		{
			Resource resource = Resource.Load(stackModelName);
			if (!resource || !resource.IsValid())
			{
				EL_Debug.Error("Quantity", string.Format("stack model %1 for quantity %2 does not resolve", stackModelName, m_iQuantity));
				return;
			}

			BaseResourceObject resourceObject = resource.GetResource();
			if (!resourceObject)
			{
				EL_Debug.Error("Quantity", string.Format("stack model %1 for quantity %2 has no resource", stackModelName, m_iQuantity));
				return;
			}

			targetModel = resourceObject.ToVObject();
			if (!targetModel)
			{
				EL_Debug.Error("Quantity", string.Format("stack model %1 for quantity %2 is not a mesh", stackModelName, m_iQuantity));
				return;
			}

			EL_Debug.Info("Quantity", string.Format("stack visual %1 at quantity %2", stackModelName, m_iQuantity));
			m_sAppliedStackModel = stackModelName;
		}
		else
		{
			if (!m_InitalModel)
				m_InitalModel = GetOwner().GetVObject();
			if (!m_InitalModel)
				return;

			targetModel = m_InitalModel;
			m_sAppliedStackModel = "";
		}

		owner.SetObject(targetModel, "");
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		SetEventMask(owner, EntityEvent.INIT);
	}

	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		m_InitalModel = owner.GetVObject();

		if (m_iInitQuantity > 1)
			SetQuantity(m_iInitQuantity);
		else
			UpdateStackModel();
	}

	//------------------------------------------------------------------------------------------------
	ScriptInvoker GetOnQuantityChanged()
	{
		if (!m_pOnQuantityChanged) m_pOnQuantityChanged = new ScriptInvoker();
		return m_pOnQuantityChanged;
	}

	//------------------------------------------------------------------------------------------------
	bool CanCombine(notnull EL_QuantityComponent quantitySource)
	{
		bool notSelf = this != quantitySource;
		bool prefabEqual = EL_Utils.GetPrefabName(GetOwner()) == EL_Utils.GetPrefabName(quantitySource.GetOwner());
		bool capacityLeft = m_iQuantity < GetMaxQuantity();
		return notSelf && prefabEqual && capacityLeft;
	}

	//------------------------------------------------------------------------------------------------
	//! Return new quantity after operation
	int Combine(notnull EL_QuantityComponent quantitySource, int amount = -1, out int transferred = 0)
	{
		if (CanCombine(quantitySource))
		{
			if (amount == -1) amount = quantitySource.GetQuantity();
			if (amount < 0) amount = 0;

			transferred = EL_Utils.MinInt(quantitySource.GetQuantity(), EL_Utils.MinInt(GetRemainingCapacity(), amount));

			AddQuantity(transferred);
			quantitySource.AddQuantity(-transferred);
		}

		return m_iQuantity;
	}

	//------------------------------------------------------------------------------------------------
	void Split(int splitSize = -1)
	{
		if (splitSize == -1) splitSize = m_iQuantity / 2;
		if (splitSize < 1 || splitSize >= m_iQuantity) return;

		IEntity owner = GetOwner();
		IEntity destinationEntity = EL_Utils.SpawnEntityPrefab(EL_Utils.GetPrefabName(owner), owner.GetOrigin());
		EL_QuantityComponent quantityDestination = EL_Component<EL_QuantityComponent>.Find(destinationEntity);
		if (!quantityDestination)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(destinationEntity);
			return;
		}

		InventoryItemComponent sourceInventoryItem = EL_Component<InventoryItemComponent>.Find(owner);

		// Ground item, just move it somewhere else so it can be picked up seperatly from the source stack
		if (!sourceInventoryItem || !sourceInventoryItem.GetParentSlot())
		{
			AddQuantity(-splitSize);
			quantityDestination.SetQuantity(splitSize);
			SetTransferIntent(destinationEntity, true);

			vector maxDims;
			owner.GetBounds(null, maxDims);
			float minRadius = Math.Max(maxDims[0], maxDims[2]); //Bounding radius
			RandomGenerator random();
			destinationEntity.SetOrigin(random.GenerateRandomPointInRadius(minRadius + 0.05, minRadius + 0.25, owner.GetOrigin()));
			return;
		}

		BaseInventoryStorageComponent storage = sourceInventoryItem.GetParentSlot().GetStorage();
		InventoryStorageManagerComponent storageManager = EL_InventoryUtils.GetResponsibleStorageManager(owner);
		if (!storageManager || !storageManager.TryInsertItemInStorage(destinationEntity, storage))
		{
			// Storage could not take the split item: roll the quantity back and drop the spawned entity
			SCR_EntityHelper.DeleteEntityAndChildren(destinationEntity);
			return;
		}

		AddQuantity(-splitSize);
		quantityDestination.SetQuantity(splitSize);
		SetTransferIntent(destinationEntity, true);
	}

	//------------------------------------------------------------------------------------------------
	static void SetTransferIntent(notnull IEntity sourceEntity, bool keepSeperate)
	{
		if (!s_mQuantityTransferIntents) s_mQuantityTransferIntents = new map<IEntity, bool>();
		s_mQuantityTransferIntents.Set(sourceEntity, keepSeperate);
	}

	//------------------------------------------------------------------------------------------------
	static void RemoveTransferIntent(notnull IEntity sourceEntity)
	{
		if (s_mQuantityTransferIntents) s_mQuantityTransferIntents.Remove(sourceEntity);
	}

	//------------------------------------------------------------------------------------------------
	static bool HandleOnItemAdded(InventoryStorageManagerComponent invManager, BaseInventoryStorageComponent storageOwner, IEntity item)
	{
		EL_QuantityComponent quantitySource = EL_Component<EL_QuantityComponent>.Find(item);
		if (!quantitySource) return false;

		bool ignoreSuper;
		if (Replication.IsServer())
		{
			bool keepSeperate;
			bool intentSet;
			if (s_mQuantityTransferIntents)
			{
				intentSet = s_mQuantityTransferIntents.Find(item, keepSeperate);
				if (intentSet) RemoveTransferIntent(item);
			}

			if (!keepSeperate)
			{
				BaseInventoryStorageComponent storageRestriction;
				if (intentSet) storageRestriction = storageOwner;

				//If Quantity source fully distributed onto other items, abort. Quantity sync on the item will cause menu refresh, so no need to call it here
				ignoreSuper = TransferQuantity(invManager, quantitySource, storageRestriction);
			}
		}

		RefreshInventory(item);

		return ignoreSuper;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool TransferQuantity(InventoryStorageManagerComponent invManager, EL_QuantityComponent quantitySource, BaseInventoryStorageComponent storageRestriction = null)
	{
		array<IEntity> combineableItems();
		EL_QuantityCombineablePredicate quantityCombinePredicate(quantitySource, storageRestriction);
		invManager.FindItems(combineableItems, quantityCombinePredicate);

		array<EL_QuantityComponent> quantityComponents = ExtractQuantityComponents(combineableItems);

		foreach (EL_QuantityComponent quantityDestination : SortByQuantity(quantityComponents))
		{
			// In case 0 quantity remains on the source it was deleted so the caller scope needs to skip the super call
			quantityDestination.Combine(quantitySource);
			if (!quantitySource || quantitySource.GetQuantity() == 0) return true;
		}

		// Some quantity remained on the source so allow caller scope to conttinue to super call
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Sort quantity components in descending order
	static array<EL_QuantityComponent> SortByQuantity(notnull array<EL_QuantityComponent> components, bool descending = true)
	{
		array<int> sortKeys();
		map<int, ref array<EL_QuantityComponent>> componentMap();
		foreach (EL_QuantityComponent quantityComponent : components)
		{
			int quantity = quantityComponent.GetQuantity();

			if (!sortKeys.Contains(quantity)) sortKeys.Insert(quantity);

			array<EL_QuantityComponent> sameQuantity = componentMap.Get(quantity);
			if (!sameQuantity)
			{
				sameQuantity = {};
				componentMap.Set(quantity, sameQuantity);
			}

			sameQuantity.Insert(quantityComponent);
		}
		sortKeys.Sort(descending);

		array<EL_QuantityComponent> sorted();
		sorted.Reserve(components.Count());
		foreach (int sortKey : sortKeys)
		{
			foreach (EL_QuantityComponent quantityDestination : componentMap.Get(sortKey))
			{
				sorted.Insert(quantityDestination);
			}
		}

		return sorted;
	}

	//------------------------------------------------------------------------------------------------
	static array<EL_QuantityComponent> ExtractQuantityComponents(notnull array<IEntity> entities)
	{
		array<EL_QuantityComponent> components();
		components.Reserve(entities.Count());
		foreach(IEntity entity : entities)
		{
			components.Insert(EL_Component<EL_QuantityComponent>.Find(entity));
		}

		return components;
	}

	//------------------------------------------------------------------------------------------------
	protected static void RefreshInventory(IEntity item)
	{
		SCR_InventoryMenuUI inventoryMenu = SCR_InventoryMenuUI.EL_GetCurrentInstance();
		if (inventoryMenu) inventoryMenu.EL_QuantityRefresh(item);
	}
}

class EL_QuantityCombineablePredicate: InventorySearchPredicate
{
	protected EL_QuantityComponent m_pQuantitySource;
	protected BaseInventoryStorageComponent m_pStorageRestriction;

	//------------------------------------------------------------------------------------------------
	void EL_QuantityCombineablePredicate(notnull EL_QuantityComponent quantitySource, BaseInventoryStorageComponent storageRestriction = null)
	{
		m_pQuantitySource = quantitySource;
		m_pStorageRestriction = storageRestriction;
		QueryComponentTypes.Insert(EL_QuantityComponent);
	}

	//------------------------------------------------------------------------------------------------
	override protected bool IsMatch(BaseInventoryStorageComponent storage, IEntity item, array<GenericComponent> queriedComponents, array<BaseItemAttributeData> queriedAttributes)
	{
		return (!m_pStorageRestriction || m_pStorageRestriction == storage) && (EL_QuantityComponent.Cast(queriedComponents[0])).CanCombine(m_pQuantitySource);
	}
}
