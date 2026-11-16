//------------------------------------------------------------------------------------------------
//! A hand tool: an item the player takes into the hands from the 1-0 quick slots, held like the
//! vanilla repair wrench, whose use actions only show while it is held (the vault drill; the courier
//! parcel reuses it). It is the vanilla gadget path: an item with an SCR_GadgetComponent is
//! quick-slottable, SCR_CharacterInventoryStorageComponent.UseItem sends it to the hands through
//! SCR_GadgetManagerComponent.SetGadgetMode(IN_HAND), and the hold pose is the prefab's
//! ItemAnimationAttributes (the wrench's RepairKit graph). The prefab sets the tool id.
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Core", description: "Hand tool: held from the quick slots; use actions read its tool id while it is held.")]
class LF_HandToolComponentClass : SCR_GadgetComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_HandToolComponent : SCR_GadgetComponent
{
	[Attribute("", UIWidgets.EditBox, desc: "Tool id the use actions look for (drill, parcel). Empty: the prefab file name.", category: "Hand tool")]
	protected string m_sToolId;

	[Attribute("0", UIWidgets.CheckBox, desc: "Take the tool into the hands once, when it first enters its carrier's inventory (a courier parcel fills the hands; a drill waits for its quick slot).", category: "Hand tool")]
	protected bool m_bEquipOnPickup;

	protected bool m_bPickupEquipped;

	//------------------------------------------------------------------------------------------------
	string GetToolId()
	{
		if (!m_sToolId.IsEmpty())
			return m_sToolId;

		return FilePath.StripExtension(FilePath.StripPath(LF_Utils.GetPrefabName(GetOwner())));
	}

	//------------------------------------------------------------------------------------------------
	//! A tool, like the vanilla wrench: it gets its own quick slot type and is never raised.
	override EGadgetType GetType()
	{
		return EGadgetType.SPECIALIST_ITEM;
	}

	//------------------------------------------------------------------------------------------------
	override void OnModeChanged(EGadgetMode mode, IEntity charOwner)
	{
		super.OnModeChanged(mode, charOwner);
		// Vanilla auto-slots only types with a default quick slot; a hand tool takes the first free
		// tool slot on the owner's machine, where the 1-0 bar lives.
		if (mode == EGadgetMode.IN_STORAGE && charOwner && charOwner == SCR_PlayerController.GetLocalControlledEntity())
		{
			LF_HandTool.QuickSlot(charOwner, GetOwner());
			// Only the first time: a tool the player put away stays away.
			if (m_bEquipOnPickup && !m_bPickupEquipped)
			{
				m_bPickupEquipped = true;
				GetGame().GetCallqueue().CallLater(EquipOnPickup, 250, false, charOwner);
			}
		}

		if (mode == EGadgetMode.IN_HAND)
			LF_Debug.Log(LF_HandTool.FEATURE, string.Format("%1 in hand of %2", GetToolId(), LF_Utils.GetPlayerUid(charOwner)));
	}

	//------------------------------------------------------------------------------------------------
	//! Local machine, a moment after the pickup: the engine finishes taking the item into the
	//! inventory first. A tool that left the carrier meanwhile stays where it is.
	protected void EquipOnPickup(IEntity character)
	{
		if (!character || LF_HandTool.Carried(character, GetToolId()) != GetOwner())
			return;

		LF_HandTool.Equip(character, GetOwner());
	}

	//------------------------------------------------------------------------------------------------
	bool EquipsOnPickup()
	{
		return m_bEquipOnPickup;
	}

	//------------------------------------------------------------------------------------------------
	//! A tool deleted within the pickup delay (a delivered parcel) must not leave the call queued.
	void ~LF_HandToolComponent()
	{
		if (m_bPickupEquipped && GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(EquipOnPickup);
	}
}

//------------------------------------------------------------------------------------------------
//! Reads and sets which hand tool a character holds. Server and client; reads only replicated
//! gadget state (the gadget mode is the component's RplSave).
class LF_HandTool
{
	static const string FEATURE = "HandTool";

