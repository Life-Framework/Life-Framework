[EntityEditorProps(category: "EveronLife/HandCarry", description: "Storage for items that can only be carried in hands", color: "0 0 255 255")]
class LF_HandInventoryStorageComponentClass: UniversalInventoryStorageComponentClass
{
}

enum LF_EHandCarryState
{
	NONE,
	AWAIT_HOLSTER,
	READY,
	ACTIVE,
	AWAIT_SWAP
}

class LF_HandInventoryStorageComponent : UniversalInventoryStorageComponent
{
	protected LF_EHandCarryState m_eState;
	protected IEntity m_pNextCarryItem;

	//------------------------------------------------------------------------------------------------
	override protected bool CanStoreItem(IEntity item, int slotID)
	{
		return item && item.FindComponent(LF_HandCarryComponent);
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnAddedToSlot(IEntity item, int slotID)
	{
		super.OnAddedToSlot(item, slotID);

		if (!HasLocalControl()) return;

		CharacterControllerComponent characterController = LF_Component<CharacterControllerComponent>.Find(GetOwner());
		if (!characterController)
		{
			LF_Debug.Error("HandCarry", "hand carry skipped: owner has no CharacterControllerComponent");
			return;
		}

		IEntity currentGadget = characterController.GetAttachedGadgetAtLeftHandSlot();
		if (currentGadget)
		{
			if (currentGadget == item) return; //Handle same storage drag and drop during hand carry

			// If gadget, remove gadget from hand (instantly, no need to wait for gadget change invoke)
			characterController.RemoveGadgetFromHand(true);
		}

		m_pNextCarryItem = item;
		m_eState = LF_EHandCarryState.READY;

		// If weapon, holster weapon and and wait invoke of weapon changed
		BaseWeaponManagerComponent weaponManager = characterController.GetWeaponManagerComponent();
		if (weaponManager && weaponManager.GetCurrent())
		{
			m_eState = LF_EHandCarryState.AWAIT_HOLSTER;
			characterController.SelectWeapon(null);
		}

		TakeToHands();
	}

	//------------------------------------------------------------------------------------------------
	void OnGadgetStateChanged(IEntity gadget, bool isInHand, bool isOnGround)
	{
		if (!HasLocalControl()) return;

		if (m_eState >= LF_EHandCarryState.ACTIVE && (!isInHand || isOnGround))
		{
			// Only a deliberate action (the drop on the ground, a weapon taken, a swap) or a downed
			// or dead carrier lets go. Sprinting or a stance change that lowers the item for a
			// moment puts it back in the hands (#920).
			if (ShouldKeepCarrying(gadget, isInHand, isOnGround))
				RetakeToHands(gadget);
			else
				StopCarry(gadget);
		}
		else if(m_eState != LF_EHandCarryState.ACTIVE && isInHand)
		{
			// If active gadget is in the hand storage update the state to active
			InventoryItemComponent inventoryItem = LF_Component<InventoryItemComponent>.Find(gadget);
			if (inventoryItem)
			{
				InventoryStorageSlot parentSlot = inventoryItem.GetParentSlot();
				if (parentSlot && parentSlot.GetStorage() == this)
				{
					m_eState = LF_EHandCarryState.ACTIVE;
				}
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! True when the engine lowered a carried item that the player did not let go of.
	protected bool ShouldKeepCarrying(IEntity gadget, bool isInHand, bool isOnGround)
	{
		if (isInHand || isOnGround || !gadget || m_eState != LF_EHandCarryState.ACTIVE)
			return false;

		if (LF_ParcelComponent.CarrierIsDown(GetOwner()))
			return false;

		CharacterControllerComponent controller = LF_Component<CharacterControllerComponent>.Find(GetOwner());
		BaseWeaponManagerComponent weapons = null;
		if (controller)
			weapons = controller.GetWeaponManagerComponent();
		if (weapons && weapons.GetCurrent())
			return false;

		return LF_ParcelComponent.InSlot(gadget);
	}

	//------------------------------------------------------------------------------------------------
	protected void RetakeToHands(IEntity gadget)
	{
		LF_Debug.Info("HandCarry", "carried item lowered without a drop; back to the hands");
		m_pNextCarryItem = gadget;
		m_eState = LF_EHandCarryState.READY;
		TakeToHands();
	}

	//------------------------------------------------------------------------------------------------
	void OnGadgetModeSet(IEntity gadget, EGadgetMode targetMode, bool doFocus)
	{
		if (m_eState != LF_EHandCarryState.ACTIVE || !gadget || targetMode != EGadgetMode.IN_HAND) return;
		m_eState = LF_EHandCarryState.AWAIT_SWAP;
	}

	//------------------------------------------------------------------------------------------------
	void OnWeaponChanged(BaseWeaponComponent weapon, BaseWeaponComponent prevWeapon)
	{
		if (!HasLocalControl()) return;

		if(m_eState == LF_EHandCarryState.ACTIVE && weapon)
		{
			StopCarry();
		}
		else if (m_eState == LF_EHandCarryState.AWAIT_HOLSTER && !weapon)
		{
			m_eState = LF_EHandCarryState.READY;
			TakeToHands();
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void TakeToHands()
	{
		if (!m_pNextCarryItem || m_eState != LF_EHandCarryState.READY) return;

		IEntity character = GetOwner();

		// Remove all hand items except for the one that we want to carry
		ClearHandStorage(m_pNextCarryItem);

		SCR_GadgetManagerComponent gadgetManager = SCR_GadgetManagerComponent.GetGadgetManager(character);
		if (gadgetManager) gadgetManager.HandleInput(m_pNextCarryItem, 1);

		m_pNextCarryItem = null;
	}

	//------------------------------------------------------------------------------------------------
	protected void StopCarry(IEntity gadget = null)
	{
		CharacterControllerComponent characterController = LF_Component<CharacterControllerComponent>.Find(GetOwner());
		if (!characterController)
			LF_Debug.Error("HandCarry", "stop carry: owner has no CharacterControllerComponent; dropping hand items only");
		else if (m_eState != LF_EHandCarryState.AWAIT_SWAP)
		{
			// Only remove gadget if there not a new one scheduled. Otherwise the new gadget will be removed instead "too".
			characterController.RemoveGadgetFromHand(true);
		}
		ClearHandStorage(); // Drop all hand carry items (it should normally just be the one we currently carry)
		if (gadget) LF_InventoryUtils.DropItem(GetOwner(), gadget); // Drop current gadget from different storage it might has been moved to
		m_eState = LF_EHandCarryState.NONE;
	}

	//------------------------------------------------------------------------------------------------
	protected void ClearHandStorage(IEntity keepItem = null)
	{
		InventoryStorageManagerComponent inventoryManager = LF_Component<InventoryStorageManagerComponent>.Find(GetOwner());
		if (!inventoryManager)
		{
			LF_Debug.Error("HandCarry", "clear hand storage skipped: owner has no InventoryStorageManagerComponent");
			return;
		}

		array<IEntity> outItems();
		GetAll(outItems);
		foreach (IEntity handItem : outItems)
		{
			if (handItem != keepItem) inventoryManager.TryRemoveItemFromStorage(handItem, this);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected bool HasLocalControl()
	{
		PlayerController playerController = GetGame().GetPlayerController();
		return playerController && GetOwner() == playerController.GetControlledEntity();
	}
}
