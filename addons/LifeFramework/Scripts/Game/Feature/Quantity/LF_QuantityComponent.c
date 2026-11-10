//------------------------------------------------------------------------------------------------
//! One visual tier for a quantity item. When the stack quantity reaches m_iQuantityAmount,
//! the owner swaps to m_StackModel; below the lowest tier it returns to its base model.
[BaseContainerProps(), SCR_BaseContainerCustomTitleField("m_iQuantityAmount", "%1")]
class LF_QuantityStack
{
	[Attribute("0", UIWidgets.Range, "Quantity at and above which this stack model is shown")]
	int m_iQuantityAmount;

	[Attribute(ResourceName.Empty, UIWidgets.ResourcePickerThumbnail, "Model to show for this quantity tier", "xob")]
	ResourceName m_StackModel;
}

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "EveronLife/Feature/Quantity", description: "Virtual quantities for inventory items.")]
class LF_QuantityComponentClass : ScriptComponentClass
{
	[Attribute(defvalue: "0", desc: "Inclusive maximal quantity this item can stack to. 0 means unlimited stack size.", params: "0 2000000000 1")]
	int m_iMaxQuantity;

	//------------------------------------------------------------------------------------------------
	override static array<typename> CannotCombine(IEntityComponentSource src)
	{
		return {LF_QuantityComponent}; // Prevent user from adding multiple quantity components
	}
}

class LF_QuantityComponent : ScriptComponent
{
	protected static ref map<IEntity, bool> s_mQuantityTransferIntents;
	//! World time (ms) each intent was set. An intent whose move never produced an OnItemAdded
	//! (the transfer failed) goes stale after INTENT_TTL_MS instead of steering a later, unrelated add.
	protected static ref map<IEntity, float> s_mQuantityTransferIntentTimes;
	static const float INTENT_TTL_MS = 5000;

	[RplProp(onRplName: "OnQuantityChanged")]
	protected int m_iQuantity = 1;

	[Attribute(defvalue: "1", desc: "Quantity this item spawns with. Must be 1 or higher; a value below 1 is ignored.", params: "1 2000000000 1")]
	int m_iInitQuantity;

	[Attribute("", UIWidgets.Object, "Quantity tiers whose models replace the item mesh at and above m_iQuantityAmount", category: "Quantity Visuals")]
	ref array<ref LF_QuantityStack> m_aQuantityStacks;

	protected ref ScriptInvoker m_pOnQuantityChanged;

	protected VObject m_InitalModel;

	//! Last stack-tier model applied to the owner; empty when the base model is shown.
	protected ResourceName m_sAppliedStackModel;

	//! Holds the applied tier mesh's resource. Without it the resource is released when
	//! UpdateStackModel returns, and the next SetObject touches a released VObject.
	protected ref Resource m_AppliedStackResource;