	//------------------------------------------------------------------------------------------------
	static LF_HandToolComponent Of(IEntity item)
	{
		if (!item)
			return null;

		return LF_HandToolComponent.Cast(item.FindComponent(LF_HandToolComponent));
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: does a tool with id `heldId` in mode `mode` count as holding `wanted`?
	static bool Matches(string heldId, EGadgetMode mode, string wanted)
	{
		return mode == EGadgetMode.IN_HAND && !heldId.IsEmpty() && heldId == wanted;
	}

	//------------------------------------------------------------------------------------------------
	//! The hand tool with this id the character holds in the hands, or null. A tool in the pack,
	//! a pocket or a quick slot does not count.
	static IEntity HeldTool(IEntity character, string toolId)
	{
		if (!character || toolId.IsEmpty())
			return null;

		SCR_GadgetManagerComponent gadgets = SCR_GadgetManagerComponent.Cast(character.FindComponent(SCR_GadgetManagerComponent));
		if (gadgets)
		{
			IEntity held = gadgets.GetHeldGadget();
			LF_HandToolComponent heldTool = Of(held);
			if (heldTool && Matches(heldTool.GetToolId(), heldTool.GetMode(), toolId))
				return held;
		}

		// The manager learns of the hand at the end of the take animation; the gadget's own mode is
		// what replicates, so read it too.
		InventoryStorageManagerComponent inventory = LF_InventoryUtils.GetResponsibleStorageManager(character);
		if (!inventory)
			return null;

		array<IEntity> items = {};
		inventory.GetItems(items);
		foreach (IEntity item : items)
		{
			LF_HandToolComponent tool = Of(item);
			if (tool && Matches(tool.GetToolId(), tool.GetMode(), toolId))
				return item;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	static bool Holds(IEntity character, string toolId)
	{
		return HeldTool(character, toolId) != null;
	}

	//------------------------------------------------------------------------------------------------
	//! The first carried hand tool with this id, held or not.
	static IEntity Carried(IEntity character, string toolId)
	{
		if (!character || toolId.IsEmpty())
			return null;

		InventoryStorageManagerComponent inventory = LF_InventoryUtils.GetResponsibleStorageManager(character);
		if (!inventory)
			return null;

		array<IEntity> items = {};
		inventory.GetItems(items);
		foreach (IEntity item : items)
		{
			LF_HandToolComponent tool = Of(item);
			if (tool && tool.GetToolId() == toolId)
				return item;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Takes a carried hand tool into the hands, as pressing its quick slot does.
	static bool Equip(IEntity character, IEntity item)
	{
		LF_HandToolComponent tool = Of(item);
		if (!character || !tool)
		{
			LF_Debug.Error(FEATURE, "equip refused: no character or the item is not a hand tool");
			return false;
		}

		SCR_GadgetManagerComponent gadgets = SCR_GadgetManagerComponent.Cast(character.FindComponent(SCR_GadgetManagerComponent));
		if (!gadgets)
		{
			LF_Debug.Error(FEATURE, "equip refused: the character has no SCR_GadgetManagerComponent");
			return false;
		}

		if (gadgets.GetHeldGadget() && gadgets.GetHeldGadget() != item)
			gadgets.RemoveHeldGadget();

		gadgets.SetGadgetMode(item, EGadgetMode.IN_HAND);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Puts a carried hand tool in the first free 1-0 quick slot. \return the slot index, or -1.
	static int QuickSlot(IEntity character, IEntity item)
	{
		if (!character || !Of(item))
			return -1;

		SCR_CharacterInventoryStorageComponent storage = SCR_CharacterInventoryStorageComponent.Cast(character.FindComponent(SCR_CharacterInventoryStorageComponent));
		if (!storage)
			return -1;

		int slot = storage.StoreItemToQuickSlot(item);
		if (slot >= 0)
			return slot;

		// No default slot for the tool type: the first free slot after the weapon slots.
		for (int i = SCR_InventoryMenuUI.WEAPON_SLOTS_COUNT; i < 10; i++)
		{
			if (!storage.GetContainerFromQuickslot(i))
				return storage.StoreItemToQuickSlot(item, i);
		}

		return -1;
	}
}