	//------------------------------------------------------------------------------------------------
	int GetMaxQuantity()
	{
		int result = LF_QuantityComponentClass.Cast(GetComponentData(GetOwner())).m_iMaxQuantity;
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
		RplComponent rpl = LF_Component<RplComponent>.Find(GetOwner());
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
	static ResourceName GetStackModelForQuantity(int quantity, array<ref LF_QuantityStack> stacks)
	{
		ResourceName model;
		if (!stacks)
			return model;

		int bestThreshold = -1;
		foreach (LF_QuantityStack stack : stacks)
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
	//! Mesh swaps since the session started. A test reads it to prove a plain item keeps its mesh.
	static int s_iModelSwaps;

	//------------------------------------------------------------------------------------------------
	void UpdateStackModel()
	{
		// No tiers: the prefab's own mesh stays. Re-setting it would drop its material overrides.
		if (!m_aQuantityStacks || m_aQuantityStacks.IsEmpty())
			return;

		IEntity owner = GetOwner();
		if (!owner)
			return;

		ResourceName stackModelName = GetStackModelForQuantity(m_iQuantity, m_aQuantityStacks);
		VObject targetModel;
		Resource targetResource;

		if (stackModelName)
		{
			Resource resource = Resource.Load(stackModelName);
			if (!resource || !resource.IsValid())
			{
				LF_Debug.Error("Quantity", string.Format("stack model %1 for quantity %2 does not resolve", stackModelName, m_iQuantity));
				return;
			}

			BaseResourceObject resourceObject = resource.GetResource();
			if (!resourceObject)
			{
				LF_Debug.Error("Quantity", string.Format("stack model %1 for quantity %2 has no resource", stackModelName, m_iQuantity));
				return;
			}

			targetModel = resourceObject.ToVObject();
			if (!targetModel)
			{
				LF_Debug.Error("Quantity", string.Format("stack model %1 for quantity %2 is not a mesh", stackModelName, m_iQuantity));
				return;
			}

			targetResource = resource;
			LF_Debug.Info("Quantity", string.Format("stack visual %1 at quantity %2", stackModelName, m_iQuantity));
			m_sAppliedStackModel = stackModelName;
		}
		else
		{
			// Already on the base mesh: SetObject with an empty remap would drop the prefab's
			// MaterialAssignClass overrides and the mesh would render its baked material
			// (the white mining items, 2026-11-08).
			if (m_sAppliedStackModel.IsEmpty())
				return;

			if (!m_InitalModel)
				m_InitalModel = GetOwner().GetVObject();
			if (!m_InitalModel)
				return;

			targetModel = m_InitalModel;
			m_sAppliedStackModel = "";
		}

		s_iModelSwaps++;
		owner.SetObject(targetModel, "");
		// Swap the held resource only after SetObject, so the old mesh stays valid during the swap.
		m_AppliedStackResource = targetResource;
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		SetEventMask(owner, EntityEvent.INIT);
	}

	//------------------------------------------------------------------------------------------------
	//! A deleted item must not leave a dead key (and its intent) in the static intent map.
	override void OnDelete(IEntity owner)
	{
		if (owner && s_mQuantityTransferIntents)
			s_mQuantityTransferIntents.Remove(owner);
		if (owner && s_mQuantityTransferIntentTimes)
			s_mQuantityTransferIntentTimes.Remove(owner);

		super.OnDelete(owner);
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
	bool CanCombine(notnull LF_QuantityComponent quantitySource)
	{
		bool notSelf = this != quantitySource;
		bool prefabEqual = LF_Utils.GetPrefabName(GetOwner()) == LF_Utils.GetPrefabName(quantitySource.GetOwner());
		bool capacityLeft = m_iQuantity < GetMaxQuantity();
		return notSelf && prefabEqual && capacityLeft;
	}

	//------------------------------------------------------------------------------------------------
	//! Ported from Jacob-Mango/EveronLife branch feature/item-stacking (MIT).
	//! Combine adds the source quantity onto the destination. 2 and 3 yield 5.
	static int CombineQuantities(int destinationQuantity, int sourceQuantity)
	{
		if (destinationQuantity < 0)
			destinationQuantity = 0;
		if (sourceQuantity < 0)
			sourceQuantity = 0;
		return destinationQuantity + sourceQuantity;
	}

	//------------------------------------------------------------------------------------------------
	//! Ported from Jacob-Mango/EveronLife branch feature/item-stacking (MIT).
	//! A split that takes splitSize out of totalQuantity leaves the remainder.
	//! Split of 5 into 2 leaves 3. An out-of-range split leaves the stack unchanged.
	static int QuantityRemainingAfterSplit(int totalQuantity, int splitSize)
	{
		if (splitSize < 1 || splitSize >= totalQuantity)
			return totalQuantity;
		return totalQuantity - splitSize;
	}

	//------------------------------------------------------------------------------------------------
	//! Return new quantity after operation
	int Combine(notnull LF_QuantityComponent quantitySource, int amount = -1, out int transferred = 0)
	{
		if (CanCombine(quantitySource))
		{
			if (amount == -1) amount = quantitySource.GetQuantity();
			if (amount < 0) amount = 0;

			int take = amount;
			if (take > quantitySource.GetQuantity())
				take = quantitySource.GetQuantity();
			if (take > GetRemainingCapacity())
				take = GetRemainingCapacity();

			int combined = CombineQuantities(m_iQuantity, take);
			transferred = combined - m_iQuantity;

			AddQuantity(transferred);
			quantitySource.AddQuantity(-transferred);
		}

		return m_iQuantity;
	}

	//------------------------------------------------------------------------------------------------
	void Split(int splitSize = -1)
	{
		if (splitSize == -1) splitSize = m_iQuantity / 2;
		int kept = QuantityRemainingAfterSplit(m_iQuantity, splitSize);
		if (kept == m_iQuantity) return;
		splitSize = m_iQuantity - kept;

		IEntity owner = GetOwner();
		IEntity destinationEntity = LF_Utils.SpawnEntityPrefab(LF_Utils.GetPrefabName(owner), owner.GetOrigin());
		LF_QuantityComponent quantityDestination = LF_Component<LF_QuantityComponent>.Find(destinationEntity);
		if (!quantityDestination)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(destinationEntity);
			return;
		}

		InventoryItemComponent sourceInventoryItem = LF_Component<InventoryItemComponent>.Find(owner);

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

		// The fresh entity spawned holding its init quantity (100 for MoneyStack). Size it and
		// mark it keep-separate BEFORE the insert: OnItemAdded runs inside the insert, and
		// without the intent it merges the whole spawned stack into the source (split 1 => +99).
		quantityDestination.SetQuantity(splitSize);
		SetTransferIntent(destinationEntity, true);

		BaseInventoryStorageComponent storage = sourceInventoryItem.GetParentSlot().GetStorage();
		InventoryStorageManagerComponent storageManager = LF_InventoryUtils.GetResponsibleStorageManager(owner);
		if (!storageManager || !storageManager.TryInsertItemInStorage(destinationEntity, storage))
		{
			// Storage could not take the split item: the source was never touched, drop the spawned entity
			LF_Debug.Warn("Quantity", string.Format("split refused: storage did not take a %1 stack", splitSize));
			RemoveTransferIntent(destinationEntity);
			SCR_EntityHelper.DeleteEntityAndChildren(destinationEntity);
			return;
		}

		AddQuantity(-splitSize);
	}

	//------------------------------------------------------------------------------------------------
	static void SetTransferIntent(notnull IEntity sourceEntity, bool keepSeperate)
	{
		if (!s_mQuantityTransferIntents) s_mQuantityTransferIntents = new map<IEntity, bool>();
		if (!s_mQuantityTransferIntentTimes) s_mQuantityTransferIntentTimes = new map<IEntity, float>();
		s_mQuantityTransferIntents.Set(sourceEntity, keepSeperate);
		s_mQuantityTransferIntentTimes.Set(sourceEntity, NowMs());
	}

	//------------------------------------------------------------------------------------------------
	static void RemoveTransferIntent(notnull IEntity sourceEntity)
	{
		if (s_mQuantityTransferIntents) s_mQuantityTransferIntents.Remove(sourceEntity);
		if (s_mQuantityTransferIntentTimes) s_mQuantityTransferIntentTimes.Remove(sourceEntity);
	}


	//------------------------------------------------------------------------------------------------
	//! \return True when an intent set at setMs is too old to belong to the add happening at nowMs.
	static bool IsIntentStale(float setMs, float nowMs)
	{
		return (nowMs - setMs) > INTENT_TTL_MS;
	}

	//------------------------------------------------------------------------------------------------
	//! Game-mode start/end reset (LF_StaticReset).
	static void ResetStatics()
	{
		s_mQuantityTransferIntents = null;
		s_mQuantityTransferIntentTimes = null;
	}

	//------------------------------------------------------------------------------------------------
	protected static float NowMs()
	{
		if (!GetGame() || !GetGame().GetWorld())
			return 0;

		return GetGame().GetWorld().GetWorldTime();
	}

	//------------------------------------------------------------------------------------------------
	static bool HandleOnItemAdded(InventoryStorageManagerComponent invManager, BaseInventoryStorageComponent storageOwner, IEntity item)
	{
		LF_QuantityComponent quantitySource = LF_Component<LF_QuantityComponent>.Find(item);
		if (!quantitySource) return false;

		bool ignoreSuper;
		if (Replication.IsServer())
		{
			bool keepSeperate;
			bool intentSet;
			if (s_mQuantityTransferIntents)
			{
				intentSet = s_mQuantityTransferIntents.Find(item, keepSeperate);
				if (intentSet)
				{
					float setMs;
					if (s_mQuantityTransferIntentTimes && s_mQuantityTransferIntentTimes.Find(item, setMs) && IsIntentStale(setMs, NowMs()))
					{
						// The move this intent was set for failed; this add is a different one.
						LF_Debug.Warn("Quantity", string.Format("stale transfer intent ignored for %1", item));
						intentSet = false;
						keepSeperate = false;
					}

					RemoveTransferIntent(item);
				}
			}

			// A store's goods (on its shelf, or taken and not yet paid) keep their own identity: the unpaid
			// tag belongs to the item, and merging it into a stack would delete the tagged item (#202).
			if (LF_Stores.HoldsApart(item))
				keepSeperate = true;

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
	protected static bool TransferQuantity(InventoryStorageManagerComponent invManager, LF_QuantityComponent quantitySource, BaseInventoryStorageComponent storageRestriction = null)
	{
		array<IEntity> combineableItems();
		LF_QuantityCombineablePredicate quantityCombinePredicate(quantitySource, storageRestriction);
		invManager.FindItems(combineableItems, quantityCombinePredicate);

		array<LF_QuantityComponent> quantityComponents = ExtractQuantityComponents(combineableItems);

		foreach (LF_QuantityComponent quantityDestination : SortByQuantity(quantityComponents))
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
	static array<LF_QuantityComponent> SortByQuantity(notnull array<LF_QuantityComponent> components, bool descending = true)
	{
		array<int> sortKeys();
		map<int, ref array<LF_QuantityComponent>> componentMap();
		foreach (LF_QuantityComponent quantityComponent : components)
		{
			int quantity = quantityComponent.GetQuantity();

			if (!sortKeys.Contains(quantity)) sortKeys.Insert(quantity);

			array<LF_QuantityComponent> sameQuantity = componentMap.Get(quantity);
			if (!sameQuantity)
			{
				sameQuantity = {};
				componentMap.Set(quantity, sameQuantity);
			}

			sameQuantity.Insert(quantityComponent);
		}
		sortKeys.Sort(descending);

		array<LF_QuantityComponent> sorted();
		sorted.Reserve(components.Count());
		foreach (int sortKey : sortKeys)
		{
			foreach (LF_QuantityComponent quantityDestination : componentMap.Get(sortKey))
			{
				sorted.Insert(quantityDestination);
			}
		}

		return sorted;
	}

	//------------------------------------------------------------------------------------------------
	static array<LF_QuantityComponent> ExtractQuantityComponents(notnull array<IEntity> entities)
	{
		array<LF_QuantityComponent> components();
		components.Reserve(entities.Count());
		foreach(IEntity entity : entities)
		{
			components.Insert(LF_Component<LF_QuantityComponent>.Find(entity));
		}

		return components;
	}

	//------------------------------------------------------------------------------------------------
	protected static void RefreshInventory(IEntity item)
	{
		SCR_InventoryMenuUI inventoryMenu = SCR_InventoryMenuUI.LF_GetCurrentInstance();
		if (inventoryMenu) inventoryMenu.LF_QuantityRefresh(item);
	}
}

class LF_QuantityCombineablePredicate: InventorySearchPredicate
{
	protected LF_QuantityComponent m_pQuantitySource;
	protected BaseInventoryStorageComponent m_pStorageRestriction;

	//------------------------------------------------------------------------------------------------
	void LF_QuantityCombineablePredicate(notnull LF_QuantityComponent quantitySource, BaseInventoryStorageComponent storageRestriction = null)
	{
		m_pQuantitySource = quantitySource;
		m_pStorageRestriction = storageRestriction;
		QueryComponentTypes.Insert(LF_QuantityComponent);
	}

	//------------------------------------------------------------------------------------------------
	override protected bool IsMatch(BaseInventoryStorageComponent storage, IEntity item, array<GenericComponent> queriedComponents, array<BaseItemAttributeData> queriedAttributes)
	{
		return (!m_pStorageRestriction || m_pStorageRestriction == storage) && (LF_QuantityComponent.Cast(queriedComponents[0])).CanCombine(m_pQuantitySource);
	}
}
